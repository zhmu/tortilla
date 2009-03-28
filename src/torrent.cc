#include <sys/types.h>
#include <algorithm>
#include <assert.h>
#include <fcntl.h>
#include <iostream>
#include <pthread.h>
#include <stdlib.h>
#include <sstream>
#include <stdint.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include "exceptions.h"
#include "file.h"
#include "hasher.h"
#include "http.h"
#include "overseer.h"
#include "peer.h"
#include "sha1.h"
#include "tracer.h"
#include "torrent.h"

using namespace std;

#define MIN(a,b) \
	(((a) < (b)) ? (a) : (b))

#define LOCK(x)   pthread_mutex_lock(&mtx_ ## x);
#define UNLOCK(x) pthread_mutex_unlock(&mtx_ ## x);

Torrent::Torrent(Overseer* o, Metadata* md)
{
	overseer = o; downloaded = 0; uploaded = 0; left = 0;
	haveThread = false; terminating = false; complete = false;
	lastChokingAlgorithm = 0; unchokingRound = 0;
	optimisticUnchokedPeer = NULL; tracker_key = "";

	/* force the thread to contact the tracker */
	tracker_interval = 0; tracker_min_interval = 0;
	lastTrackerContact = 0;

	pthread_mutex_init(&mtx_peers, NULL);
	pthread_mutex_init(&mtx_data, NULL);

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
	requestedPiece.reserve(numPieces);
	hashingPiece.reserve(numPieces);
	pieceCardinality.reserve(numPieces);
	for (unsigned int i = 0; i < numPieces; i++) {
		havePiece.push_back(false);
		requestedPiece.push_back(NULL);
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
			haveRequestedChunk.push_back(false);
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
					MetaString* ms = dynamic_cast<MetaString*>(*itt);
					if (ms == NULL)
						throw TorrentException("file path list doesn't contain strings");
					/* XXX check string for badness */
					fullPath += "/" + ms->getString();

					files.push_back(new File(fullPath, miLength->getInteger()));
					total_size += miLength->getInteger();
				}
			}
	} else {
		/* There is only a single file in this torrent - all too easy */
		MetaInteger* miLength = dynamic_cast<MetaInteger*>((*info)["length"]);
		if (miLength == NULL)
			throw TorrentException("info dictionary doesn't contain a length");

		files.push_back(new File(msName->getString(), miLength->getInteger()));
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
			if (havePiece[piecenum])
				scheduleHashing(piecenum);
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
				scheduleHashing(piecenum);
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
			scheduleHashing(piecenum);
		piecenum++;
	}

	/*
	 * We must have processed as many pieces as there are in the file.
	 */
	assert(piecenum == numPieces);
}

Torrent::~Torrent()
{
	/* If we have a thread, gracefully ask it to die */
	if (haveThread)
		stop();

	overseer->cancelHashingTorrent(this);

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
	 * Nuke all our peers. XXX we should implement signalling and exit more
	 * gracefully.
	 */
	LOCK(peers);
	while (true) {
		vector<Peer*>::iterator it = peers.begin();
		if (it == peers.end())
			break;
		Peer* p = *it;
		peers.erase(it);
		callbackPeerGone(p);
		delete p;
	}
	UNLOCK(peers);

	/* Close all files, too */
	while (true) {
		vector<File*>::iterator it = files.begin();
		if (it == files.end())
			break;
		files.erase(it);
		delete (*it);
	}
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
		/* Otherwise, try to grab as many peers as we need to fill our list */
		LOCK(peers);
		uint32_t numPeers = peers.size();
		UNLOCK(peers);
		m["numwant"] = convertInteger(TORRENT_DESIRED_PEERS - numPeers);
	}
	lastTrackerContact = time(NULL);
	string result = HTTP::get(announceURL, m);

	/* Parse the result as metadata (which it should be) */
	Metadata* md;
	try {
		stringbuf sb(result);
		istream is(&sb);
		md = new Metadata(is);
	} catch (MetadataException e) {
		TRACE(TORRENT, "torrent %p: tracker returned garbage '%s'", this, result.c_str());
		return NULL;
	}

	/*
	 * If we got here, the result is valid metadata. However, a failure may have
	 * been reported.
	 */
	MetaString* ms = dynamic_cast<MetaString*>((*md->getDictionary())["failure reason"]);
	if (ms != NULL) {
		string failure = ms->getString();
		cout << failure << endl;
		delete md; /* Prevent memory leak */
		throw new TorrentException("tracker reported failure: " + failure);
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
		throw new TorrentException("tracker didn't report interval");
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
	 peerslist != NULL && peerslist != NULL ? peerslist->getList().size() : 0);
	if (peerslist != NULL) {
		/*
		 * The tracker has provided us with (possibly new) peers. Add them to the
		 * list if applicable.
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

			/*
			 * This peer looks fine. However, ensure we don't add too many peers to
			 * the list; this hurts the network.
			 */
			LOCK(peers);
			uint32_t numPeers = peers.size();
			UNLOCK(peers);
			if (numPeers >= TORRENT_DESIRED_PEERS)
				break;
		
			try {
				TRACE(NETWORK, "trying peer: torrent=%p, address=%s, port=%lu", this, msHost->getString().c_str(), msPort->getInteger());
				Peer* p = new Peer(this, msPeerID->getString(), msHost->getString(), msPort->getInteger());
				LOCK(peers);
				peers.push_back(p);
				UNLOCK(peers);
				TRACE(NETWORK, "added peer: torrent=%p, peer=%p, address=%s, port=%lu", this, p, msHost->getString().c_str(), msPort->getInteger());
			} catch (ConnectionException e) {
				TRACE(NETWORK, "skipping peer: torrent=%p, address=%s, port=%lu, error=%s", this, msHost->getString().c_str(), msPort->getInteger(), e.what());
			}
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

			try {
				TRACE(NETWORK, "trying compact peer: torrent=%p, address=%s, port=%u", this, ip, port);
				Peer* p = new Peer(this, string(""), string(ip), port);
				LOCK(peers);
				peers.push_back(p);
				UNLOCK(peers);
				TRACE(NETWORK, "added compact peer: torrent=%p, address=%s, port=%u", this, ip, port);
			} catch (ConnectionException e) {
				TRACE(NETWORK, "skipping compact peer: torrent=%p, address=%s, port=%u, error=%s", this, ip, port, e.what());
			}
		}
	}

	delete md;
}

void
Torrent::go()
{
	while (!terminating) {
		fd_set fds;

		/*
	 	 * Gracefully handle any peers that are going away.
		  */
		LOCK(peers);
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
		UNLOCK(peers);

		/* Construct our file descriptor set */
		int maxfd = -1;
		FD_ZERO(&fds);
		LOCK(peers);
		for (vector<Peer*>::iterator it = peers.begin();
		     it != peers.end(); it++) {
			int fd = (*it)->getFD();
			if (maxfd < fd) maxfd = fd;
			FD_SET(fd, &fds);
		}
		UNLOCK(peers);

		/*
		 *
		 * Note that, for busy torrents, this 0.5 second loop will never be reached.
		 */
		struct timeval tv;
		tv.tv_sec = 0; tv.tv_usec = 5000;
		int n = select(maxfd + 1, &fds, (fd_set*)NULL, (fd_set*)NULL, &tv);
		if (n == 0)
			continue;

		/*
		 * Wade through all peers, handle any data to service. No locking is needed
		 * here, as the only place were peers can die is the cleanup code above.
	   */
		for (vector<Peer*>::iterator it = peers.begin();
				 it != peers.end(); it++) {
			int fd = (*it)->getFD();
			if (!FD_ISSET(fd, &fds))
				continue;
			Peer* p = (*it);
			/*
			 * There is data here.
			 */
			uint8_t buf[65536 /* XXX */];
			ssize_t len = ::recv(fd, buf, sizeof(buf), 0);
			if (len <= 0) {
				/* socket lost */
				cerr << "connection lost" << endl;
				p->shutdown();
				continue;
			}

			/* Hand the data off to the application */
			if (p->receive(buf, len) == true) {
				/* Need to sever the connection */
				p->shutdown();
				continue;
			}
		}
	}
}

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

	scheduleRequests();
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
Torrent::scheduleRequests()
{
	/*
	 * XXX this algorithm should schedule a piece more randomly
	 */
	LOCK(data);
	for (unsigned int i = 0; i < numPieces && !terminating; i++) {
		if (!havePiece[i] && requestedPiece[i] == NULL && pieceCardinality[i] > 0) {
			/*
			 * This piece is of interest! Pick a peer, and if possible, give it the
		 	 * request.
			 */
			Peer* p = findPeerForPiece(i);
			assert(p != NULL);
			if (p->getNumRequests() >= TORRENT_PEER_MAX_REQUESTS)
				continue;
			p->claimInterest();
			p->requestPiece(i);
			TRACE(TORRENT, "requesting piece from peer %p, piece=%i", p, i);
			requestedPiece[i] = p;
		}
	}
	UNLOCK(data);
}

std::string
Torrent::convertInteger(uint64_t i)
{
	ostringstream o;
	o << i;
	return o.str();
}

int
Torrent::getMissingChunk(unsigned int piece)
{
	assert (piece < numPieces);

	unsigned int numChunks;
	if (piece == numPieces - 1) {
		numChunks = (total_size % pieceLen) / TORRENT_CHUNK_SIZE;
	} else {
		numChunks = pieceLen / TORRENT_CHUNK_SIZE;
	}

	LOCK(data);
	for (unsigned int j = 0; j < calculateChunksInPiece(piece); j++)
		if (!haveChunk[(piece * (pieceLen / TORRENT_CHUNK_SIZE)) + j] &&
		    !haveRequestedChunk[(piece * (pieceLen / TORRENT_CHUNK_SIZE)) + j]) {
			UNLOCK(data);
			return j;
		}
	UNLOCK(data);

	return -1;
}

void
Torrent::callbackCompletePiece(Peer* p, unsigned int piece)
{
	assert (piece < numPieces);

	LOCK(data);
	havePiece[piece] = true;
	UNLOCK(data);

	/*
	 * Ask the hasher to verify this chunk - using the callback, we figure out if
	 * we have to refetch the piece or accept that we think it's fine.
	 */
	scheduleHashing(piece);

	/* Try more! */
	scheduleRequests();
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

	writeChunk(piece, offset, data, len);

	LOCK(data);
	haveChunk[(piece * (pieceLen / TORRENT_CHUNK_SIZE)) + offset / TORRENT_CHUNK_SIZE] = true;
	downloaded += len;

	/* See if we have all chunks; if so, the piece is in */
	for (unsigned int i = 0; i < calculateChunksInPiece(piece); i++)
		if (!haveChunk[(piece * (pieceLen / TORRENT_CHUNK_SIZE)) + i]) {
			UNLOCK(data);
			return;
		}
	UNLOCK(data);

	/* Yay! */
	TRACE(TORRENT, "piece completed: piece=%u", piece);
	callbackCompletePiece(p, piece);
}

void
Torrent::setChunkRequested(unsigned int piece, unsigned int chunk, bool requested)
{
	assert (piece < numPieces);
	assert (chunk < pieceLen / TORRENT_CHUNK_SIZE);

	LOCK(data);
	haveRequestedChunk[(piece * (pieceLen / TORRENT_CHUNK_SIZE)) + chunk] = requested;
	UNLOCK(data);
}

void
Torrent::dump()
{
	LOCK(data);

	printf("piece map\n");
	for (unsigned int i = 0; i < numPieces; i++) {
		if (i % 50 == 0) {
				printf("\n");
				printf ("%4u:", i);
		}
		if (i % 10 == 0) printf(" ");

		if (havePiece[i]) {
			printf("%c", hashingPiece[i] ? '?' : '#');
		} else {
			int numQueued = 0;
			for (unsigned int j = 0; j < (pieceLen / TORRENT_CHUNK_SIZE); j++) {
				if (haveRequestedChunk[(i * (pieceLen / TORRENT_CHUNK_SIZE)) + j]) {
					numQueued++;
				}
			}
			printf("%c", numQueued > 0 ? '0' + numQueued  : '.');
		}
	}
	printf("\n");

	UNLOCK(data);
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
	hashingPiece[piece] = false;
	if (!result) {
		/*
		 * We got a corrupted piece! Mark it as not-available and attempt to
		 * request it again from someone else. XXX we should identify and ban
		 * seeders that provide us with bad content.
		 */
		havePiece[piece] = false;
		UNLOCK(data);
		scheduleRequests();
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
	 * Inform our peers that we have this piece.
	 */
	LOCK(peers);
	for (vector<Peer*>::iterator it = peers.begin();
	     it != peers.end(); it++) {
		Peer* p = (*it);
		p->have(piece);
	}
	UNLOCK(peers);
	UNLOCK(data);

	/* Try to use this as an attempt to signal disinterest in peers */
	processCurrentPeers();

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

void
Torrent::writeChunk(unsigned int piece, unsigned int offset, const uint8_t* buf, size_t length)
{
	assert(piece < numPieces);
	assert(length <= TORRENT_CHUNK_SIZE);

	/* Calculate the absolute position */
	size_t absolutePos = piece * pieceLen + offset;

	/* Locate the first file matching this position */
	unsigned int fileIndex;
	File* f = NULL;
	for (fileIndex = 0; fileIndex < files.size(); fileIndex++)  {
		if (absolutePos < files[fileIndex]->getLength()) {
			f = files[fileIndex];
			break;
		}
		absolutePos -= files[fileIndex]->getLength();
	}
	assert(f != NULL);

	/*
	 * Chunks are allowed to span between multiple files, so we keep on writing
	 * stuff until we run out of stuff to write.
	 */
	while (length > 0) {
		uint32_t writelen = MIN(f->getLength() - absolutePos, length);

		f->write(absolutePos, buf, writelen);

		if (writelen != length) {
			/* Couldn't write the entire chunk; go use the next file */
			fileIndex++; assert(fileIndex < files.size());
			f = files[fileIndex];
			absolutePos = 0;
		} else {
			absolutePos += writelen;
		}
		buf += writelen; length -= writelen;
	}
}

void
Torrent::readChunk(unsigned int piece, unsigned int offset, uint8_t* buf, size_t length)
{
	assert(piece < numPieces);

	/* Calculate the absolute position */
	size_t absolutePos = piece * pieceLen + offset;

	/* Locate the first file matching this position */
	unsigned int fileIndex;
	File* f = NULL;
	for (fileIndex = 0; fileIndex < files.size(); fileIndex++)  {
		if (absolutePos < files[fileIndex]->getLength()) {
			f = files[fileIndex];
			break;
		}
		absolutePos -= files[fileIndex]->getLength();
	}
	assert(f != NULL);

	/*
	 * Chunks are allowed to span between multiple files, so we keep on reading
	 * stuff until we run out of stuff to read.
	 */
	while (length > 0) {
		uint32_t readlen = MIN(f->getLength() - absolutePos, length);

		f->read(absolutePos, buf, readlen);

		if (readlen != length) {
			/* Couldn't read the entire chunk; go use the next file */
			fileIndex++; assert(fileIndex < files.size());
			f = files[fileIndex];
			absolutePos = 0;
		} else {
			absolutePos += readlen;
		}
		buf += readlen; length -= readlen;
	}
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
	printf(">>> torrent is complete!\n");
	complete = true;

	/* Kick anyone who is also a seeder */
	processCurrentPeers();
}

void
Torrent::scheduleHashing(unsigned int piece)
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
	UNLOCK(data);
	overseer->queueHashPiece(this, piece);
}

void*
torrent_thread(void* ptr)
{
	((Torrent*)ptr)->go();
	return NULL;
}

void
Torrent::start()
{
	assert(!haveThread);

	pthread_create(&thread, NULL, torrent_thread, this);
	haveThread = true;
}

void
Torrent::stop()
{
	assert(haveThread);

	terminating = true;
	pthread_join(thread, NULL);

	haveThread = false;
}

void
Torrent::updateBandwidth()
{
	LOCK(peers);

	/* XXX we use this to update the snubbed status too! */

	rx_rate = 0; tx_rate = 0;
	for (vector<Peer*>::iterator it = peers.begin();
	     it != peers.end(); it++) {
		Peer* p = (*it);
		rx_rate += p->getRxRate(); tx_rate += p->getTxRate();

		p->timer();
	}

	UNLOCK(peers);
}

void
Torrent::getRateCounters(uint32_t* rx, uint32_t* tx)
{
	*rx = rx_rate; *tx = tx_rate;
}

Peer*
Torrent::findPeerForPiece(uint32_t piece)
{
	/*
	 * Pick a random peer that has this piece and go. This is O(|peers|)), but we
	 * will will limit the number of peers eventually so this is fine XXX
	 *
	 * Note, this assumed the DATA mutex is locked!
	 *
	 */

	Peer* p = NULL;
	LOCK(peers);
	unsigned int peer_num = rand() % pieceCardinality[piece];
	for (vector<Peer*>::iterator it = peers.begin();
	     it != peers.end(); it++) {
		if (!(*it)->hasPiece(piece))
			continue;
		if (peer_num-- == 0) {
			p = (*it);
			break;
		}
	}
	UNLOCK(peers);
	return p;
}

void
Torrent::callbackPeerGone(Peer* p)
{
	/*
	 * Deregister any requested pieces by this peer. This is safe to call without
	 * locking since the select(2) call notifies us of any changes and thus,
	 * nothing can change while we are nuking requests...
	 *
	 * Note that this will be called with peers locked anyway!
	 */
	bool unregisteredPieces = false;
	for (unsigned int i = 0; i < requestedPiece.size(); i++) {
		if (requestedPiece[i] != p)
			continue;
    		requestedPiece[i] = NULL;
		unregisteredPieces = true;
	}

	/* Get rid of any pieces being uploaded to this peer */
	overseer->dequeuePeer(p);

	if (!unregisteredPieces)
		return;

	/* We removed pieces; try to schedule requests to fill the void
	 *
	 * XXX don't reschedule here
	scheduleRequests();
	*/
}

const uint8_t*
Torrent::getPeerID()
{
	return overseer->getPeerID();
}

void
Torrent::addPeer(Peer* p)
{
	assert(p->getPeerID().size() == TORRENT_PEERID_LEN);

	LOCK(peers);
	peers.push_back(p);
	UNLOCK(peers);
}

unsigned int 
Torrent::getNumPiecesHashing()
{
	unsigned int num = 0;

	LOCK(data);
	for (unsigned int i = 0; i < numPieces; i++)
		if (hashingPiece[i])
			num++;
	UNLOCK(data);

	return num;
}

void
Torrent::queueUploadRequest(Peer* p, uint32_t piece, uint32_t begin, uint32_t len)
{
	assert(piece < numPieces);
	overseer->queueUploadRequest(p, piece, begin, len);
}

void
Torrent::dequeueUploadRequest(Peer* p, uint32_t piece, uint32_t begin, uint32_t len)
{
	assert(piece < numPieces);
	overseer->dequeueUploadRequest(p, piece, begin, len);
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
	if (!haveThread)
		return;

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
}

void
Torrent::handleUnchokingAlgorithm()
{
	vector<Peer*> newUnchokes;

	/*
	 * The algorithm used here is the Choke Algorithm as described in:
	 *
	 * Arnaud Legout, Guillaume Urvoy-Keller, Pietro Michiardi.
	 * Understanding BitTorrent: An Experimental Perspective, INRIA 2005.
	 * (Obtained from http://hal.inria.fr/inria-00000156/en, v3 2005-11-10)
	 */

	/* New round, but >3 rounds doesn't matter so we round them */
	unchokingRound = (unchokingRound + 1) % 3;

	/*
	 * 1) Every three rounds, chose a random peer that is unchoked and
	 *    interested.  The algorithm calls this peer the 'optimistic unchoked
	 *    peer'.
	 */
	if (unchokingRound == 0) {
		optimisticUnchokedPeer = pickRandomPeer(1, 1, newUnchokes);
	}

	/*
	 * 2) Order peers that are interested and have sent at least one
	 *    block in the last 30 seconds (i.e. peers that are not snubbed)
	 */
	LOCK(peers);
	vector<Peer*> uiPeers;
	for (vector<Peer*>::iterator it = peers.begin();
	     it != peers.end(); it++) {
		Peer* p = (*it);
		if (p->isPeerSnubbed() || !p->isPeerInterested())
			continue;
		uiPeers.push_back(p);
	}
	UNLOCK(peers);

	/* Given this list of peers, sort them by upload rate */
	sort(uiPeers.begin(), uiPeers.end(), Peer::compareByUpload);

	/*
	 * 3) The three fastest peers are unchoked
	 *
	 * Note that we already check for the necessary condition for steps 4+ here!
	 */
	bool unchokedOUP = false;
	for (unsigned int i = 0; i < MIN(3, uiPeers.size()); i++) {
		newUnchokes.push_back(uiPeers[i]);
		if (uiPeers[i] == optimisticUnchokedPeer)
			unchokedOUP = true;
	}

	/*
	 * (4) If the optimistic unchoked is not part of the peers we just unchoked,
	 *     unchoke it and we are done.
	 */
	if (!unchokedOUP) {
		if (optimisticUnchokedPeer != NULL) {
			newUnchokes.push_back(optimisticUnchokedPeer);
			optimisticUnchokedPeer = NULL;
		}
	} else {
		/*
		 * (5) If the planned optimistic unchoke peer was part of the unchoked peers
		 *     (it was), we need to pick a new choked peer at random.
		 */
		while (true) {
			optimisticUnchokedPeer = pickRandomPeer(1, -1, newUnchokes);
			if (optimisticUnchokedPeer == NULL)
				break;

			/* Note that steps 5a/5b always unchoke the peer,  so we do that here */
			newUnchokes.push_back(optimisticUnchokedPeer);

			/* (5a) If this peer is interested, it is unchoked and the round is complete */
			if (optimisticUnchokedPeer->isPeerInterested())
				break;

			/* (5b) If this peer is not interested, it is unchoked and another peer is chosen */
		}
	}

	/*
	 * OK, newunchokes is the amount of peers we unchoke; start by choking
	 * not in it.
	 */
	LOCK(peers);
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
	UNLOCK(peers);

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

	lastChokingAlgorithm = time(NULL);
	TRACE(CHOKING, "(un)choke algorithm: choked=%u, unchoked=%u", numChoked, numUnchoked);
}

Peer*
Torrent::pickRandomPeer(int choked, int interested, std::vector<Peer*>& skiplist)
{
	std::vector<Peer*> sufficingPeers;

	/* Note that this implementation is O(|peers| |skiplist|) */
	LOCK(peers);
	for (vector<Peer*>::iterator it = peers.begin();
	     it != peers.end(); it++) {
		Peer* p = (*it);
		if (choked >= 0 && p->isPeerChoked() != (choked != 0))
			continue;
		if (interested >= 0 && p->isPeerInterested() != (interested != 0))
			continue;
		if (find(skiplist.begin(), skiplist.end(), p) != skiplist.end())
			continue;
		sufficingPeers.push_back(p);
	}
	UNLOCK(peers);

	/* If the set is empty, nothing matched our criteria */
	if (sufficingPeers.size() == 0)
		return NULL;

	/* Return someone */
	return sufficingPeers[rand() % sufficingPeers.size()];
}

void
Torrent::callbackPeerChangedInterest(Peer* p)
{
	/* We need to rerun the unchoking algorithm if the peer was unchoked */
	if (!p->isPeerChoked())
		handleUnchokingAlgorithm();
}

void
Torrent::processCurrentPeers()
{
	LOCK(data); LOCK(peers);

	for (vector<Peer*>::iterator it = peers.begin();
	     it != peers.end(); it++) {
		Peer* p = (*it);

		if (complete && p->isSeeder()) {
			TRACE(TORRENT, "kicking seeder peer=%p", p);
			p->shutdown();
			continue;
		}

		bool haveStuff = false;
		for (unsigned int i = 0; i < numPieces; i++)
			if (!havePiece[i] && p->hasPiece(i)) {
				haveStuff = true;
				break;
			}

		if (!haveStuff) {
			p->revokeInterest();
		}
	}

	UNLOCK(peers); UNLOCK(data);
}

/* vim:set ts=2 sw=2: */
