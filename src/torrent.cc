#include <sys/types.h>
#include <assert.h>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <stdint.h>
#include <sys/select.h>
#include <sys/socket.h>
#include "exceptions.h"
#include "http.h"
#include "peer.h"
#include "sha1.h"
#include "torrent.h"

using namespace std;

Torrent::Torrent(Metadata* md)
{
	downloaded = 0; uploaded = 0; left = 0;

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
	pieceHash.reserve(numPieces);
	for (int i = 0; i < numPieces; i++) {
		pieceHash.push_back(miPieces->getString().substr(i * TORRENT_HASH_LEN, TORRENT_HASH_LEN));
	}

	/* For now, assume we have no pieces and requested none */
	havePiece.reserve(numPieces);
	requestedPiece.reserve(numPieces);
	pieceCardinality.reserve(numPieces);
	for (int i = 0; i < numPieces; i++) {
		havePiece.push_back(false);
		requestedPiece.push_back(NULL);
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

	/* Construct the SHA1 hash of the 'info' dictionary  */
	stringbuf sb;
	ostream os(&sb); istream is(&sb);
	HashSHA1 sha1(is);
	os << *info;
 	infoHash = sha1.getHash();

	fd = creat("out.put", 0644);
	lseek(fd, pieceLen * numPieces - 1, SEEK_SET);
	uint8_t b = 0xff;
	write(fd, &b, 1);
}

Metadata*
Torrent::contactTracker(std::string event)
{
	map<string, string> m;

	/* Construct the tracker request, and off it goes */
	m["info_hash"] = infoHash;
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
				peers[msPeerID->getString()] = p;
				printf("got peer %p\n", p);
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

	while (true) {
		fd_set fds;

		/* Construct our file descriptor set */
		int maxfd = -1;
		FD_ZERO(&fds);
		for (map<string, Peer*>::iterator it = peers.begin();
		     it != peers.end(); it++) {
			int fd = (*it).second->getFD();
			if (maxfd < fd) maxfd = fd;
			FD_SET(fd, &fds);
		}

		int n = select(maxfd + 1, &fds, (fd_set*)NULL, (fd_set*)NULL, NULL);

		/* Wade through all peers, handle any data to service */
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
				peers.erase(it);
				delete (*it).second;
				continue;
			}
printf("got %u bytes\n", len);

			/* Hand the data off to the application */
			if ((*it).second->receive(buf, len) == true) {
				/*
				* Need to server the connection - we decide to nuke the connection, and
				* leave the dirty work to the destructor.
				*/
				peers.erase(it);
				delete (*it).second;
				printf("need to hang up connection!\n");
				continue;
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
	for (unsigned int i = 0; i < numPieces; i++) {
		if (!havePiece[i] && requestedPiece[i] == NULL && pieceCardinality[i] > 0) {
			/*
			 * This piece is of interest! Pick a peer; this is O(|peers|) every time,
			 * but we assume |peers| < 10 so this will be fine for now XXX
			 */
			Peer* p = NULL;
			for (std::map<std::string, Peer*>::iterator peerit = peers.begin();
			     peerit != peers.end(); peerit++)
				if (peerit->second->hasPiece(i)) {
					p = peerit->second; break;
				}

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

Torrent::~Torrent()
{
	/*
	 * Nuke all our peers. XXX we should implement signalling and exit more
	 * gracefully.
	 */
	for (map<string, Peer*>::iterator it = peers.begin();
	     it != peers.end(); it++) {
		delete (*it).second;
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

	for (int j = 0; j < pieceLen / TORRENT_CHUNK_SIZE; j++)
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

	/* Try more! */
	scheduleRequests();
}

void
Torrent::callbackCompleteChunk(Peer* p, unsigned int piece, unsigned int chunk, const uint8_t* data, uint32_t len)
{
	assert (piece < numPieces);
	assert (chunk < pieceLen / TORRENT_CHUNK_SIZE);

	haveChunk[(piece * (pieceLen / TORRENT_CHUNK_SIZE)) + chunk] = true;

	off_t o = ((off_t)piece * (off_t)pieceLen) + ((off_t)chunk * (off_t)TORRENT_CHUNK_SIZE);
	printf("piece %u chunk %u => %lu\n", piece, chunk, o);
	lseek(fd, o, SEEK_SET);
	if (write(fd, data, len) != len)
		printf("fail\n");
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
			printf("#");
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

/* vim:set ts=2 sw=2: */
