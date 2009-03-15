#include <sys/types.h>
#include <assert.h>
#include <fcntl.h>
#include <iostream>
#include <pthread.h>
#include <sstream>
#include <stdint.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include "exceptions.h"
#include "file.h"
#include "hasher.h"
#include "http.h"
#include "peer.h"
#include "sha1.h"
#include "torrent.h"

using namespace std;

#define MIN(a,b) \
	(((a) < (b)) ? (a) : (b))

Torrent::Torrent(Overseer* o, Metadata* md)
{
	overseer = o; downloaded = 0; uploaded = 0; left = 0;
	haveThread = false; terminating = false;

	pthread_mutex_init(&mtx_peers, NULL);

	/* XXX these two variables should be passed from upper hand some day */
	port = 0; peerID = "123456789abcdef01234";

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
	for (int i = 0; i < numPieces; i++) {
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
	for (int i = 0; i < numPieces; i++) {
		for (int j = 0; j < pieceLen / TORRENT_CHUNK_SIZE; j++) {
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

	/* Stream the info-part of the torrent to a buffer... */
	stringbuf sb;
	ostream os(&sb);
	os << *info;

	/* ...and SHA1 that so we get our info hash */
	HashSHA1 sha1;
	sha1.process(sb.str().c_str(), sb.str().size());
 	memcpy(infoHash, sha1.getHash(), sizeof(infoHash));

	/* Summon the hashing thread */
	hasher = new Hasher(this);

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
	bool previousFileReopened;
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

	/*
	 * Nuke all our peers. XXX we should implement signalling and exit more
	 * gracefully.
	 */
	pthread_mutex_lock(&mtx_peers);
	while (true) {
		map<string, Peer*>::iterator it = peers.begin();
		if (it == peers.end())
			break;
		delete (*it).second;
		peers.erase(it);
	}
	pthread_mutex_unlock(&mtx_peers);

	/*
	 * Need to get rid of the hasher first, as it'll crash and burn if we suddenly
	 * nuke the files.
	 */
	delete hasher;

	/* Close all files, too */
	while (true) {
		vector<File*>::iterator it = files.begin();
		if (it == files.end())
			break;
		delete (*it);
		files.erase(it);
	}
}

Metadata*
Torrent::contactTracker(std::string event)
{
	map<string, string> m;

	/* Construct the tracker request, and off it goes */
	string h((const char*)infoHash, sizeof(infoHash));
	m["info_hash"] = h;
	m["peer_id"] = peerID;
	if (event != "")
		m["event"] = event;
	m["downloaded"] = convertInteger(downloaded);
	m["uploaded"] = convertInteger(uploaded);
	m["left"] = convertInteger(left);
	m["port"] = convertInteger(port);
	string result = HTTP::get(announceURL, m);

	/* Parse the result as metadata (which it should be) */
	stringbuf sb(result);
	istream is(&sb);
	Metadata* md = new Metadata(is);

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
Torrent::handleTracker()
{
	Metadata* md = contactTracker();

	MetaList* peerslist = dynamic_cast<MetaList*>((*md->getDictionary())["peers"]);
	if (peerslist != NULL) {
		/*
		 * The tracker has provided us with (possibly new) peers. Add them to the
		 * list. XXX we should limit this to ensure we don't get overflowed.
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

			/* If we already know this peer ID, ignore it */
			if (peers.find(msPeerID->getString()) != peers.end())
				continue;
		
			try {
				Peer* p = new Peer(this, peerID, msPeerID->getString(), msHost->getString(), msPort->getInteger());
				pthread_mutex_lock(&mtx_peers);
				peers[msPeerID->getString()] = p;
				pthread_mutex_unlock(&mtx_peers);
				printf("got peer %p at %s:%u\n", p, msHost->getString().c_str(), msPort->getInteger());
			} catch (ConnectionException e) {
				cerr << "skipping peer: "; cerr << e.what(); cerr << endl;
			}
		}
	}

	delete md;
}

void
Torrent::go()
{
	handleTracker();

	while (!terminating) {
		fd_set fds;

		/* Construct our file descriptor set */
		int maxfd = -1;
		FD_ZERO(&fds);
		pthread_mutex_lock(&mtx_peers);
		for (map<string, Peer*>::iterator it = peers.begin();
		     it != peers.end(); it++) {
			int fd = (*it).second->getFD();
			if (maxfd < fd) maxfd = fd;
			FD_SET(fd, &fds);
		}
		pthread_mutex_unlock(&mtx_peers);

		/*
		 * Sleep for at most 0.5 seconds while waiting for a response; this is to
		 * ensure we won't wait too long while shutting down.
		 *
		 * Note that, for busy torrenta, this 0.5 second loop will never be reached.
		 */
		struct timeval tv;
		tv.tv_sec = 0; tv.tv_usec = 5000;
		int n = select(maxfd + 1, &fds, (fd_set*)NULL, (fd_set*)NULL, &tv);
		if (n == 0)
			continue;

		/* Wade through all peers, handle any data to service */
		bool looping = true;
		while (looping) {
			looping = false;

			/*
			 * The reason for this nested loop is, that calling peers.erase()
			 * will invalidate all iterators for peers. However, if a connection
		 	 * was lost, other connections may need servicing so we must restart
	 	 	 * the loop.
			 */
			for (map<string, Peer*>::iterator it = peers.begin();
					 it != peers.end(); it++) {
				int fd = (*it).second->getFD();
				if (!FD_ISSET(fd, &fds))
					continue;

				/*
				 * There is data here.
				 */
				uint8_t buf[65536 /* XXX */];
				ssize_t len = ::recv(fd, buf, sizeof(buf), 0);
				if (len <= 0) {
					/* socket lost */
					cerr << "connection lost" << endl;
					pthread_mutex_lock(&mtx_peers);
					peers.erase(it);
					pthread_mutex_unlock(&mtx_peers);
					delete (*it).second;
					looping = true;
					break;
				}

				/* Hand the data off to the application */
				if ((*it).second->receive(buf, len) == true) {
					/*
					* Need to server the connection - we decide to nuke the connection, and
					* leave the dirty work to the destructor.
					*/
					pthread_mutex_lock(&mtx_peers);
					peers.erase(it);
					pthread_mutex_unlock(&mtx_peers);
					delete (*it).second;
					printf("need to hang up connection!\n");
					looping = true;
					break;
				}
			}
		}
	}
}

void
Torrent::callbackPiecesAdded(Peer* p, vector<unsigned int>& pieces)
{
	for (vector<unsigned int>::iterator it = pieces.begin();
	     it != pieces.end(); it++) {
		assert(*it < numPieces);
		pieceCardinality[*it]++;
	}

	scheduleRequests();
}

void
Torrent::callbackPiecesRemoved(Peer* p, vector<unsigned int>& pieces)
{
	for (vector<unsigned int>::iterator it = pieces.begin();
	     it != pieces.end(); it++) {
		assert(*it < numPieces);
		assert(pieceCardinality[*it] > 0);
		pieceCardinality[*it]--;
	}
}

void
Torrent::scheduleRequests()
{
	/*
	 * XXX this algorithm should schedule a piece more randomly
	 */
	for (unsigned int i = 0; i < numPieces && !terminating; i++) {
		if (!havePiece[i] && requestedPiece[i] == NULL && pieceCardinality[i] > 0) {
			/*
			 * This piece is of interest! Pick a peer; this is O(|peers|) every time,
			 * but we assume |peers| < 10 so this will be fine for now XXX
			 */
			Peer* p = NULL;
			pthread_mutex_lock(&mtx_peers);
			for (std::map<std::string, Peer*>::iterator peerit = peers.begin();
			     peerit != peers.end(); peerit++)
				if (peerit->second->hasPiece(i)) {
					p = peerit->second; break;
				}
			pthread_mutex_unlock(&mtx_peers);

			assert(p != NULL);
			if (p->getNumRequests() >= TORRENT_PEER_MAX_REQUESTS)
				continue;
			p->claimInterest();
			p->requestPiece(i);
			requestedPiece[i] = p;
			printf("queing request vor piece%i\n", i);
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
Torrent::getMissingChunk(unsigned int piece)
{
	assert (piece < numPieces);

	unsigned int numChunks;
	if (piece == numPieces - 1) {
		numChunks = (total_size % pieceLen) / TORRENT_CHUNK_SIZE;
	} else {
		numChunks = pieceLen / TORRENT_CHUNK_SIZE;
	}
	for (int j = 0; j < calculateChunksInPiece(piece); j++)
		if (!haveChunk[(piece * (pieceLen / TORRENT_CHUNK_SIZE)) + j] &&
		    !haveRequestedChunk[(piece * (pieceLen / TORRENT_CHUNK_SIZE)) + j])
			return j;

	return -1;
}

void
Torrent::callbackCompletePiece(Peer* p, unsigned int piece)
{
	assert (piece < numPieces);

	cout << "Torrent::callbackCompletePiece(): piece = "; cout << piece; cout << endl;
	havePiece[piece] = true;

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

	haveChunk[(piece * (pieceLen / TORRENT_CHUNK_SIZE)) + offset / TORRENT_CHUNK_SIZE] = true;
	writeChunk(piece, offset, data, len);

	/* See if we have all chunks; if so, the piece is in */
	for (unsigned int i = 0; i < calculateChunksInPiece(piece); i++)
		if (!haveChunk[(piece * (pieceLen / TORRENT_CHUNK_SIZE)) + i])
			return;

	/* Yay! */
	callbackCompletePiece(p, piece);
}

void
Torrent::setChunkRequested(unsigned int piece, unsigned int chunk, bool requested)
{
	assert (piece < numPieces);
	assert (chunk < pieceLen / TORRENT_CHUNK_SIZE);

	haveRequestedChunk[(piece * (pieceLen / TORRENT_CHUNK_SIZE)) + chunk] = requested;
}

void
Torrent::dump()
{
	printf("piece map\n");
	for (unsigned int i = 0; i < numPieces; i++) {
		if (i % 50 == 0) {
				printf("\n");
				printf ("% 4u:", i);
		}
		if (i % 10 == 0) printf(" ");

		if (havePiece[i]) {
			printf("%c", hashingPiece[i] ? "?" : "#");
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
}

bool
Torrent::hasPiece(unsigned int piece)
{
	assert (piece < numPieces);
	return havePiece[piece];
}

void
Torrent::callbackCompleteHashing(unsigned int piece, bool result)
{
	printf("completed hashing of piece %u: valid=%c\n",
	 piece, result ? 'Y' : 'N');

	hashingPiece[piece] = false;
	if (!result) {
		/*
		 * We got a corrupted piece! Mark it as not-available and attempt to
		 * request it again from someone else. XXX we should identify and ban
		 * seeders that give is bad content
		 */
		havePiece[piece] = false;
		scheduleRequests();
		return;
	}

	/* If we have all pieces, rejoice */
	for (unsigned int i = 0; i < numPieces; i++)
		if (!havePiece[i] || hashingPiece[i])
			return;

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
}

void
Torrent::scheduleHashing(unsigned int piece)
{
	assert(piece < numPieces);
	assert(!hashingPiece[piece]);

	hasher->addPiece(piece);
	hashingPiece[piece] = true;
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
	pthread_mutex_lock(&mtx_peers);
	/* XXX do stuff */
	pthread_mutex_unlock(&mtx_peers);
}

/* vim:set ts=2 sw=2: */
