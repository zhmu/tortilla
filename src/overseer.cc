#include <sys/select.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "overseer.h"
#include "peer.h"
#include "tracer.h"

using namespace std;

extern std::string formatHex(const uint8_t* hex, unsigned int len);

#define OVERSEER_THREAD(x) \
void* \
x ## _thread(void* ptr) \
{ \
	((Overseer*)ptr)->x ## Thread(); \
	return NULL; \
}

OVERSEER_THREAD(bandwidth);
OVERSEER_THREAD(listener);
OVERSEER_THREAD(heartbeat);

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
	pthread_create(&thread_heartbeat, NULL, heartbeat_thread, this);
}

Overseer::~Overseer()
{
	terminating = true;

	/* Remove all helper threads */
	pthread_join(thread_bandwidth_monitor, NULL);
	pthread_join(thread_listener, NULL);
	pthread_join(thread_heartbeat, NULL);

	while (true) {
		map<string, Torrent*>::iterator it = torrents.begin();
		if (it == torrents.end())
			break;
		delete it->second;
		torrents.erase(it);
	}

	delete uploader;

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
	while (!terminating) {
		/*
		 * Wait for at most second for a request; this means we stall at most
		 * one second while terminating.
		 */
		struct timeval tv;
		tv.tv_sec = 1; tv.tv_usec = 0;

		fd_set fds;
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
		TRACE(NETWORK, "accepted: connection=%p, fd=%u", c, c->getFD());

		handleIncomingConnection(c);
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

void
Overseer::heartbeatThread()
{
	while (!terminating) {
		/* Wait for a second */
		struct timeval tv;
		tv.tv_sec = 1; tv.tv_usec = 0;
		select(0, NULL, NULL, NULL, &tv);

		/*
		 * Tell all torrents to heartbeat. The reason this is done in a seperate
		 * thread is because the heartbeat may stall.
		 */
		pthread_mutex_lock(&mtx_torrents);
		for (map<string, Torrent*>::iterator it = torrents.begin();
				 it != torrents.end(); it++) {
			Torrent* t = it->second;
			pthread_mutex_unlock(&mtx_torrents);
			t->heartbeat();
			pthread_mutex_lock(&mtx_torrents);
		}
		pthread_mutex_unlock(&mtx_torrents);
	}
}

void
Overseer::handleIncomingConnection(Connection* c)
{
	fd_set fds;
	struct timeval tv;
	int fd = c->getFD();

	tv.tv_sec = 5; tv.tv_usec = 0;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	if (select(fd + 1, &fds, NULL, NULL, &tv) <= 0 || !FD_ISSET(fd, &fds)) {
		/* No data! So sad... */
		TRACE(NETWORK, "timeout waiting for handshake: connection=%p", c);
		delete c;
		return;
	}

	/*
	 * Attempt to grab the handshake in one go. XXX this will break if there
	 * ever exists protocol 2.0...
	 *
	 * XXX is this really the right place to do this?
	 */
	uint8_t handshake[2000 /* XXX */];

	size_t left = 68; /* XXX */
	size_t got = 0;

	char* ptr = (char*)handshake;
	while (left > 0) {
		TRACE(NETWORK, "reading handshake: connection=%p, fd=%u, got=%u,left=%u", c, fd, got, left);
		ssize_t l = ::read(fd, ptr, 1);
		TRACE(NETWORK, "reading handshake: l=%i", l);
		if (l <= 0) {
			/* Ugh, not complete... XXX now what? */
		perror("read");
			cerr << "got incomplete handshake from peer, dropping!" << endl;
			delete c;
			return;
		}
		ptr += l; got += l; left -= l;
	}

	char msg[1024];
	sprintf(msg, "got handshake: connection=%p, got=%lu, left=%lu, data=", c, got, left);
	for(unsigned int z = 0; z < got; z++) {
		char tmp[32];
		sprintf(tmp , " %02x(%c)", handshake[z], handshake[z]);
		strcat(msg, tmp);
	}
	TRACE(NETWORK, msg);
	
	/* So, we have a handshake; validate it */
	if (handshake[0] != strlen(PEER_PSTR) ||
			memcmp((handshake + 1), PEER_PSTR, strlen(PEER_PSTR))) {
		TRACE(NETWORK, "got bad protocol version, dropping!");
		delete c;
		return;
	}

	/* Grab all values from the handshake and throw the handshake data away */
	uint8_t reserved[8];
	memcpy(reserved, (const char*)(handshake + 1 + strlen(PEER_PSTR)), 8);
	string info((const char*)(handshake + 1 + strlen(PEER_PSTR) + 8), TORRENT_HASH_LEN);
	string peer((const char*)(handshake + 1 + strlen(PEER_PSTR) + 8 + TORRENT_HASH_LEN), TORRENT_PEERID_LEN);

	string sInfo = formatHex((uint8_t*)info.c_str(), TORRENT_HASH_LEN);
	string sPeer = formatHex((uint8_t*)peer.c_str(), TORRENT_PEERID_LEN);
	TRACE(NETWORK, "got handshake: infohash='%s',peer='%s'", sInfo.c_str(), sPeer.c_str());

	/* Find the torrent that belongs to this info hash */
	pthread_mutex_lock(&mtx_torrents);
	Torrent* t = NULL;
	map<string, Torrent*>::iterator it = torrents.find(info);
	if (it != torrents.end())
		t = it->second;
	pthread_mutex_unlock(&mtx_torrents);
	if (t == NULL) {
		TRACE(TORRENT, "connection %p: peer '%s' requests unknown info hash '%s', dropping", c, sPeer.c_str(), sInfo.c_str());
		delete c;
		return;
	}

	/* We accept! We have no choice! */
	TRACE(NETWORK, "accepted peer id '%s' (%p) for torrent hash '%s' (%p)",
	 sPeer.c_str(), c, sInfo.c_str(), t);
	t->addIncomingPeer(c, peer, reserved);
}

/* vim:set ts=2 sw=2: */
