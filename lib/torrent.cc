#include <sys/types.h>
#include <algorithm>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <pthread.h>
#include <stdlib.h>
#include <sstream>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include "exceptions.h"
#include "file.h"
#include "hasher.h"
#include "http.h"
#include "macros.h"
#include "overseer.h"
#include "peer.h"
#include "pendingpeer.h"
#include "sha1.h"
#include "tracer.h"
#include "torrent.h"

using namespace std;

#define TRACER (overseer->getTracer())

Torrent::Torrent(Overseer* o, Metadata* md)
{
	overseer = o; downloaded = 0; uploaded = 0; left = 0;
	terminating = false; complete = false;
	lastChokingAlgorithm = 0; unchokingRound = 0;
	optimisticUnchokedPeer = NULL; tracker_key = "";
	name = ""; endgame_mode = false;
	rx_rate = 0; tx_rate = 0;

	/* force the thread to contact the tracker */
	tracker_interval = 0; tracker_min_interval = 0;
	lastTrackerContact = 0;

	INIT_RWLOCK(peers);
	INIT_RWLOCK(files);
	INIT_MUTEX(data);
	INIT_MUTEX(log);

	/*
	 * Ensure the 'info' and 'announce' metadata fields exist; we can't do much
	 * without them.
	 */
	MetaDictionary* dictionary = md->getDictionary();
	MetaDictionary* info = dynamic_cast<MetaDictionary*>((*dictionary)["info"]);
	if (info == NULL)
		throw TorrentException("metadata doesn't contain an info dictionary");
	MetaString* msAnnounce = dynamic_cast<MetaString*>((*dictionary)["announce"]);
	if (msAnnounce == NULL)
		throw TorrentException("metadata doesn't contain an announce URL");
	announceURL = msAnnounce->getString();

	MetaInteger* miPieceLength = dynamic_cast<MetaInteger*>((*info)["piece length"]);
	MetaString* miPieces = dynamic_cast<MetaString*>((*info)["pieces"]);

	if (miPieceLength == NULL)
		throw TorrentException("metadata doesn't contain piece length");
	if (miPieces == NULL)
		throw TorrentException("metadata doesn't contain pieces");
	if (miPieces->getString().size() % TORRENT_HASH_LEN != 0)
		throw TorrentException("metadata pieces content isn't a multiple of hash length");

	/* Construct the hash of pieces, to ease individual access */
	pieceLen = miPieceLength->getInteger();
	numPieces = miPieces->getString().size() / TORRENT_HASH_LEN;
	pieceHash = new uint8_t[numPieces * TORRENT_HASH_LEN];
	memcpy(pieceHash, miPieces->getString().c_str(), numPieces * TORRENT_HASH_LEN);

	/* For now, assume we have no pieces, requested none and are hashing none */
	havePiece.reserve(numPieces);
	hashingPiece.reserve(numPieces);
	pieceCardinality.reserve(numPieces);
	for (unsigned int i = 0; i < numPieces; i++) {
		havePiece.push_back(false);
		hashingPiece.push_back(false);
		pieceCardinality.push_back(0);
	}

	/*
	 * Construct the chunk overview. XXX ideally, TORRENT_CHUNK_SIZE should be
	 * variable and adjusted.
	 */
	if (pieceLen % TORRENT_CHUNK_SIZE != 0)
		throw TorrentException("torrent piece length is not a multiple of chunk size!");
	int numChunks = pieceLen;
	haveChunk.reserve(numChunks);
	haveRequestedChunk.reserve(numChunks);
	for (unsigned int i = 0; i < numPieces; i++) {
		for (unsigned int j = 0; j < pieceLen / TORRENT_CHUNK_SIZE; j++) {
			haveChunk.push_back(havePiece[i]);
			haveRequestedChunk.push_back(PeerList());
		}
	}

	/*
	 * Construct the list of files. There are two possibilities:
	 * 1) The torrent consists of only a single file
	 *    info['name'] and info['length'] refer to this file
	 * 2) The torrent has more than one file
	 *    info['name'] refers to the directory where the files must be
	 *    places.
	 *   
	 *
	 * In case (2), there is a 'files' list, which houses the
	 * dictionaries containing 'length' and 'path' information.
	 */
	MetaString* msName = dynamic_cast<MetaString*>((*info)["name"]);
	if (msName == NULL)
		throw TorrentException("info dictionary doesn't contain a name!");
	name = msName->getString();

	/* XXX check name for badness */
	total_size = 0;
	MetaList* mlFiles = dynamic_cast<MetaList*>((*info)["files"]);
	if (mlFiles != NULL) {
		for (list<MetaField*>::iterator it = mlFiles->getList().begin();
			   it != mlFiles->getList().end(); it++) {
				MetaDictionary* md = dynamic_cast<MetaDictionary*>(*it);
				if (md == NULL)
					throw TorrentException("files list doesn't contain dictionaries");

				/* Fetch the file length and path */
				MetaInteger* miLength = dynamic_cast<MetaInteger*>((*md)["length"]);
				MetaList* mlPath = dynamic_cast<MetaList*>((*md)["path"]);
				if (miLength == NULL)
					throw TorrentException("file dictionary doesn't contain a length");
				if (mlPath == NULL)
					throw TorrentException("file dictionary doesn't contain a path");

				/* Construct the full path of the torrent file */
				string fullPath = msName->getString();
				for (list<MetaField*>::iterator itt = mlPath->getList().begin();
						 itt != mlPath->getList().end(); itt++) {

					/* If the path doesn't exist, create it */
					struct stat fs;
					if (stat(fullPath.c_str(), &fs) < 0)
						mkdir(fullPath.c_str(), 0755);

					MetaString* ms = dynamic_cast<MetaString*>(*itt);
					if (ms == NULL)
						throw TorrentException("file path list doesn't contain strings");
					/* XXX check string for badness */
					fullPath += "/" + ms->getString();
				}

				File* f = new File(fullPath, miLength->getInteger());
				files.push_back(f);
				overseer->addFile(f);
				total_size += miLength->getInteger();
			}
	} else {
		/* There is only a single file in this torrent - all too easy */
		MetaInteger* miLength = dynamic_cast<MetaInteger*>((*info)["length"]);
		if (miLength == NULL)
			throw TorrentException("info dictionary doesn't contain a length");

		File* f = new File(msName->getString(), miLength->getInteger());
		files.push_back(f);
		overseer->addFile(f);
		total_size += miLength->getInteger();
	}

	/* Ensure the total size is covered by the pieces in the torrent */
	if ((total_size + (pieceLen - 1)) / pieceLen != numPieces)
		throw TorrentException("sum of file lengths doesn't agree with number of pieces");

	/*
	 * Start out by assuming we have to download everything; the hashing
	 * callback will adjust this number as needed. We need to set this
	 * value here, as a successful piece will decrement this
	 * number.
	 */
	left = total_size;

	/* Stream the info-part of the torrent to a buffer... */
	stringbuf sb;
	ostream os(&sb);
	os << *info;

	/* ...and SHA1 that so we get our info hash */
	HashSHA1 sha1;
	sha1.process(sb.str().c_str(), sb.str().size());
 	memcpy(infoHash, sha1.getHash(), sizeof(infoHash));

	/*
	 * If one or more files were pre-existing (this means they existed and
	 * have the correct length), we already have the pieces. Since we have
	 * no away of knowing whether the full file was retrieved or just a
	 * portion, hash away!
	 *
	 * XXX we should be able to dump / restore such state information
	 */
	unsigned int piecenum = 0;
	unsigned int leftoverLength = 0;
	numPiecesHashing = 0;
	bool previousFileReopened = false /* quench warning, can't be used */;
	for (unsigned int i = 0; i < files.size(); i++) {
		File* f = files[i];
		size_t fileLength = f->getLength();

		if (leftoverLength > 0) {
			/*
		 	 * The previous file ended at a non-piece barrier. This means we
			 * have to use the previous file information to figure out whether
		 	 * this piece is completed.
			 */
			if (leftoverLength + fileLength < pieceLen) {
				/* This file won't complete the piece */
				leftoverLength += fileLength;
				previousFileReopened = previousFileReopened && f->haveReopened();
				continue;
			}

			/* This file is big enough to process the previous missing pieces */
			havePiece[piecenum] = previousFileReopened && f->haveReopened();
			if (havePiece[piecenum]) {
				scheduleHashing(piecenum, true);
			}
			piecenum++;

			/* No longer leftover, but subtract the data of this file we used */
			fileLength -= pieceLen - leftoverLength;
			leftoverLength = 0;
		}

		/*
		 * We are at a piece length barrier now, so we can just handle the
		 * full pieces in order here.
		 */
		while (fileLength > pieceLen) {
			havePiece[piecenum] = f->haveReopened();
			if (f->haveReopened())
				scheduleHashing(piecenum, true);
			piecenum++;
			fileLength -= pieceLen;
		}

		/* If the file ends at a non-piece barrier, continue in the next run */
		leftoverLength += fileLength;
		previousFileReopened = f->haveReopened();
	}

	/* If the final file has leftover pieces, add an extra full piece to cope */
	if (leftoverLength > 0) {
		havePiece[piecenum] = previousFileReopened;
		if (havePiece[piecenum])
			scheduleHashing(piecenum, true);
		piecenum++;
	}

	/*
	 * We must have processed as many pieces as there are in the file.
	 */
	assert(piecenum == numPieces);
}

Torrent::~Torrent()
{
	overseer->cleanupTorrent(this);

	/*
	 * Inform the tracker that we are going away. We care not
	 * about any errors; we are leaving anyway.
	 */
	try {
		Metadata* md = contactTracker("stopped");
		delete md;
	} catch (TortillaException e) {
		/* ... */
	}

	/*
	 * Remove our peers; actual cleanup will be performed by the peer manager.
	 */
	WLOCK(peers);
	while (true) {
		vector<Peer*>::iterator it = peers.begin();
		if (it == peers.end())
			break;
		Peer* p = *it;
		peers.erase(it);
	}
	RWUNLOCK(peers);

	/* Close all files, too */
	WLOCK(files);
	while (true) {
		vector<File*>::iterator it = files.begin();
		if (it == files.end())
			break;
		File* f = *it;
		files.erase(it);
		overseer->removeFile(f);
		delete f;
	}
	RWUNLOCK(files);

	/* Delete pending peers and piece hashes */
	while (!pendingPeers.empty()) {
		PendingPeer* pp = pendingPeers.front();
		pendingPeers.pop_front();
		delete pp;
	}
	delete[] pieceHash;

	DESTROY_MUTEX(log);
	DESTROY_MUTEX(data);
	DESTROY_RWLOCK(peers);
	DESTROY_RWLOCK(files);
}

Metadata*
Torrent::contactTracker(std::string event)
{
	map<string, string> m;

	/* Construct the tracker request, and off it goes */
	string h((const char*)infoHash, sizeof(infoHash));
	string peerID((const char*)overseer->getPeerID(), TORRENT_PEERID_LEN);
	LOCK(data);
	m["info_hash"] = h;
	m["peer_id"] = peerID;
	if (event != "")
		m["event"] = event;
	m["downloaded"] = convertInteger(downloaded);
	m["uploaded"] = convertInteger(uploaded);
	m["left"] = convertInteger(left);
	m["port"] = convertInteger(overseer->getListeningPort());
	if (tracker_key != "")
		m["key"] = tracker_key;
	m["compact"] = "1";
	UNLOCK(data);
	/* If we are a seeder, we care not about any new peers XXX small race here */
	if (complete) {
		m["numwant"] = convertInteger(0);
	} else {
		/*
		 * Otherwise, try to grab as many peers as we need to fill our list. Note
		 * that we ask for twice as much peers as we need, since we expect that
		 * about half of the peers will fail.
		 */
		uint32_t numPeers = getNumPeers();
		m["numwant"] = convertInteger((TORRENT_DESIRED_PEERS - numPeers) * 2);
	}
	lastTrackerContact = time(NULL);
	string result;
	try {
		result = HTTP::get(announceURL, m);
	} catch (HTTPException e) {
		log(NULL, "unable to contact tracker: %s", e.what());
		return NULL;
	}

	/* Parse the result as metadata (which it should be) */
	Metadata* md;
	try {
		stringbuf sb(result);
		istream is(&sb);
		md = new Metadata(is);
	} catch (MetadataException e) {
		log(NULL, "tracker returned garbage: %s", result.c_str());
		return NULL;
	}

	/*
	 * If we got here, the result is valid metadata. However, a failure may have
	 * been reported.
	 */
	MetaString* ms = dynamic_cast<MetaString*>((*md->getDictionary())["failure reason"]);
	if (ms != NULL) {
		string failure = ms->getString();
		delete md; /* Prevent memory leak */
		log(NULL, "tracker reported failure: %s", failure.c_str());
		return NULL;
	}

	return md;
}

void
Torrent::handleTracker(string event)
{
	Metadata* md = contactTracker(event);
	if (md == NULL)
		return;

	/*
	 * Fetch the tracker interval times. The maximum interval must be present.
	 */
	MetaInteger* msInterval = dynamic_cast<MetaInteger*>((*md->getDictionary())["interval"]);
	if (msInterval == NULL)
		log(NULL, "tracker didn't report interval, ignoring reply");
	tracker_interval = msInterval->getInteger();
	MetaInteger* msMinInterval = dynamic_cast<MetaInteger*>((*md->getDictionary())["min interval"]);
	if (msMinInterval != NULL)
		tracker_min_interval = msMinInterval->getInteger();
	else
		tracker_min_interval = 0;
	MetaString* msKey = dynamic_cast<MetaString*>((*md->getDictionary())["key"]);
	if (msKey != NULL)
		tracker_key = msKey->getString();

	MetaList* peerslist = dynamic_cast<MetaList*>((*md->getDictionary())["peers"]);
	TRACE(TRACKER, "contacted tracker: torrent=%p, interval=%u, min_interval=%u, key='%s',peers=%u",
	 this, tracker_interval, tracker_min_interval, tracker_key.c_str(),
	 peerslist != NULL ? peerslist->getList().size() : 0);
	if (peerslist != NULL && !complete) {
		/*
		 * The tracker has provided us with (possibly new) peers. Add them to the
		 * list if applicable - note that we only do this if we aren't seeding!
		 */
		for (list<MetaField*>::iterator it = peerslist->getList().begin();
		    it != peerslist->getList().end(); it++) {
			MetaDictionary* dict = dynamic_cast<MetaDictionary*>(*it);
			if (dict == NULL)
				continue;

			/* Grab all fields we really need; if any isn't available, ignore the entry */
			MetaString*  msHost   = dynamic_cast<MetaString*>((*dict)["ip"]);
			MetaString*  msPeerID = dynamic_cast<MetaString*>((*dict)["peer id"]);
			MetaInteger* msPort   = dynamic_cast<MetaInteger*>((*dict)["port"]);
			if (msHost == NULL || msPeerID == NULL || msPort == NULL)
				continue;

#if 0
			/* If we already know this peer ID, ignore it */
			if (peers.find(msPeerID->getString()) != peers.end())
				continue;
#endif

			/* Don't connect with ourselves *shrug* */
			if (!memcmp((const char*)overseer->getPeerID(), msPeerID->getString().c_str(), TORRENT_PEERID_LEN))
				continue;

			LOCK(data);
			pendingPeers.push_back(new PendingPeer(this, msHost->getString(), msPort->getInteger(), msPeerID->getString()));
			UNLOCK(data);
		}
	}

	MetaString* peerstring = dynamic_cast<MetaString*>((*md->getDictionary())["peers"]);
	if (peerstring != NULL && peerstring->getString().size() % TORRENT_COMPACTPEER_SIZE == 0) {
		/* We got a compact peer list! */
		TRACE(TRACKER, "contacted tracker: torrent=%p, compact peers=%u",
		 this, peerstring->getString().size() / TORRENT_COMPACTPEER_SIZE);
		for (unsigned int i = 0; i < peerstring->getString().size() / TORRENT_COMPACTPEER_SIZE; i++) {
			char ip[32];
			uint16_t port;

			/* XXX this makes eyes bleed */
			const uint8_t* ptr = (const uint8_t*)(peerstring->getString().c_str() + i * TORRENT_COMPACTPEER_SIZE);
			snprintf(ip,   sizeof(ip),   "%u.%u.%u.%u",
			 (uint8_t)ptr[0], (uint8_t)ptr[1],
			 (uint8_t)ptr[2], (uint8_t)ptr[3]);
			port = (uint16_t)(ptr[4] << 8) | ptr[5];

			LOCK(data);
			pendingPeers.push_back(new PendingPeer(this, string(ip), port, ""));
			UNLOCK(data);
		}
	}

	delete md;
}

#if 0
void
Torrent::go()
{
	while (!terminating) {
		fd_set readfds, writefds;

		/*
	 	 * Gracefully handle any peers that are going away.
		  */
		WLOCK(peers);
		vector<Peer*>::iterator peerit = peers.begin();
		while (peerit != peers.end()) {
			Peer* p = *peerit;
			if (!p->isShuttingDown()) {
				peerit++;
				continue;
			}

			peers.erase(peerit);
			callbackPeerGone(p);
			delete p;

			peerit = peers.begin();
		}
		RWUNLOCK(peers);

		/* If we can fill up our peer slots, try it */
		while (true) {
			unsigned int numPeers = getNumPeers();
			if (numPeers >= TORRENT_DESIRED_PEERS)
				break;

			LOCK(data);
			if (pendingPeers.empty()) {
				UNLOCK(data);
				break;
			}
			PendingPeer* pp = pendingPeers.front();
			pendingPeers.pop_front();
			UNLOCK(data);

			Peer* p = pp->connect();
			delete pp;

			if (p != NULL) {
				WLOCK(peers);
				peers.push_back(p);
				RWUNLOCK(peers);

				/*
				 * Send the handshake; we can only do this after we have added the
				 * peer since the Sender won't know of it otherwise.
				 */
				p->sendHandshake();
				overseer->callbackPeerAdded(p);
			}
		}

		/* Construct our file descriptor set */
		int maxfd = -1;
		FD_ZERO(&readfds); FD_ZERO(&writefds);
		RLOCK(peers);
		for (vector<Peer*>::iterator it = peers.begin();
		     it != peers.end(); it++) {
			Peer* p = *it;
			int fd = p->getFD();
			if (maxfd < fd) maxfd = fd;
			if (p->areConnecting())
				FD_SET(fd, &writefds);
			FD_SET(fd, &readfds);
		}
		RWUNLOCK(peers);

		/*
		 *
		 * Note that, for busy torrents, this 0.5 second loop will never be reached.
		 */
		struct timeval tv;
		tv.tv_sec = 0; tv.tv_usec = 5000;
		int n = select(maxfd + 1, &readfds, &writefds, (fd_set*)NULL, &tv);
		if (n == 0)
			continue;

		/* If we are terminating, we don't care about any data as we're leaving */
		if (terminating)
			continue;

		/*
		 * Wade through all peers, handle any data to service.
		 */
		RLOCK(peers);
		for (vector<Peer*>::iterator it = peers.begin();
				 it != peers.end(); it++) {
			Peer* p = (*it);
			int fd = (*it)->getFD();
			if (FD_ISSET(fd, &writefds)) {
				/* Handle with made connections */
				if (p->areConnecting())
					p->connectionDone();
			}
			if (!FD_ISSET(fd, &readfds))
				continue;
			/*
			 * There is data here.
			 */
			uint8_t buf[65536 /* XXX */];
			ssize_t len = ::recv(fd, buf, sizeof(buf), MSG_DONTWAIT);
			if (len <= 0) {
				/* socket lost */
				TRACE(TORRENT, "connection to peer=%s lost, socket closed, errno=%u, len=%ld", p->getID().c_str(), errno, len);
				p->shutdown();
				continue;
			}

			/* Hand the data off to the application */
			if (p->receive(buf, len) == true) {
				/* Need to sever the connection */
				TRACE(TORRENT, "severing connection to peer=%s", p->getID().c_str());
				p->shutdown();
				continue;
			}
		}
		RWUNLOCK(peers);
	}
}
#endif

void
Torrent::callbackPiecesAdded(Peer* p, vector<unsigned int>& pieces)
{
	LOCK(data);
	for (vector<unsigned int>::iterator it = pieces.begin();
	     it != pieces.end(); it++) {
		assert(*it < numPieces);
		pieceCardinality[*it]++;
	}
	UNLOCK(data);

	/* Use this to signal interest in a peer */
	processPeerStatus();
}

void
Torrent::callbackPiecesRemoved(Peer* p, vector<unsigned int>& pieces)
{
	LOCK(data);
	for (vector<unsigned int>::iterator it = pieces.begin();
	     it != pieces.end(); it++) {
		assert(*it < numPieces);
		assert(pieceCardinality[*it] > 0);
		pieceCardinality[*it]--;
	}
	UNLOCK(data);
}

void
Torrent::schedulePeerRequests(Peer* p)
{
	/*
	 * If we have the full torrent already, the peer is choking us,
	 * or the peer is going away, * don't bother trying to schedule something.
	 */
	if (complete || p->isChoking() || p->isShuttingDown())
		return;

	/*
	 * XXX this algorithm should schedule a piece more randomly
	 */
	for (unsigned int i = 0; i < numPieces && !terminating; i++) {
		LOCK(data);
		bool b = havePiece[i] || !p->hasPiece(i);
		UNLOCK(data);
		if (b)
			continue;

		/*
		 * We don't have the piece, but this peer does. Find a piece, and go
		 * request.
		 */
		assert(p->isInterested());

		int result = p->sendPieceRequest(i);
		if (result > 0) {
			break;
		}
	}
}

std::string
Torrent::convertInteger(uint64_t i)
{
	ostringstream o;
	o << i;
	return o.str();
}

int
Torrent::getMissingChunk(Peer* p, unsigned int piece)
{
	assert (piece < numPieces);

	LOCK(data);
	for (unsigned int j = 0; j < calculateChunksInPiece(piece); j++) {
		unsigned int chunkIndex = (piece * (pieceLen / TORRENT_CHUNK_SIZE)) + j;
		/* If we already have this chunk, skip over it */
		if (haveChunk[chunkIndex])
			continue;

		/* If we aren't doing endgame mode, don't request the chunk from >1 peer */
		if (!endgame_mode && haveRequestedChunk[chunkIndex].size() > 0)
			continue;

		/* Only request the chunk if we haven't already done so from this peer */
		if (find(haveRequestedChunk[chunkIndex].begin(),
		         haveRequestedChunk[chunkIndex].end(),
		         p) == haveRequestedChunk[chunkIndex].end()) {
		  haveRequestedChunk[chunkIndex].push_back(p);
			UNLOCK(data);
			return j;
		}
	}
	UNLOCK(data);

	return -1;
}

void
Torrent::callbackCompletePiece(Peer* p, unsigned int piece)
{
	assert(piece < numPieces);
	LOCK(data);
	assert(!havePiece[piece]);
	havePiece[piece] = true;
	UNLOCK(data);

	/*
	 * Ask the hasher to verify this chunk - using the callback, we figure out if
	 * we have to refetch the piece or accept that we think it's fine.
	 */
	scheduleHashing(piece);
}

unsigned int
Torrent::calculateChunksInPiece(unsigned int piece)
{
	assert(piece < numPieces);

	if (piece < numPieces - 1)
		return pieceLen / TORRENT_CHUNK_SIZE;

	unsigned int chunks = (total_size % pieceLen) / TORRENT_CHUNK_SIZE;
	if (total_size % TORRENT_CHUNK_SIZE)
		chunks++;
	return chunks;
}

void
Torrent::callbackCompleteChunk(Peer* p, unsigned int piece, uint32_t offset, const uint8_t* data, uint32_t len)
{
	assert (piece < numPieces);
	assert (len <= TORRENT_CHUNK_SIZE);
	assert (offset % TORRENT_CHUNK_SIZE == 0);

	LOCK(data);
	if (havePiece[piece]) {
		/*
		 * This can happen in endgame mode; if we have requested a piece but
		 * couldn't cancel it anymore (or if we are too late), we may get the
		 * last data while we are hashing. If this happens, just ignore the
		 * data alltogether.
		 */
		UNLOCK(data);
		schedulePeerRequests(p);
		return;
	}

	/*
	 * Immediately mark the chunk as completed; this prevents anyone else from
	 * scheduling it.
	 */
	haveChunk[(piece * (pieceLen / TORRENT_CHUNK_SIZE)) + offset / TORRENT_CHUNK_SIZE] = true;
	UNLOCK(data);

	if (!writeChunk(piece, offset, data, len)) {
		TRACE(TORRENT, "unable to write chunk, piece=%lu, offset=%lu, len=%lu", piece, offset, len);
		LOCK(data);
		haveChunk[(piece * (pieceLen / TORRENT_CHUNK_SIZE)) + offset / TORRENT_CHUNK_SIZE] = false;
		UNLOCK(data);
	}

	/*
	 * If anyone else is downloading this chunk, cancel it. Same goes for any
	 * REQUEST messages we may have queued but not sent.
	 */
	RLOCK(peers);
	for (vector<Peer*>::iterator it = peers.begin();
	     it != peers.end(); it++) {
		Peer* p = (*it);
		p->cancelChunk(piece, offset, len);
		p->cancelChunkRequest(piece, offset, len);
	}
	RWUNLOCK(peers);

	LOCK(data);
	downloaded += len;

	/* See if we have all chunks; if so, the piece is in */
	bool full = true;
	for (unsigned int i = 0; i < calculateChunksInPiece(piece); i++)
		if (!haveChunk[(piece * (pieceLen / TORRENT_CHUNK_SIZE)) + i]) {
			full = false;
			break;
		}
	UNLOCK(data);

	schedulePeerRequests(p);
	if (!full)
		return;

	/* Yay! */
	TRACE(TORRENT, "piece completed: piece=%u", piece);
	callbackCompletePiece(p, piece);
}

bool
Torrent::hasPiece(unsigned int piece)
{
	assert (piece < numPieces);

	LOCK(data);
	bool b = havePiece[piece];
	UNLOCK(data);

	return b;
}

void
Torrent::callbackCompleteHashing(unsigned int piece, bool result)
{
	LOCK(data);
	if (numPiecesHashing > 0)
		numPiecesHashing--;

	hashingPiece[piece] = false;
	if (!result) {
		/*
		 * We got a corrupted piece! Mark it as not-available; we'll automatically
		 * reschedule this piece again later.
		 *
		 * XXX we should identify and ban seeders that provide us with bad content.
		 */
		havePiece[piece] = false;
		UNLOCK(data);
		return;
	}

	/* At least someone has this piece... we! */
	pieceCardinality[piece]++;
	if (piece == numPieces - 1) {
		left -= getTotalSize() % pieceLen > 0 ?
		        getTotalSize() % pieceLen : pieceLen;
	} else {
		left -= pieceLen;
	}

	/*
	 * Enter endgame mode if needed.
	 */
	if (!endgame_mode && ((total_size - left) / (float)total_size) * 100.0f >= TORRENT_ENDGAME_PERCENTAGE) {
		endgame_mode = true;
		TRACE(TORRENT, "endgame mode: torrent=%p", this);
	}

	/*
	 * Inform our peers that we have this piece. We let go of the
	 * data lock, since we don't care if data changes here.
	 */
	UNLOCK(data);
	RLOCK(peers);
	for (vector<Peer*>::iterator it = peers.begin();
	     it != peers.end(); it++) {
		Peer* p = (*it);
		p->have(piece);
	}
	RWUNLOCK(peers);

	/* Try to use this as an attempt to signal disinterest in peers */
	processPeerStatus();

	/* If we have all pieces, rejoice */
	LOCK(data);
	for (unsigned int i = 0; i < numPieces; i++)
		if (!havePiece[i] || hashingPiece[i]) {
			UNLOCK(data);
			return;
		}
	UNLOCK(data);

	/* Yeah! */
	complete = true;
	TRACE(TORRENT, "torrent completed: torrent=%p", this);
	callbackCompleteTorrent();
}

bool
Torrent::handleChunk(unsigned int piece, unsigned int offset, uint8_t* buf, size_t length, bool writing)
{
	assert(piece < numPieces);
	assert(length <= TORRENT_CHUNK_SIZE);

	/* Calculate the absolute position */
	off_t absolutePos = (off_t)piece * (off_t)pieceLen + (off_t)offset;

	/* Locate the first file matching this position */
	unsigned int idx = 0;
	File* f = NULL;
	RLOCK(files);
	while (idx < files.size()) {
		if (absolutePos < files[idx]->getLength()) {
			/* At least a part of the offset to handle resides in this file */
			f = files[idx];
			break;
		}
		absolutePos -= files[idx]->getLength();
		idx++;
	}
	if (f == NULL) {
		/*
		 * Invalid offset was presented - this should only happen if the
		 * torrent is terminating.
		 */
		RWUNLOCK(files);
		assert(terminating);
		return false;
	}

	/*
	 * Chunks are allowed to span between multiple files, so we keep on writing
	 * stuff until we run out of stuff to write.
	 */
	while (length > 0) {
		uint32_t partlen = MIN(f->getLength() - absolutePos, length);
    
		if (writing)
			overseer->writeFile(f, absolutePos, buf, partlen);
		else
			overseer->readFile(f, absolutePos, buf, partlen);

		if (partlen != length) {
			/* This operation spans multiple files, so use the next one */
			idx++; assert(idx < files.size());
			f = files[idx];
			absolutePos = 0;
		} else {
			absolutePos += partlen;
		}
		buf += partlen; length -= partlen;
	}
	RWUNLOCK(files);
	return true;
}

bool
Torrent::writeChunk(unsigned int piece, unsigned int offset, const uint8_t* buf, size_t length)
{
	return handleChunk(piece, offset, (uint8_t*)buf, length, true);
}

bool
Torrent::readChunk(unsigned int piece, unsigned int offset, uint8_t* buf, size_t length)
{
	return handleChunk(piece, offset, (uint8_t*)buf, length, false);
}

const uint8_t*
Torrent::getPieceHash(unsigned int piece)
{
	assert(piece < numPieces);
	return (pieceHash + (piece * TORRENT_HASH_LEN));
}

void
Torrent::callbackCompleteTorrent()
{
	complete = true;

	/* Kick anyone who is also a seeder */
	processPeerStatus();
}

void
Torrent::scheduleHashing(unsigned int piece, bool registerHashing)
{
	assert(piece < numPieces);

	/*
	 * Mark the piece as being hashed before we actually add it; since the
	 * hasher is in a seperate thread, there is a small chance it completes
	 * before this thread continues, which means we clear the 'hashing' flag,
	 * resulting in a torrent that will never be flagged as completed...
	 */
	LOCK(data);
	assert(!hashingPiece[piece]);
	hashingPiece[piece] = true;
	if (registerHashing)
		numPiecesHashing++;
	UNLOCK(data);
	overseer->queueHashPiece(this, piece);
}

void
Torrent::updateBandwidth()
{
	RLOCK(peers);

	/* XXX we use this to update the snubbed status too! */
	uint32_t rx = 0, tx = 0;
	for (vector<Peer*>::iterator it = peers.begin();
	     it != peers.end(); it++) {
		Peer* p = (*it);
		rx += p->getRxRate(); tx += p->getTxRate();

		p->timer();
	}

	RWUNLOCK(peers);

	LOCK(data);
	rx_rate = rx; tx_rate = tx;
	UNLOCK(data);
}

void
Torrent::getRateCounters(uint32_t* rx, uint32_t* tx)
{
	LOCK(data);
	*rx = rx_rate; *tx = tx_rate;
	UNLOCK(data);
}

class peervector_matches {
public:
	peervector_matches(Peer* p) { peer = p; }

	bool operator() (const Peer* p) { return p == peer; }

private:
	Peer* peer;
};

void
Torrent::registerPeer(Peer* p)
{
	assert(p->getPeerID().size() == TORRENT_PEERID_LEN);

	WLOCK(peers);
	peers.push_back(p);
	RWUNLOCK(peers);
}

void
Torrent::unregisterPeer(Peer* p)
{
	/*
	 * First of all, remove the peer from our list. This ensures we won't
	 * try to obtain statistics or schedule requests.
	 */
	WLOCK(peers)
	vector<Peer*>::iterator peerit = peers.begin();
	while (peerit != peers.end()) {
		Peer* peer = *peerit;
		if (peer != p) {
			peerit++;
			continue;
		}

		peers.erase(peerit);
		break;
	}
	RWUNLOCK(peers);
	
	/*
	 * Deregister any requested pieces by this peer. This is safe to call without
	 * locking since the select(2) call notifies us of any changes and thus,
	 * nothing can change while we are nuking requests...
	 *
	 * Note that this will be called with peers locked anyway!
	 */
	LOCK(data);
	for (unsigned int j = 0; j < numPieces * (pieceLen / TORRENT_CHUNK_SIZE); j++)
		haveRequestedChunk[j].remove_if(peervector_matches(p));
	UNLOCK(data);
}

const uint8_t*
Torrent::getPeerID()
{
	return overseer->getPeerID();
}

void
Torrent::incrementUploadedBytes(uint64_t amount)
{
	LOCK(data);
	uploaded += amount;
	UNLOCK(data);
}

void
Torrent::heartbeat()
{
	/* Don't bother doing anything if we aren't fully launched */
	LOCK(data);
	if (numPiecesHashing > 0) {
		UNLOCK(data);
		return;
	}
	UNLOCK(data);

	/*
	 * If we need to chat with the tracker, do so. Note that we use the minimum interval
	 * if we are in dire need of peers, and the maximum interval otherwise.
	 */
	int interval = tracker_interval;
	if (!complete && tracker_min_interval > 0)
		interval = tracker_min_interval;
	if (time(NULL) >= lastTrackerContact + interval) {
		/* The time is now */
		handleTracker(lastTrackerContact == 0 ? "started" : "");
	}

	if (time(NULL) >= lastChokingAlgorithm + TORRENT_DELTA_CHOKING_ALGO) {
		handleUnchokingAlgorithm();
	}

	/* If we can fill up our peer slots, try it */
	while (true) {
		unsigned int numPeers = getNumPeers();
		if (numPeers >= TORRENT_DESIRED_PEERS)
				break;

		LOCK(data);
		if (pendingPeers.empty()) {
			UNLOCK(data);
			break;
		}
		PendingPeer* pp = pendingPeers.front();
		pendingPeers.pop_front();
		UNLOCK(data);

		Peer* p = pp->connect();
		delete pp;

		if (p != NULL) {
			/*
			 * It seems possible to connct to this peer; we should add it to both ourselves
			 * and the peer manager.
			 */
			WLOCK(peers);
			peers.push_back(p);
			RWUNLOCK(peers);
			overseer->callbackPeerAdded(p);

			/*
			 * Send the handshake; we can only do this after we have added the
			 * peer since the Sender won't know of it otherwise.
			 */
			p->sendHandshake();
		}
	}
}

void
Torrent::handleUnchokingAlgorithm()
{
	vector<Peer*> newUnchokes;

	/*
	 * Order interested peers based on their upload rate.
	 */
	RLOCK(peers);
	vector<Peer*> uiPeers;
	for (vector<Peer*>::iterator it = peers.begin();
	     it != peers.end(); it++) {
		Peer* p = (*it);
		if (!p->isPeerInterested())
			continue;
		uiPeers.push_back(p);
	}
	RWUNLOCK(peers);

	/* Given this list of peers, sort them by upload rate */
	sort(uiPeers.begin(), uiPeers.end(), Peer::compareByUpload);

	/*
	 * Unchoke the peers, but no more than TORRENT_MAX_UNCHOKED_PEERS per
	 * torrent.
	 */
	for (unsigned int i = 0; i < uiPeers.size() && newUnchokes.size() < TORRENT_MAX_UNCHOKED_PEERS; i++) {
		newUnchokes.push_back(uiPeers[i]);
	}

	RLOCK(peers);
	vector<Peer*> newChokes;
	for (vector<Peer*>::iterator it = peers.begin();
	     it != peers.end(); it++) {
		Peer* p = (*it);
		if (p->isPeerChoked())
			continue;
		if (find(newUnchokes.begin(), newUnchokes.end(), p) != newUnchokes.end())
			continue;
		newChokes.push_back(p);
	}
	RWUNLOCK(peers);

	int numChoked = 0, numUnchoked = 0;

	for (vector<Peer*>::iterator it = newChokes.begin();
	     it != newChokes.end(); it++) {
		(*it)->choke();
		numChoked++;
	}

	for (vector<Peer*>::iterator it = newUnchokes.begin();
	     it != newUnchokes.end(); it++){
		/* Only unchoke if the peer was choked */
		Peer* p = *it;
		if (!p->isPeerChoked())
			continue;
		p->unchoke();
		numUnchoked++;
	}

	/* Count the number of choked / unchoked peers */
	unsigned int curUnchoked = 0;
	RLOCK(peers);
	for (vector<Peer*>::iterator it = peers.begin();
	     it != peers.end(); it++) {
		Peer* p = (*it);
		if (p->isPeerChoked())
			continue;
		curUnchoked++;
	}
	RWUNLOCK(peers);

	lastChokingAlgorithm = time(NULL);
	if (numChoked > 0 || numUnchoked > 0)
		TRACE(CHOKING, "(un)choke algorithm: choked=%u and unchoked=%u, total unchoked=%u", numChoked, numUnchoked, curUnchoked);
}

void
Torrent::callbackPeerChangedInterest(Peer* p)
{
	/* We need to rerun the unchoking algorithm if the peer was unchoked */
	if (!p->isPeerChoked())
		handleUnchokingAlgorithm();
}

void
Torrent::callbackPeerChangedChoking(Peer* p)
{
	if (p->isChoking())
		return;

	/*
	 * We are unchoked! Fill up the peer with requests.
	 */
	schedulePeerRequests(p);
}

void
Torrent::processPeerStatus()
{
	LOCK(data); RLOCK(peers);

	for (vector<Peer*>::iterator it = peers.begin();
	     it != peers.end(); it++) {
		Peer* p = (*it);

		/*
		 * If this peer is a seeder and so we are, kick him; no point staying
		 * connected anyway.
		 */
		if (complete && p->isSeeder()) {
			TRACE(TORRENT, "kicking seeder peer=%s", p->getID().c_str());
			p->shutdown();
			continue;
		}

		/*
		 * For every peer, see if they have stuff we want. If so, claim interest;
		 * if not, revoke it.
		 */
		bool haveStuff = false;
		for (unsigned int i = 0; i < numPieces; i++)
			if (!havePiece[i] && p->hasPiece(i)) {
				haveStuff = true;
				break;
			}

		haveStuff ? p->claimInterest() : p->revokeInterest();
	}

	RWUNLOCK(peers); UNLOCK(data);
}

unsigned int
Torrent::getNumPeers()
{
	unsigned int n;

	RLOCK(peers);
	n = peers.size();
	RWUNLOCK(peers);

	return n;
}

vector<PieceInfo>
Torrent::getPieceDetails()
{
	vector<PieceInfo> pi;

	LOCK(data);
	for (unsigned int i = 0; i < numPieces; i++) {
		bool requested = false;
		for (unsigned int j = 0; j < calculateChunksInPiece(i); j++)
			if (haveRequestedChunk[(i * (pieceLen / TORRENT_CHUNK_SIZE)) + j].size() > 0) {
				requested = true;
				break;
			}
		pi.push_back(PieceInfo(i, havePiece[i], hashingPiece[i], requested));
	}
	UNLOCK(data);
	return pi;
}

PeerInfo::PeerInfo(Peer* p)
{
	snubbed = p->isPeerSnubbed();
	peer_interested = p->isPeerInterested();
	peer_choked = p->isPeerChoked();
	interested = p->isInterested();
	choking = p->isChoking();
	incoming = p->isIncoming();
	p->getAverageRate(&rx, &tx);
	endpoint = p->getEndpoint();
	num_pieces = p->getNumPeerPieces();
}

vector<PeerInfo>
Torrent::getPeerDetails()
{
	vector<PeerInfo> pi;

	RLOCK(peers);
	for (vector<Peer*>::iterator it = peers.begin();
	     it != peers.end(); it++) {
		Peer* p = *it;
		pi.push_back(PeerInfo(p));
	}
	RWUNLOCK(peers);

	return pi;
}

unsigned int 
Torrent::getNumPiecesComplete()
{
	unsigned int num = 0;

	LOCK(data);
	for (unsigned int i = 0; i < numPieces; i++)
		if (havePiece[i] && !hashingPiece[i])
			num++;
	UNLOCK(data);

	return num;
}

unsigned int
Torrent::getNumPendingPeers()
{
	unsigned int num;

	LOCK(data);
	num = pendingPeers.size();
	UNLOCK(data);
	return num;
}

Tracer*
Torrent::getTracer() {
	return overseer->getTracer();
}

void
Torrent::signalSender()
{
	overseer->signalSender();
}

bool
Torrent::canAcceptPeer()
{	
	LOCK(data);
	unsigned int n = numPiecesHashing;
	UNLOCK(data);

	/*
	 * Only accept if we have finished hashing and if there are enough peer slots
	 * left.
	 */
	return n == 0 && getNumPeers() < TORRENT_MAX_PEERS;
}

unsigned int
Torrent::getNumPiecesHashing()
{
	LOCK(data);
	unsigned int n = numPiecesHashing;
	UNLOCK(data);
	return n;
}

std::list<std::string>
Torrent::getMessageLog()
{
	LOCK(log);
	std::list<std::string> l = messageLog;
	UNLOCK(log);
	return l;
}

void
Torrent::clearMessageLog()
{
	LOCK(log);
	messageLog.clear();
	UNLOCK(log);
}

void
Torrent::log(Peer* p, const char* fmt, ...)
{
	struct tm tm;
	time_t t;
	va_list vl;
	string s;

	char timestamp[64 /* XXX */];
	time(&t);
	localtime_r(&t, &tm);
	strftime(timestamp, sizeof(timestamp), "%b %d %T", &tm);
	s = timestamp;

	if (p != NULL) {
		s += " (" + p->getID() + ")";
	}

	char temp[256 /* XXX */ ];
	va_start(vl, fmt);
	vsnprintf(temp, sizeof(temp), fmt, vl);
	va_end(vl);
	s += " - "; s += temp;

	LOCK(log);
	messageLog.push_back(s);
	UNLOCK(log);
}

void
Torrent::debugDump(FILE* f)
{
#define PRINT(fmt,args...) \
	fprintf(f, fmt"\n", ## args)

	LOCK(data);

	PRINT("<?xml version=\"1.0\"?>");
	PRINT("<torrent>");
	PRINT(" <name>%s</name>", name.c_str());
	if (endgame_mode)
		PRINT(" <endgame/>");
	PRINT(" <piecelen>%u</piecelen>", pieceLen);
	PRINT(" <pieces amount=\"%u\">", numPieces);
	for (unsigned int piece = 0; piece < numPieces; piece++) {
		PRINT("  <piece num=\"%u\">", piece);
		if (hashingPiece[piece]) {
			PRINT("   <hashing/>");
		} else if (havePiece[piece]) {
			PRINT("   <complete/>");
		} else {
			for (unsigned int j = 0; j < calculateChunksInPiece(piece); j++) {
				/* Only report individual chunks if there is something useful to report */
				if (haveChunk[(piece * (pieceLen / TORRENT_CHUNK_SIZE)) + j] || !haveRequestedChunk[(piece * (pieceLen / TORRENT_CHUNK_SIZE)) + j].empty()) {
					PRINT("   <chunk num=\"%u\">", j);
					if (haveChunk[(piece * (pieceLen / TORRENT_CHUNK_SIZE)) + j]) {
						PRINT("    <complete/>");
					} else {
						for (PeerList::iterator it = haveRequestedChunk[(piece * (pieceLen / TORRENT_CHUNK_SIZE)) + j].begin();
								 it != haveRequestedChunk[(piece * (pieceLen / TORRENT_CHUNK_SIZE)) + j].end(); it++) {
							Peer* p = *it;
							PRINT("    <requested fromPeer=\"%s\"/>", p->getID().c_str());
						}
					}
					PRINT("   </chunk>");
				}
			}
		}
		PRINT("  </piece>");
	}
	PRINT(" </pieces>");

	PRINT(" <peers>");
	RLOCK(peers);
	for (vector<Peer*>::iterator it = peers.begin();
     it != peers.end(); it++) {
		Peer* p = *it;
		PRINT("  <peer id=\"%s\">", p->getID().c_str());
		if (p->isPeerChoked())
			PRINT("   <peerChoked/>");
		if (p->isPeerInterested())
			PRINT("   <peerInterested/>");
		if (p->isChoking())
			PRINT("   <choking/>");
		if (p->isInterested())
			PRINT("   <interested/>");
		if (p->isPeerSnubbed())
			PRINT("   <snubbed/>");
		vector<bool> pm = p->getPieceMap();
		unsigned int num_pieces = 0;
		for (vector<bool>::iterator it = pm.begin();
		     it != pm.end(); it++) {
			if (*it)
				num_pieces++;
		}
		PRINT("   <pieces available=\"%u\" missing=\"%u\"/>", num_pieces, numPieces - num_pieces);
		PRINT("  </peer>");
	}
	RWUNLOCK(peers);
	PRINT(" </peers>");

	PRINT(" <files>");
	std::vector<FileInfo> fi = getFileDetails();
	for (std::vector<FileInfo>::iterator it = fi.begin();
	     it != fi.end(); it++) {

		PRINT("  <file name=\"%s\" length=\"%llu\" firstPiece=\"%u\" numPieces=\"%u\"/>",
		 (*it).getFilename().c_str(), (unsigned long long)(*it).getLength(), (*it).getFirstPieceNum(),
		 (*it).getNumPieces());
	}
	PRINT(" </files>");

	PRINT("</torrent>");

	UNLOCK(data);

#undef PRINT
}

FileInfo::FileInfo(File* f, unsigned int piece, unsigned int num)
{
	fname = f->getFilename();
	length = f->getLength();
	firstPiece = piece; numPieces = num;
}

std::vector<FileInfo>
Torrent::getFileDetails()
{
	vector<FileInfo> fi;

	/*
	 * We construct the fileinfo vector by wading through the available files,
	 * and calculating the begin piece and number of pieces for each file; this
	 * is much easier than attempting to figure out the pieces corresponding to
	 * a given file - and we want to handle them all anyway.
	 */
	off_t offset = 0;
	unsigned int piece, num; /* outside for loop for assertion below */

	RLOCK(files);
	for (vector<File*>::iterator it = files.begin();
	     it != files.end(); it++) {
		File* f = *it;

		piece = offset / pieceLen;
		num = 0;
		size_t fileLength = f->getLength();
		if (offset % pieceLen > 0) {
			/* This piece uses part of the current block, so add that */
			num++; fileLength -= MIN(pieceLen - (offset % pieceLen), fileLength);
		}
		num += fileLength / pieceLen;
		if (fileLength % pieceLen > 0)
			num++; /* round up to the next complete block */

		fi.push_back(FileInfo(f, piece, num));

		offset += f->getLength();
	}
	RWUNLOCK(files);

	/* safety guards */
	assert((uint64_t)offset == total_size);
	assert(piece + num == numPieces);

	return fi;
}

/* vim:set ts=2 sw=2: */
