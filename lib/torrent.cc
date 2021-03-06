#include <boost/thread/locks.hpp>
#include <sys/types.h>
#include <algorithm>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <stdlib.h>
#include <sstream>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include "callbacks.h"
#include "exceptions.h"
#include "file.h"
#include "hasher.h"
#include "httprequest.h"
#include "info.h"
#include "macros.h"
#include "overseer.h"
#include "peer.h"
#include "pendingpeer.h"
#include "sha1.h"
#include "tracer.h"
#include "torrent.h"
#include "trackertalker.h"

using namespace std;
using namespace boost;
using namespace Tortilla;

#define TRACER (overseer->getTracer())

#define CALLBACK(x,args...) \
	overseer->getCallbacks()->x(args)

Torrent::Torrent(Overseer* o, Metadata* md, std::string path)
{
	overseer = o; downloaded = 0; uploaded = 0; left = 0;
	terminating = false; terminateTime = 0; removeOK = false; complete = false;
	lastChokingAlgorithm = 0; pendingRequest = NULL;
	optimisticUnchokedPeer = NULL; tracker_key = "";
	name = ""; endgame_mode = false; user_ptr = NULL;
	rx_rate = 0; tx_rate = 0; 

	/* force the thread to contact the tracker - but try so only each 10 minutes */
	tracker_interval = 600; tracker_min_interval = 0;
	lastTrackerContact = 0;

	/*
	 * Ensure the 'info' and 'announce' metadata fields exist; we can't do much
	 * without them.
	 */
	MetaDictionary* dictionary = md->getDictionary();
	const MetaDictionary* info = dynamic_cast<const MetaDictionary*>((*dictionary)["info"]);
	if (info == NULL)
		throw TorrentException("metadata doesn't contain an info dictionary");

	const MetaInteger* miPieceLength = dynamic_cast<const MetaInteger*>((*info)["piece length"]);
	const MetaString* miPieces = dynamic_cast<const MetaString*>((*info)["pieces"]);

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
	 * variable and adjusted (the specification does not state any restrictions
	 * on the piece length, but everyone seems to use >=2**18=256KB anyway)
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
	 * In case (2), there is a 'files' list, which houses the
	 * dictionaries containing 'length' and 'path' information.
	 */
	const MetaString* msName = dynamic_cast<const MetaString*>((*info)["name"]);
	if (msName == NULL)
		throw TorrentException("info dictionary doesn't contain a name!");
	name = msName->getString();

	/* XXX check name for badness */
	total_size = 0;
	const MetaList* mlFiles = dynamic_cast<const MetaList*>((*info)["files"]);
	if (mlFiles != NULL) {
		for (list<MetaField*>::const_iterator it = mlFiles->getList().begin();
			   it != mlFiles->getList().end(); it++) {
				const MetaDictionary* md = dynamic_cast<const MetaDictionary*>(*it);
				if (md == NULL)
					throw TorrentException("files list doesn't contain dictionaries");

				/* Fetch the file length and path */
				const MetaInteger* miLength = dynamic_cast<const MetaInteger*>((*md)["length"]);
				const MetaList* mlPath = dynamic_cast<const MetaList*>((*md)["path"]);
				if (miLength == NULL)
					throw TorrentException("file dictionary doesn't contain a length");
				if (mlPath == NULL)
					throw TorrentException("file dictionary doesn't contain a path");

				/* Construct the full path of the torrent file */
				string fullPath = msName->getString();
				for (list<MetaField*>::const_iterator itt = mlPath->getList().begin();
						 itt != mlPath->getList().end(); itt++) {
					const MetaString* ms = dynamic_cast<const MetaString*>(*itt);
					if (ms == NULL)
						throw TorrentException("file path list doesn't contain strings");
					/* XXX check string for badness */
					fullPath += "/" + ms->getString();
				}

				File* f = new File(fullPath, miLength->getInteger(), path);
				files.push_back(f);
				overseer->addFile(f);
				total_size += miLength->getInteger();
			}
	} else {
		/* There is only a single file in this torrent - all too easy */
		const MetaInteger* miLength = dynamic_cast<const MetaInteger*>((*info)["length"]);
		if (miLength == NULL)
			throw TorrentException("info dictionary doesn't contain a length");

		File* f = new File(msName->getString(), miLength->getInteger(), path);
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
	 * Make a copy of the torrent dictionary. As our torrent status data is just
	 * the torrent dictionary extended with status fields, it makes more sense
	 * to just copy the torrent dictionary than having to regenerate it (which
	 * would gain only a few KB of memory)
	 */
	torrentDictionary = new MetaDictionary(*md->getDictionary());

	/* Initializer our talker; this will speak with the trackers */
	trackerTalker = new TrackerTalker(this, torrentDictionary);

	/*
	 * If there is a 'taStatus' dictionary in the torrent, we must parse it. This is a
	 * Tortilla-specific dictionary containing the current torrent status, which we
	 * will use to prevent duplicate downloading of informating, needness hashing etc.
	 */
	bool restoredStatus = false;
	const MetaDictionary* status = dynamic_cast<const MetaDictionary*>((*dictionary)["taStatus"]);
	if (status != NULL) {
		/*
		 * Before we do anything with the status data, we must have all files in
		 * this torrent available; if not, we discard any stored status. This may
		 * seem excessive, but if files are missing, we can only assume the user
		 * has been playing with them and we dare not trust the status at all.
		 * Better safe than sorry.
		 */
		bool filesOK = true;
		for (unsigned int i = 0; i < files.size(); i++) {
			if (!files[i]->haveReopened()) {
				filesOK = false;
				break;
			}
		}

		if (filesOK) {
			restoredStatus = restoreStatus(status);
			TRACE(TORRENT, "%s embedded torrent status", restoredStatus ? "accepted" : "rejected");
		} else {
			TRACE(TORRENT, "found embedded torrent status but couldn't re-open any files - status discarded");
		}
	}

	/*
	 * If one or more files were pre-existing (this means they existed and
	 * have the correct length), we already have the pieces. Since we have
	 * no away of knowing whether the full file was retrieved or just a
	 * portion, hash away!
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
			if (havePiece[piecenum] && !restoredStatus) {
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
			if (f->haveReopened() && !restoredStatus)
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
		if (havePiece[piecenum] && !restoredStatus)
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
	/* Cancel any hashing attempt, as we'll close the files soon enough */
	overseer->cancelHashing(this);

	/*
	 * Remove our peers; actual cleanup will be handled by the overseer.
	 */
	{
		unique_lock<shared_mutex> lock(rwl_peers);
		for (vector<Peer*>::iterator it = peers.begin();
				 it != peers.end(); it++) {
			Peer* p = *it;
			overseer->removePeer(p);
		}
		peers.clear();
	}

	/* Close all files, too */
	{
		unique_lock<shared_mutex> lock(rwl_files);
		while (true) {
			vector<File*>::iterator it = files.begin();
			if (it == files.end())
				break;
			File* f = *it;
			files.erase(it);
			overseer->removeFile(f);
			delete f;
		}
	}

	/* Delete pending peers and piece hashes */
	while (!pendingPeers.empty()) {
		PendingPeer* pp = pendingPeers.front();
		pendingPeers.pop_front();
		delete pp;
	}
	delete[] pieceHash;

	delete torrentDictionary;
	delete trackerTalker;
}

void
Torrent::contactTracker(std::string event)
{
	map<string, string> m;

	TRACE(DEBUG, "contacttracker: '%s'", event.c_str());

	/* Construct the tracker request, and off it goes */
	string h((const char*)infoHash, sizeof(infoHash));
	string peerID((const char*)overseer->getPeerID(), TORRENT_PEERID_LEN);
	{
		unique_lock<mutex> lock(mtx_data);
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
	}
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

	trackerTalker->request(m);
}

void
Torrent::handleTrackerReply(string reply)
{
	/* Parse the result as metadata (which it should be) */
	Metadata* md;
	try {
		stringbuf sb(reply);
		istream is(&sb);
		md = new Metadata(is);
	} catch (MetadataException e) {
		log(NULL, "tracker returned garbage: %s", reply.c_str());
		CALLBACK(gotTrackerReply, this, -1, "<cannot parse result>");
		return;
	}

	/*
	 * If we got here, the result is valid metadata. However, a failure may have
	 * been reported.
	 */
	const MetaString* ms = dynamic_cast<const MetaString*>((*md->getDictionary())["failure reason"]);
	if (ms != NULL) {
		string failure = ms->getString();
		delete md; /* Prevent memory leak */
		log(NULL, "tracker reported failure: %s", failure.c_str());
		CALLBACK(gotTrackerReply, this, -1, "<cannot parse result>");
		return;
	}

	/*
	 * Fetch the tracker interval times. The maximum interval must be present.
	 */
	const MetaInteger* msInterval = dynamic_cast<const MetaInteger*>((*md->getDictionary())["interval"]);
	if (msInterval == NULL)
		log(NULL, "tracker didn't report interval, ignoring reply");
	tracker_interval = msInterval->getInteger();
	const MetaInteger* msMinInterval = dynamic_cast<const MetaInteger*>((*md->getDictionary())["min interval"]);
	if (msMinInterval != NULL)
		tracker_min_interval = msMinInterval->getInteger();
	else
		tracker_min_interval = 0;
	const MetaString* msKey = dynamic_cast<const MetaString*>((*md->getDictionary())["key"]);
	if (msKey != NULL)
		tracker_key = msKey->getString();

	int numNewPeers = 0;
	const MetaList* peerslist = dynamic_cast<const MetaList*>((*md->getDictionary())["peers"]);
	TRACE(TRACKER, "contacted tracker: torrent=%p, interval=%u, min_interval=%u, key='%s',peers=%u",
	 this, tracker_interval, tracker_min_interval, tracker_key.c_str(),
	 peerslist != NULL ? peerslist->getList().size() : 0);
	if (peerslist != NULL && !complete) {
		/*
		 * The tracker has provided us with (possibly new) peers. Add them to the
		 * list if applicable - note that we only do this if the torrent is
		 * incomplete, as it makes no sense to try to find new peers in such
		 * a case (let them find us instead)
		 */
		for (list<MetaField*>::const_iterator it = peerslist->getList().begin();
		    it != peerslist->getList().end(); it++) {
			const MetaDictionary* dict = dynamic_cast<const MetaDictionary*>(*it);
			if (dict == NULL)
				continue;

			/* Grab all fields we really need; if any isn't available, ignore the entry */
			const MetaString*  msHost   = dynamic_cast<const MetaString*>((*dict)["ip"]);
			const MetaString*  msPeerID = dynamic_cast<const MetaString*>((*dict)["peer id"]);
			const MetaInteger* msPort   = dynamic_cast<const MetaInteger*>((*dict)["port"]);
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

			{
				unique_lock<mutex> lock(mtx_data);
				pendingPeers.push_back(new PendingPeer(this, msHost->getString(), msPort->getInteger(), msPeerID->getString()));
			}
			numNewPeers++;
		}
	}

	int compactpeerSize = 6;

	const MetaString* peerstring = dynamic_cast<const MetaString*>((*md->getDictionary())["peers"]);
	if (peerstring != NULL && peerstring->getString().size() % compactpeerSize == 0) {
		/*We got a compact peer list! */
		TRACE(TRACKER, "contacted tracker: torrent=%p, compact peers=%u",
		 this, peerstring->getString().size() / compactpeerSize);
		for (unsigned int i = 0; i < peerstring->getString().size() / compactpeerSize; i++) {
			char ip[32];
			uint16_t port;

			/*
			 * A compact peer list means we just get N times 6-byte entry
			 * A,B,C,D,E,F; which means we have connect to IP address
		 	 * A.B.C.D port E * 256 + F. Convert this to a string and
			 * feed it into PendingPeer (both PendingPeer and Connection
			 * can only deal with strings anyway since they can be used to
			 * express both IPv4 and IPv6 adresses)
		 	 */
			const uint8_t* ptr = (const uint8_t*)(peerstring->getString().c_str() + i * compactpeerSize);
			snprintf(ip,   sizeof(ip),   "%u.%u.%u.%u",
			 (uint8_t)ptr[0], (uint8_t)ptr[1],
			 (uint8_t)ptr[2], (uint8_t)ptr[3]);
			port = (uint16_t)(ptr[4] << 8) | ptr[5];

			{
				unique_lock<mutex> lock(mtx_data);
				pendingPeers.push_back(new PendingPeer(this, string(ip), port, ""));
			}
			numNewPeers++;
		}
	}
	delete md;

	CALLBACK(gotTrackerReply, this, numNewPeers, "");
}

void
Torrent::callbackPiecesAdded(Peer* p, vector<unsigned int>& pieces)
{
	{
		unique_lock<mutex> lock(mtx_data);
		for (vector<unsigned int>::iterator it = pieces.begin();
				 it != pieces.end(); it++) {
			assert(*it < numPieces);
			pieceCardinality[*it]++;
		}
	}

	/* Use this to signal interest in a peer */
	processPeerStatus();
}

void
Torrent::callbackPiecesRemoved(Peer* p, vector<unsigned int>& pieces)
{
	unique_lock<mutex> lock(mtx_data);
	for (vector<unsigned int>::iterator it = pieces.begin();
	     it != pieces.end(); it++) {
		assert(*it < numPieces);
		assert(pieceCardinality[*it] > 0);
		pieceCardinality[*it]--;
	}
}

void
Torrent::schedulePeerRequests(Peer* p)
{
	/*
	 * If we have the full torrent already, are shutting down, the peer is
	 * choking us, or the peer is going away, don't bother trying to schedule
	 * something.
	 */
	if (complete || terminating || p->isChoking() || p->isShuttingDown())
		return;

	/*
	 * XXX this algorithm should schedule a piece more randomly
	 */
	for (unsigned int i = 0; i < numPieces && !terminating; i++) {
		bool b;
		{
			unique_lock<mutex> lock(mtx_data);
			b = havePiece[i] || !p->hasPiece(i);
		}
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

	unique_lock<mutex> lock(mtx_data);
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
			return j;
		}
	}

	return -1;
}

void
Torrent::callbackCompletePiece(Peer* p, unsigned int piece)
{
	assert(piece < numPieces);
	{
		unique_lock<mutex> lock(mtx_data);
		assert(!havePiece[piece]);
		havePiece[piece] = true;
	}

	/*
	 * Ask the hasher to verify this chunk - once it is done, we use
	 * the callback to figure out whether we have to refetch the piece or accept
	 * that we think it's fine.
	 */
	scheduleHashing(piece);
}

unsigned int
Torrent::calculateChunksInPiece(unsigned int piece) const
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

	{
		unique_lock<mutex> lock(mtx_data);
		if (havePiece[piece]) {
			/*
			 * This can happen in endgame mode; if we have requested a piece but
			 * couldn't cancel it anymore (or if we are too late), we may get the
			 * last data while we are hashing. If this happens, just ignore the
			 * data alltogether.
			 */
			mtx_data.unlock();
			schedulePeerRequests(p);
			return;
		}

		/*
		 * Immediately mark the chunk as completed; this prevents anyone else from
		 * scheduling it.
		 */
		haveChunk[(piece * (pieceLen / TORRENT_CHUNK_SIZE)) + offset / TORRENT_CHUNK_SIZE] = true;
	}

	if (!writeChunk(piece, offset, data, len)) {
		TRACE(TORRENT, "unable to write chunk, piece=%lu, offset=%lu, len=%lu", piece, offset, len);
		{
			unique_lock<mutex> lock(mtx_data);
			haveChunk[(piece * (pieceLen / TORRENT_CHUNK_SIZE)) + offset / TORRENT_CHUNK_SIZE] = false;
		}
	}

	/*
	 * If anyone else is downloading this chunk, cancel it. Same goes for any
	 * REQUEST messages we may have queued but not sent.
	 */
	{
		shared_lock<shared_mutex> lock(rwl_peers);
		for (vector<Peer*>::iterator it = peers.begin();
				 it != peers.end(); it++) {
			Peer* p = (*it);
			p->cancelChunk(piece, offset, len);
			p->cancelChunkRequest(piece, offset, len);
		}
	}

	bool full = true;
	{
		unique_lock<mutex> lock(mtx_data);
		downloaded += len;

		/* See if we have all chunks; if so, the piece is in */
		for (unsigned int i = 0; i < calculateChunksInPiece(piece); i++) {
			if (!haveChunk[(piece * (pieceLen / TORRENT_CHUNK_SIZE)) + i]) {
				full = false;
				break;
			}
		}
	}

	schedulePeerRequests(p);
	if (!full)
		return;

	/* Yay! */
	TRACE(TORRENT, "piece completed: piece=%u", piece);
	callbackCompletePiece(p, piece);
}

bool
Torrent::hasPiece(unsigned int piece) const
{
	assert (piece < numPieces);

	bool b;
	{
		unique_lock<mutex> lock(mtx_data);
		b = havePiece[piece];
	}

	return b;
}

void
Torrent::callbackCompleteHashing(unsigned int piece, bool result)
{
	{
		unique_lock<mutex> lock(mtx_data);
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

			/*
			 * Furthermore, we need to clear the individual 'have chunk' too,
			 * since we need to fetch the entire piece again (maybe only a single
			 * chunk is bad, but there is no way of knowing...)
			 */
			for (unsigned int j = 0; j < calculateChunksInPiece(piece); j++) {
				haveChunk[(piece * (pieceLen / TORRENT_CHUNK_SIZE)) + j] = false;
			}
			return;
		}

		/* At least someone has this piece... we do! */
		pieceCardinality[piece]++;
		if (piece == numPieces - 1) {
			left -= getTotalSize() % pieceLen > 0 ?
							getTotalSize() % pieceLen : pieceLen;
		} else {
			left -= pieceLen;
		}

		/*
		 * Enter endgame mode if needed. XXX doing it on a fixed percentage is stupid,
		 * this must be restructured to only enter endgame mode if all chucks are
		 * scheduled.
		 */
		if (!endgame_mode && ((total_size - left) / (float)total_size) * 100.0f >= TORRENT_ENDGAME_PERCENTAGE) {
			endgame_mode = true;
			TRACE(TORRENT, "endgame mode: torrent=%p", this);
		}

		/*
		 * Inform our peers that we have this piece. We let go of the data lock,
		 * since we don't care if data changes here (we cannot lose the piece
		 * anymore, as the hash checked out)
		 */
	}

	{
		shared_lock<shared_mutex> lock(rwl_peers);
		for (vector<Peer*>::iterator it = peers.begin();
				 it != peers.end(); it++) {
			Peer* p = (*it);
			p->have(piece);
		}
	}

	/* Try to use this as an attempt to signal disinterest in peers */
	processPeerStatus();

	/* Before we check the total state, tell the client we have yet another piece */
	CALLBACK(completedPiece, this, piece);

	/* If we have all pieces, rejoice */
	{
		unique_lock<mutex> lock(mtx_data);
		for (unsigned int i = 0; i < numPieces; i++)
			if (!havePiece[i] || hashingPiece[i])
				return;
	}

	/* We have all pieces and are hashing none of them; torrent must be in */
	complete = true;
	TRACE(TORRENT, "torrent completed: torrent=%p", this);
	callbackCompleteTorrent();
}

bool
Torrent::handleChunk(unsigned int piece, unsigned int offset, uint8_t* buf, size_t length, bool writing)
{
	assert(piece < numPieces);
	assert(length <= TORRENT_CHUNK_SIZE);

	/*
	 * XXX this entire mess should be rewritten to use a tree structure; this
	 * would reduce the time needed to find the file from O(#files) to
	 * O(lg(#files))
	 */

	/* Calculate the absolute position */
	off_t absolutePos = (off_t)piece * (off_t)pieceLen + (off_t)offset;

	/* Locate the first file matching this position */
	unsigned int idx = 0;
	File* f = NULL;
	{
		shared_lock<shared_mutex> lock(rwl_files);
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
			 * torrent is terminating, but do not rely on it; bad people
			 * may use it to crash us.
			 */
			return false;
		}

		/*
		 * Chunks are allowed to span between multiple files, so we keep on writing
		 * stuff until we run out of stuff to write.
		 */
		while (length > 0) {
			/*
			 * This size_t cast is safe, since we want the minimum and max_value(size_t) <
			 * max_value(off_t),
			 */
			size_t partlen = std::min((size_t)(f->getLength() - absolutePos), length);
			
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
	}
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
Torrent::getPieceHash(unsigned int piece) const
{
	assert(piece < numPieces);
	return (pieceHash + (piece * TORRENT_HASH_LEN));
}

void
Torrent::callbackCompleteTorrent()
{
	complete = true;

	/* XXX inform tracker */

	/* Kick anyone who is also a seeder */
	processPeerStatus();

	/* Victory */
	CALLBACK(completedTorrent, this);
}

void
Torrent::scheduleHashing(unsigned int piece, bool registerHashing)
{
	assert(piece < numPieces);

	/*
	 * Mark the piece as being hashed before we actually add it; since the
	 * hasher is in a seperate thread, there would be a tiny window where
	 * the hasher finished hashing the piece yet this function sets the
	 * hashing flag...
	 */
	{
		unique_lock<mutex> lock(mtx_data);
		assert(!hashingPiece[piece]);
		hashingPiece[piece] = true;
		if (registerHashing)
			numPiecesHashing++;
	}
	overseer->queueHashPiece(this, piece);
}

void
Torrent::updateBandwidth()
{
	/*
	 * Calculate RX/TX rate; while here, call peer's timer function
	 * to handle snubbing.
	 */
	uint32_t rx = 0, tx = 0;
	{
		shared_lock<shared_mutex> lock(rwl_peers);
		for (vector<Peer*>::iterator it = peers.begin();
				 it != peers.end(); it++) {
			Peer* p = (*it);
			rx += p->getRxRate(); tx += p->getTxRate();

			p->timer();
		}
	}

	/* Update RX/TX rates */
	{
		unique_lock<mutex> lock(mtx_data);
		rx_rate = rx; tx_rate = tx;
	}
}

void
Torrent::getRateCounters(uint32_t* rx, uint32_t* tx)
{
	unique_lock<mutex> lock(mtx_data);
	*rx = rx_rate; *tx = tx_rate;
}

void
Torrent::registerPeer(Peer* p)
{
	{
		unique_lock<shared_mutex> lock(rwl_peers);
		peers.push_back(p);
	}

	CALLBACK(addedPeer, this, p);
}

void
Torrent::unregisterPeer(Peer* p)
{
	/*
	 * First of all, remove the peer from our list. This ensures we won't
	 * try to obtain statistics or schedule requests.
	 */
	{
		unique_lock<shared_mutex> lock(rwl_peers);
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
	}
	
	/*
	 * Deregister any requested pieces by this peer. This results in these pieces
	 * being rescheduled during a next call to schedulePeerRequests().
	 */
	{
		unique_lock<mutex> lock(mtx_data);
		for (unsigned int j = 0; j < numPieces * (pieceLen / TORRENT_CHUNK_SIZE); j++)
			haveRequestedChunk[j].remove_if(peervector_matches(p));
	}

	CALLBACK(removingPeer, this, p);
}

const uint8_t*
Torrent::getPeerID() const
{
	return overseer->getPeerID();
}

void
Torrent::incrementUploadedBytes(uint64_t amount)
{
	unique_lock<mutex> lock(mtx_data);
	uploaded += amount;
}

void
Torrent::heartbeat()
{
	/* Don't bother doing anything if we aren't fully launched */
	HTTPRequest* req;
	{
		unique_lock<mutex> lock(mtx_data);
		if (numPiecesHashing > 0)
			return;

		req = pendingRequest;
		pendingRequest = NULL;
	}
	if (req != NULL)
		overseer->addRequest(req);

	/*
	 * If we are terminating, don't bother initiating tracker contact or
 	 * dealing with peers.
	 */
	if (terminating)
		return;

	/*
	 * If we need to chat with the tracker, do so. Note that we use the minimum
	 * interval if we need peers (i.e. the torrent isn't completed yet), and the
	 * maximum interval otherwise.
	 */
	int interval = tracker_interval;
	if (!complete && tracker_min_interval > 0)
		interval = tracker_min_interval;
	if (time(NULL) >= lastTrackerContact + interval) {
		/* The time is now */
		contactTracker(lastTrackerContact == 0 ? "started" : "");
	}

	if (time(NULL) >= lastChokingAlgorithm + TORRENT_DELTA_CHOKING_ALGO) {
		handleUnchokingAlgorithm();
	}

	/* If we can fill up our peer slots, try it */
	while (true) {
		unsigned int numPeers = getNumPeers();
		if (numPeers >= TORRENT_DESIRED_PEERS)
				break;

		PendingPeer* pp;
		{
			unique_lock<mutex> lock(mtx_data);
			if (pendingPeers.empty())
				break;
			pp = pendingPeers.front();
			pendingPeers.pop_front();
		}

		Peer* p = pp->connect();
		delete pp;

		if (p != NULL) {
			/*
			 * It seems possible to connect to this peer; we should add it to both ourselves
			 * and the overseer.
			 */
			registerPeer(p);
			overseer->addPeer(p);

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
	vector<Peer*> uiPeers;
	{
		shared_lock<shared_mutex> lock(rwl_peers);
		for (vector<Peer*>::iterator it = peers.begin();
				 it != peers.end(); it++) {
			Peer* p = (*it);
			if (!p->isPeerInterested())
				continue;
			uiPeers.push_back(p);
		}
	}

	/* Given this list of peers, sort them by upload rate */
	sort(uiPeers.begin(), uiPeers.end(), Peer::compareByUpload);

	/*
	 * Unchoke the peers, but no more than TORRENT_MAX_UNCHOKED_PEERS per
	 * torrent.
	 */
	for (unsigned int i = 0; i < uiPeers.size() && newUnchokes.size() < TORRENT_MAX_UNCHOKED_PEERS; i++) {
		newUnchokes.push_back(uiPeers[i]);
	}

	vector<Peer*> newChokes;
	{
		shared_lock<shared_mutex> lock(rwl_peers);
		for (vector<Peer*>::iterator it = peers.begin();
				 it != peers.end(); it++) {
			Peer* p = (*it);
			if (p->isPeerChoked())
				continue;
			if (find(newUnchokes.begin(), newUnchokes.end(), p) != newUnchokes.end())
				continue;
			newChokes.push_back(p);
		}
	}

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
	{
		shared_lock<shared_mutex> lock(rwl_peers);
		for (vector<Peer*>::iterator it = peers.begin();
				 it != peers.end(); it++) {
			Peer* p = (*it);
			if (p->isPeerChoked())
				continue;
			curUnchoked++;
		}
	}

	lastChokingAlgorithm = time(NULL);
	if (numChoked > 0 || numUnchoked > 0)
		TRACE(CHOKING, "(un)choke algorithm: choked=%u and unchoked=%u, total unchoked=%u", numChoked, numUnchoked, curUnchoked);
}

void
Torrent::callbackPeerChangedInterest(Peer* p)
{
	/* We need to rerun the unchoking algorithm if the peer was unchoked */
	if (!p->isPeerChoked() && !terminating)
		handleUnchokingAlgorithm();
}

void
Torrent::callbackPeerChangedChoking(Peer* p)
{
	if (p->isChoking() || terminating)
		return;

	/*
	 * We are unchoked! Fill up the peer with requests.
	 */
	schedulePeerRequests(p);
}

void
Torrent::processPeerStatus()
{
	unique_lock<mutex> lock_data(mtx_data);
	shared_lock<shared_mutex> lock_peers(rwl_peers);

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
}

unsigned int
Torrent::getNumPeers() const
{
	unsigned int n;

	{
		shared_lock<shared_mutex> lock(rwl_peers);
		n = peers.size();
	}

	return n;
}

vector<PieceInfo>
Torrent::getPieceDetails() const
{
	vector<PieceInfo> pi;

	{
		unique_lock<mutex> lock(mtx_data);
		for (unsigned int i = 0; i < numPieces; i++) {
			bool requested = false;
			for (unsigned int j = 0; j < calculateChunksInPiece(i); j++)
				if (haveRequestedChunk[(i * (pieceLen / TORRENT_CHUNK_SIZE)) + j].size() > 0) {
					requested = true;
					break;
				}
			pi.push_back(PieceInfo(i, havePiece[i], hashingPiece[i], requested));
		}
	}
	return pi;
}

vector<PeerInfo>
Torrent::getPeerDetails() const
{
	vector<PeerInfo> pi;

	{
		shared_lock<shared_mutex> lock(rwl_peers);
		for (vector<Peer*>::const_iterator it = peers.begin();
				 it != peers.end(); it++) {
			Peer* p = *it;
			pi.push_back(PeerInfo(p));
		}
	}

	return pi;
}

unsigned int 
Torrent::getNumPiecesComplete() const
{
	unsigned int num = 0;

	{
		unique_lock<mutex> lock(mtx_data);
		for (unsigned int i = 0; i < numPieces; i++)
			if (havePiece[i] && !hashingPiece[i])
				num++;
	}

	return num;
}

unsigned int
Torrent::getNumPendingPeers() const
{
	unsigned int num;

	{
		unique_lock<mutex> lock(mtx_data);
		num = pendingPeers.size();
	}
	return num;
}

Tracer*
Torrent::getTracer() const
{
	return overseer->getTracer();
}

void
Torrent::signalSender() const
{
	overseer->signalSender();
}

bool
Torrent::canAcceptPeer() const
{	
	/*
	 * Only accept if we have finished hashing and if there are enough peer slots
	 * left.
	 */
	return getNumPiecesHashing() == 0 && getNumPeers() < TORRENT_MAX_PEERS;
}

unsigned int
Torrent::getNumPiecesHashing() const
{
	unsigned int n;
	{
		unique_lock<mutex> lock(mtx_data);
		n = numPiecesHashing;
	}
	return n;
}

std::list<std::string>
Torrent::getMessageLog() const
{
	unique_lock<mutex> lock(mtx_log);
	std::list<std::string> l = messageLog;
	return l;
}

void
Torrent::clearMessageLog()
{
	unique_lock<mutex> lock(mtx_log);
	messageLog.clear();
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

	{
		unique_lock<mutex> lock(mtx_log);
		messageLog.push_back(s);
	}
}

void
Torrent::debugDump(FILE* f) const
{
	unique_lock<mutex> lock(mtx_data);

#define PRINT(fmt,args...) \
	fprintf(f, fmt"\n", ## args)

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
						for (PeerList::const_iterator it = haveRequestedChunk[(piece * (pieceLen / TORRENT_CHUNK_SIZE)) + j].begin();
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
	{
		shared_lock<shared_mutex> lock(rwl_peers);
		for (vector<Peer*>::const_iterator it = peers.begin();
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
	}
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

#undef PRINT
}

std::vector<FileInfo>
Torrent::getFileDetails() const
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
	{
		shared_lock<shared_mutex> lock(rwl_files);
		for (vector<File*>::const_iterator it = files.begin();
				 it != files.end(); it++) {
			File* f = *it;

			piece = offset / pieceLen;
			num = 0;
			off_t fileLength = f->getLength();
			if (offset % pieceLen > 0) {
				/* This piece uses part of the current block, so add that */
				num++; fileLength -= std::min(pieceLen - (offset % pieceLen), fileLength);
			}
			num += fileLength / pieceLen;
			if (fileLength % pieceLen > 0)
				num++; /* round up to the next complete block */

			fi.push_back(FileInfo(f, piece, num));

			offset += f->getLength();
		}
	}

	/* safety guards */
	assert((uint64_t)offset == total_size);
	assert(piece + num == numPieces);

	return fi;
}

void
Torrent::callbackTrackerReply(std::string result, bool error)
{
	TRACE(TORRENT, "tracker reply received, error=%u", error ? 1 : 0);

	/*
	 * If we were terminating, just accept anything the tracker gives us. There's
	 * nothing left to gain.
	 */
	if (terminating) {
		TRACE(TORRENT, "removeOK! [%s]\n", result.c_str());
		removeOK = true;
		return;
	}

	if (!error) {
		handleTrackerReply(result);
		return;
	}
	CALLBACK(gotTrackerReply, this, -1, "<unable to establish connection>");
}

void
Torrent::addRequest(HTTPRequest* r)
{
	/*
	 * If we are terminating, allow the request can and should be overwritten.
	 * Otherwise, there is a bug.
	 */
	assert(terminating || pendingRequest == NULL);
	{
		unique_lock<mutex> lock(mtx_data);
		pendingRequest = r;
	}
}

void
Torrent::shutdown()
{
	/*
	 * First of all, set the 'terminating' value. This prevents new chunks from
	 * being scheduled, new requests from being handeled etc.
	 */
	{
		unique_lock<mutex> lock(mtx_data);
		terminating = true; terminateTime = time(NULL);
	}

	/* Cancel any hashing attempt; it's a waste of resources at this point */
	overseer->cancelHashing(this);

	/*
	 * Inform the tracker that we are going away.
	 */
	contactTracker("stopped");
}

Metadata*
Torrent::storeStatus() const
{
	Metadata* md = new Metadata(*torrentDictionary);

	/*
	 * We have a clone of the torrent's main dictionary. We'll add a 'taStatus'
	 * dictionary containing the following:
	 *
	 * - 'chunkOK': one bit per chunk indicating whether it's obtained.
	 * - 'pieceOK': one bit per piece indicating whether it's hashed OK.
	 */
	MetaDictionary* status = new MetaDictionary();
	md->getDictionary()->assign("taStatus", status);

	int piecemapLen = (numPieces + 7) / 8;
	int chunkmapLen = (numPieces * (pieceLen / TORRENT_CHUNK_SIZE) + 7) / 8;
	char* piecemap = new char[piecemapLen];
	char* chunkmap = new char[chunkmapLen];
	memset(piecemap, 0, piecemapLen);
	memset(chunkmap, 0, chunkmapLen);

	{
		unique_lock<mutex> lock(mtx_data);
		for (unsigned int piece = 0; piece < numPieces; piece++) {
			/* We have the full piece if it's not hashing */
			if (!hashingPiece[piece] && havePiece[piece])
				piecemap[piece / 8] |= (1 << (piece % 8));

			/* Add the chunks one by one */
			for (unsigned int chunk = 0; chunk < calculateChunksInPiece(piece); chunk++) {
				int chunkIdx = (piece * (pieceLen / TORRENT_CHUNK_SIZE)) + chunk;
				if (haveChunk[chunkIdx])
					chunkmap[chunkIdx / 8] |= (1 << (chunkIdx % 8));
			}
		}
	}

	status->assign("pieceOK", new MetaString(string(piecemap, piecemapLen)));
	status->assign("chunkOK", new MetaString(string(chunkmap, chunkmapLen)));

	delete[] chunkmap;
	delete[] piecemap;

	return md;
}

bool
Torrent::restoreStatus(const MetaDictionary* status)
{
	const MetaString* pieceOK = dynamic_cast<const MetaString*>((*status)["pieceOK"]);
	const MetaString* chunkOK = dynamic_cast<const MetaString*>((*status)["chunkOK"]);
	if (pieceOK == NULL || chunkOK == NULL)
		return false;

	/*
	 * The required fields of the status-dictionary are present. Perform sanity
	 * checks first, to ensure the information is not bogus.
	 */
	if (pieceOK->getString().size() != ((numPieces + 7) / 8) &&
			chunkOK->getString().size() != ((numPieces * (pieceLen / TORRENT_CHUNK_SIZE) + 7) / 8))
		return false;

	const char* pieces = pieceOK->getString().c_str();
	const char* chunks = chunkOK->getString().c_str();

	/*
	 * This is added paranoia: pieces and chunks that cannot exist in the torrent
	 * yet had to be stored (due to a byte being 8 bits) should be zero. This check
	 * may be overly pedentic.
	 */
	for (unsigned int phantomPiece = numPieces;
	     phantomPiece < (numPieces | 7); phantomPiece++)
		if (pieces[phantomPiece / 8] & (1 << (phantomPiece % 8)))
			return false;
	for (unsigned int phantomChunk = haveChunk.size();
	     phantomChunk < (haveChunk.size() | 7); phantomChunk++)
		if (chunks[phantomChunk / 8] & (1 << (phantomChunk % 8)))
			return false;

	/* Parse all piece/chunk information one by one */
	{
		unique_lock<mutex> lock(mtx_data);
		for (unsigned int piece = 0; piece < numPieces; piece++) {
			if (pieces[piece / 8] & (1 << (piece % 8))) {
				havePiece[piece] = true;

				/* Update the cardinality and left counters */
				pieceCardinality[piece]++;
				if (piece == numPieces - 1) {
					left -= getTotalSize() % pieceLen > 0 ?
									getTotalSize() % pieceLen : pieceLen;
				} else {
					left -= pieceLen;
				}
			}

			for (unsigned int chunk = 0; chunk < calculateChunksInPiece(piece); chunk++) {
				int chunkIdx = (piece * (pieceLen / TORRENT_CHUNK_SIZE)) + chunk;
				if (chunks[chunkIdx / 8] & (1 << (chunkIdx % 8)))
					haveChunk[chunkIdx] = true;
			}
		}
	}

	return true;
}

bool
Torrent::setFilePath(std::string path)
{
	bool ok = true;
	unique_lock<shared_mutex> lock(rwl_files);

	/* Obtain the old root path, in case we have to restore the old path */
	assert(files.size() > 0);
	string old_path = files[0]->getRootPath();

	for (vector<File*>::iterator it = files.begin();
	     it != files.end() && ok; it++) {
		ok = (*it)->moveRootPath(path);
	}

	/*
	 * If 'ok' is not true, some file failed; move 'm all back. Note that we
	 * cannot let go of the lock in between, and we ignore the result code as
	 * this is best-effort.
	 */
	if (!ok) {
		for (vector<File*>::iterator it = files.begin();
				 it != files.end() && ok; it++) {
			(*it)->moveRootPath(old_path);
		}
	}
	return ok;
}

bool
Torrent::constructInfoHash(Metadata* md, uint8_t* hash)
{
	/*
	 * A torrent's 'info' hash is just the SHA1 hash of the
	 * 'info' dictionary. So, first of all, grab the
	 * dictionary.
	 */
	const MetaDictionary* dictionary = md->getDictionary();
	const MetaDictionary* info = dynamic_cast<const MetaDictionary*>((*dictionary)["info"]);
	if (info == NULL)
		return false;

	/* Stream the info-part of the torrent to a buffer... */
	stringbuf sb;
	ostream os(&sb);
	os << *info;

	/* ...and SHA1 that so we get our info hash */
	HashSHA1 sha1;
	sha1.process(sb.str().c_str(), sb.str().size());
 	memcpy(hash, sha1.getHash(), TORRENT_HASH_LEN);
	return true;
}

bool
Torrent::compareTorrentNames(const Torrent* a, const Torrent* b)
{
	string nameA = a->getName();
	string nameB = b->getName();
	transform(nameA.begin(), nameA.end(), nameA.begin(), ::tolower);
	transform(nameB.begin(), nameB.end(), nameB.begin(), ::tolower);
	return nameA < nameB;
}

/* vim:set ts=2 sw=2: */
