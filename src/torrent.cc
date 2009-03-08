#include <sys/types.h>
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

	pieceLen = miPieceLength->getInteger();
	numPieces = miPieces->getString().size() / TORRENT_HASH_LEN;
	pieceHash.reserve(numPieces);
	for (int i = 0; i < numPieces; i++) {
		pieceHash.push_back(miPieces->getString().substr(i * TORRENT_HASH_LEN, TORRENT_HASH_LEN));
	}

	/* Construct the SHA1 hash of the 'info' dictionary  */
	stringbuf sb;
	ostream os(&sb); istream is(&sb);
	HashSHA1 sha1(is);
	os << *info;
 	infoHash = sha1.getHash();
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
		
			peers[msPeerID->getString()] = new Peer(this, peerID, msPeerID->getString(), msHost->getString(), msPort->getInteger());
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
			char buf[65536 /* XXX */];
			ssize_t len = ::recv(fd, buf, sizeof(buf), 0);
			if (len <= 0)
				/* XXX cleanup socket */
				break;

			/* Hand the data off to the application */
			string s;
			s.append(buf, len);
			if ((*it).second->receive(s) == true) {
				/*
				* Need to server the connection - we decide to nuke the connection, and
				* leave the dirty work to the destructor.
				*/
				peers.erase(it);
				delete (*it).second;
				continue;
			}
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

/* vim:set ts=2 sw=2: */
