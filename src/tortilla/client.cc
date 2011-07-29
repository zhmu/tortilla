#include <iostream>
#include <fstream>
#include <sstream>
#include "tortilla/exceptions.h"
#include "tortilla/overseer.h"
#include "tortilla/tracer.h"
#include "tortilla/torrent.h"
#include "tortilla/callbacks.h"
#include "client.h"
#include "torrentinfo.h"
#include "interface.h"

Client::Client(int port)
{
	tracer = new Tortilla::Tracer();
	overseer = new Tortilla::Overseer(port, tracer, this);
	interface = new Interface(this);
}

Client::~Client()
{
	delete interface;
	delete overseer;
	delete tracer;
}

int
Client::getUploadRate() const
{
	return overseer->getUploadRate();
}

void
Client::setUploadRate(int upload)
{
	overseer->setUploadRate(upload * 1024);
}

void
Client::run()
{
	interface->run();
}

void
Client::addTorrent(std::string filename)
{
	std::ifstream is;
	is.open(filename.c_str(), std::ios::binary);
	Tortilla::Metadata* md = new Tortilla::Metadata(is);

	/*
	 * Grab the torrent's info hash - we use it to figure out whether the torrent
	 * is already added (without this, we would just overwrite the previous
	 * torrent into oblivion)
	 */
	uint8_t infohash[TORRENT_HASH_LEN];
	if (!Tortilla::Torrent::constructInfoHash(md, infohash)) {
		/* We couldn't build a hash; this means the torrent won't get far either */
		delete md;
		throw Tortilla::TorrentException("Cannot generate info hash of source file (corrupt .torrent?)");
	}
	if (overseer->findTorrent(infohash) != NULL) {
		delete md;
		throw Tortilla::TorrentException("Torrent already added");
	}

	try {
		overseer->addTorrent(new Tortilla::Torrent(overseer, md, ""));
	} catch (...) {
		/* Prevent memory leak */
		delete md;
		throw;
	}
	delete md;
}

bool
Client::isTerminating() const
{
	return overseer->isTerminating();
}

void
Client::handleResize()
{
	interface->handleResize();
}

void
Client::terminate()
{
	overseer->terminate();
}

void
Client::removeTorrent(TorrentInfo* ti)
{
	overseer->removeTorrent(ti->getTorrent());
}

void
Client::gotTrackerReply(Tortilla::Torrent* t, int newPeers, std::string message)
{
	TorrentInfo* ti = (TorrentInfo*)t->user_ptr;
	ti->num_pending_peers += newPeers;
}

void
Client::completedPiece(Tortilla::Torrent* t, int piece)
{
	/*
	 * XXX Note that this callback can be called before the torrent is fully set
	 *     up; we loop if this happens.
	 */
	TorrentInfo* ti = (TorrentInfo*)t->user_ptr;
	while (ti == NULL) {
		struct timespec ts;
		ts.tv_sec = 0;
		ts.tv_nsec = 1000000; /* 1 ms */
		nanosleep(&ts, NULL);
		ti = (TorrentInfo*)t->user_ptr;
	}
	ti->num_pieces_completed++;
}

void
Client::completedTorrent(Tortilla::Torrent* t)
{
}

void
Client::addedTorrent(Tortilla::Torrent* t)
{
	/* XXX lock */
	TorrentInfo* ti = new TorrentInfo(t);
	t->user_ptr = (void*)ti;
	torrents.push_back(ti);
}

void
Client::removingTorrent(Tortilla::Torrent* t)
{
	/* XXX lock */
	for (TorrentInfoVector::iterator it = torrents.begin(); it != torrents.end(); it++) {
		TorrentInfo* ti = *it;
		if (ti->getTorrent() == t) {
			assert(ti == t->user_ptr);
			t->user_ptr = NULL;
			torrents.erase(it);
			return;
		}
	}
}

void
Client::addedPeer(Tortilla::Torrent* t, Tortilla::Peer* p)
{
	TorrentInfo* ti = (TorrentInfo*)t->user_ptr;
	ti->num_peers++;
}

void
Client::removingPeer(Tortilla::Torrent* t, Tortilla::Peer* p)
{
	TorrentInfo* ti = (TorrentInfo*)t->user_ptr;
	assert(ti->num_peers > 0);
	ti->num_peers--;
}

/* vim:set ts=2 sw=2: */
