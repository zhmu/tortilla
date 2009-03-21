#include <sys/select.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "overseer.h"
#include "peer.h"

using namespace std;

void*
bandwidth_thread(void* ptr)
{
	((Overseer*)ptr)->bandwidthThread();
	return NULL;
}

void*
listener_thread(void* ptr)
{
	((Overseer*)ptr)->listenerThread();
	return NULL;
}

Overseer::Overseer(unsigned int portnum)
{
	terminating = false; port = portnum;

	/*
	 * Construct our peer ID; we do this in Azureus style and hereby claim the
	 * identifier 'Ta' for Tortilla.
	 */
	peerid[0] = '-';
	peerid[1] = 'T'; peerid[2] = 'a';
	for (int i = 0; i < 4; i++)
		peerid[3 + i] = '0'; /* XXX version */
	for (int i = 7; i < TORRENT_PEERID_LEN; i++)
		peerid[i] = rand() % 26 + 'a';

	uploader = new Uploader();
	incoming = new Connection(port);

	pthread_mutex_init(&mtx_torrents, NULL);
	pthread_create(&thread_bandwidth_monitor, NULL, bandwidth_thread, this);
	pthread_create(&thread_listener, NULL, listener_thread, this);
}

Overseer::~Overseer()
{
	terminating = true;

	/*
	 * Remove the uploader first; we don't want it to die because all its peers
	 * are dying
	 */
	delete uploader;

	while (true) {
		map<string, Torrent*>::iterator it = torrents.begin();
		if (it == torrents.end())
			break;
		delete it->second;
		torrents.erase(it);
	}

	pthread_join(thread_bandwidth_monitor, NULL);
	pthread_join(thread_listener, NULL);
	pthread_mutex_destroy(&mtx_torrents);
}

void
Overseer::addTorrent(Torrent* t)
{
	string info((const char*)t->getInfoHash(), TORRENT_HASH_LEN);
	pthread_mutex_lock(&mtx_torrents);
	torrents[info] = t;
	pthread_mutex_unlock(&mtx_torrents);
}

void
Overseer::start()
{
	/* Launch all torrents */
	pthread_mutex_lock(&mtx_torrents);
	for (map<string, Torrent*>::iterator it = torrents.begin();
	     it != torrents.end(); it++) {
		Torrent* t = it->second;
		t->start();
	}
	pthread_mutex_unlock(&mtx_torrents);
}

void
Overseer::stop()
{
}
void
Overseer::terminate()
{
	terminating = true;
}

void
Overseer::bandwidthThread()
{
	while (!terminating) {
		/* Wait for a second */
		struct timeval tv;
		tv.tv_sec = 1; tv.tv_usec = 0;
		select(0, NULL, NULL, NULL, &tv);

		/* Ask all torrents to update their bandwidth usage */
		pthread_mutex_lock(&mtx_torrents);
		for (map<string, Torrent*>::iterator it = torrents.begin();
				 it != torrents.end(); it++) {
			Torrent* t = it->second;
			t->updateBandwidth();
		}
		pthread_mutex_unlock(&mtx_torrents);
	}
}

void
Overseer::listenerThread()
{
	fd_set fds;

	while (!terminating) {
		/*
		 * Wait for at most second for a request; this means we stall at most
		 * one second while terminating.
		 */
		struct timeval tv;
		tv.tv_sec = 1; tv.tv_usec = 0;

		int fd = incoming->getFD();
		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		if (select(fd + 1, &fds, NULL, NULL, &tv) == 0 || !FD_ISSET(fd, &fds))
			continue;

		/*
		 * Incoming connection! Note that we wait here at most 5 seconds until data
		 * arrives; if we haven't seen anything, we assume the client is broken or
		 * just not interesting. The specification states that 'The initiator of a
		 * connection is expected to transmit their handshake immediately', so
		 * 5 seconds seems reasonable.
		 */
		Connection* c = incoming->acceptConnection();
		tv.tv_sec = 5; tv.tv_usec = 0;
		fd = c->getFD();
		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		if (select(fd + 1, &fds, NULL, NULL, &tv) == 0 || !FD_ISSET(fd, &fds)) {
			/* No data! So sad... */
			delete c;
			continue;
		}

		/*
		 * Attempt to grab the handshake in one go. XXX this will break if there
		 * ever exists protocol 2.0...
	 	 *
	 	 * XXX is this really the right place to do this?
		 */
		uint32_t handshake_len = 49 + strlen(PEER_PSTR);
		uint8_t* handshake = new uint8_t[handshake_len];
		if (::read(fd, handshake, handshake_len) != handshake_len) {
			/* Ugh, not complete... XXX now what? */
			cerr << "got incomplete handshake from peer, dropping!" << endl;
			delete[] handshake;
			delete c;
			continue;
		}

		/* So, we have a handshake; validate it */
		if (handshake[0] != strlen(PEER_PSTR) ||
			  memcmp((handshake + 1), PEER_PSTR, strlen(PEER_PSTR))) {
			cerr << "got bad protocol version, dropping!" << endl;
			delete[] handshake;
			delete c;
			continue;
		}

		/* Grab all values from the handshake and throw the handshake data away */
		uint8_t reserved[8];
		memcpy(reserved, (const char*)(handshake + 1 + strlen(PEER_PSTR)), 8);
		string info((const char*)(handshake + 1 + strlen(PEER_PSTR) + 8), TORRENT_HASH_LEN);
		string peer((const char*)(handshake + 1 + strlen(PEER_PSTR) + 8 + TORRENT_HASH_LEN), TORRENT_PEERID_LEN);
		delete[] handshake;

		/* Find the torrent that belongs to this info hash */
		pthread_mutex_lock(&mtx_torrents);
		Torrent* t = NULL;
		map<string, Torrent*>::iterator it = torrents.find(info);
		if (it != torrents.end())
			t = it->second;
		pthread_mutex_unlock(&mtx_torrents);
		if (t == NULL) {
			cerr << "got connection for unknown info hash, dropping!" << endl;
			delete c;
			continue;
		}

		/* We accept! We have no choice! */
		t->addIncomingPeer(c, peer, reserved);
	}
}

void
Overseer::waitHashingComplete()
{
	while (!terminating) {
		/* Count the number of active hashers */
		pthread_mutex_lock(&mtx_torrents);
		int num_hashing = 0;
		for (map<string, Torrent*>::iterator it = torrents.begin();
		     it != torrents.end(); it++) {
			Torrent* t = it->second;
			if (t->isHashing())
				num_hashing++;
		}
		pthread_mutex_unlock(&mtx_torrents);

		if (!num_hashing)
			break;

		printf("Overseer: waiting for %u torrent(s) to finish hashing\n", num_hashing);
		sleep(1);
	}
}

void
Overseer::queueUploadRequest(Peer* p, uint32_t piece, uint32_t begin, uint32_t len)
{
	uploader->enqueue(p, piece, begin, len);
}

void
Overseer::dequeueUploadRequest(Peer* p, uint32_t piece, uint32_t begin, uint32_t len)
{
	uploader->dequeue(p, piece, begin, len);
}

void
Overseer::dequeuePeer(Peer* p)
{
	uploader->removeRequestsFromPeer(p);
}

list<Torrent*>
Overseer::getTorrents()
{
	list<Torrent*> l;

	pthread_mutex_lock(&mtx_torrents);
	for (map<string, Torrent*>::iterator it = torrents.begin();
			 it != torrents.end(); it++) {
		l.push_back(it->second);
	}
	pthread_mutex_unlock(&mtx_torrents);

	return l;
}

/* vim:set ts=2 sw=2: */
