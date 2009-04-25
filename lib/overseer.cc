#include <sys/select.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "overseer.h"
#include "peer.h"
#include "tracer.h"

using namespace std;

#define TRACER (tracer)
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

Overseer::Overseer(unsigned int portnum, Tracer* tr)
{
	terminating = false; port = portnum; tracer = tr;
	upload_rate = 0;

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

	/* Initialize the mutex; it's being used by the sender later on */
	pthread_mutex_init(&mtx_torrents, NULL);

	hasher = new Hasher(this);
	sender = new Sender(this);
	incoming = new Connection(port);

	/* Block SIGPIPE - the appropriate thread will notice this anyway */
	sigset_t sm;
	sigemptyset(&sm);
	sigaddset(&sm, SIGPIPE);
	pthread_sigmask(SIG_BLOCK, &sm, NULL);

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

	/* Get rid of the torrents; these will remove any peers and hashing requests too */
	while (true) {
		map<string, Torrent*>::iterator it = torrents.begin();
		if (it == torrents.end())
			break;
		delete it->second;
		torrents.erase(it);
	}

	delete incoming;
	delete hasher;
	delete sender;

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
Overseer::removeTorrent(Torrent* t)
{
	string info((const char*)t->getInfoHash(), TORRENT_HASH_LEN);

	pthread_mutex_lock(&mtx_torrents);
	map<string, Torrent*>::iterator it = torrents.find(info);
	if (it != torrents.end()) {
		delete it->second;
		torrents.erase(it);
	}
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

		/* Replenish amount of data transferrable */
		sender->setAmountTransferrable(upload_rate);

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
		if (select(fd + 1, &fds, NULL, NULL, &tv) == 0 || !FD_ISSET(fd, &fds) || terminating)
			continue;

		/*
		 * Incoming connection! Note that we wait here at most 5 seconds until data
		 * arrives; if we haven't seen anything, we assume the client is broken or
		 * just not interesting. The specification states that 'The initiator of a
		 * connection is expected to transmit their handshake immediately', so
		 * 5 seconds seems reasonable.
		 */
		Connection* c = incoming->acceptConnection();
		if (c == NULL)
			continue;
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
		int num_hashing = 0, hashing_pieces = 0;
		for (map<string, Torrent*>::iterator it = torrents.begin();
		     it != torrents.end(); it++) {
			Torrent* t = it->second;
			unsigned int n = t->getNumPiecesHashing();
			if (n > 0) {
				hashing_pieces += n;
				num_hashing++;
			}
		}
		pthread_mutex_unlock(&mtx_torrents);

		if (!num_hashing)
			break;

#if 0
		printf("Overseer: waiting for %u torrent(s) to finish hashing %u pieces\n", num_hashing, hashing_pieces);
#endif
		sleep(1);
	}
}

vector<Torrent*>
Overseer::getTorrents()
{
	vector<Torrent*> l;

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
	 * Attempt to grab part one of the handshake; this consists of everything but
	 * the peer id.
	 */
	uint8_t handshake[1024 /* XXX */];
	c->read((void*)handshake, 1 + strlen(PEER_PSTR) + 8 + TORRENT_HASH_LEN, true);
	TRACE(NETWORK, "got handshake (part 1): connection=%p", c);
	
	/* So, we have part one of the handshake; dissect and validate it */
	uint8_t reserved[8];
	memcpy(reserved, (const char*)(handshake + 1 + strlen(PEER_PSTR)), 8);
	string info((const char*)(handshake + 1 + strlen(PEER_PSTR) + 8), TORRENT_HASH_LEN);
	if (handshake[0] != strlen(PEER_PSTR) ||
			memcmp((handshake + 1), PEER_PSTR, strlen(PEER_PSTR))) {
		TRACE(NETWORK, "got bad protocol version from connection=%p, dropping!", c);
		delete c;
		return;
	}

	/* Find the torrent that belongs to this info hash */
	pthread_mutex_lock(&mtx_torrents);
	Torrent* t = NULL;
	map<string, Torrent*>::iterator it = torrents.find(info);
	if (it != torrents.end())
		t = it->second;
	pthread_mutex_unlock(&mtx_torrents);
	if (t == NULL) {
		TRACE(TORRENT, "connection %p: peer requests unknown info hash, dropping", c);
		delete c;
		return;
	}

	/*
	 * OK, we have a handshake and we know the torrent. This means we can
	 * accept the torrent, which we hereby do. Due to possible NAT
	 * checking, it may be that we won't get the peer ID until after we
	 * send our own handshake (the Peer constructor does this!) so do this and
	 * then grab the peer ID (Why is this implemented this way?)
	 */
	Peer* p = new Peer(t, c);

	/*
	 * We sent our stuff; wait for the final 20 bytes indicating the torrent's peer ID.
	 */
	c->read((void*)handshake, TORRENT_PEERID_LEN);
	TRACE(NETWORK, "got handshake (part 2): connection=%p", p);
	string peer((const char*)(handshake), TORRENT_PEERID_LEN);
	if (!memcmp(handshake, peerid, TORRENT_PEERID_LEN)) {
		TRACE(NETWORK, "handshake aborted: connection=%p,peer=%s is us", c, c->getEndpoint().c_str());
		delete p;
		return;
	}
	p->setPeerID(peer);
	TRACE(NETWORK, "handshake completed: connection=%p,peer=%s", c, p->getID().c_str());

	/* We accept! We have no choice! */
	t->addPeer(p);
	TRACE(NETWORK, "accepted peer %p for torrent %p",
	 c, t);
}

void
Overseer::queueHashPiece(Torrent* t, uint32_t piece)
{
	hasher->addPiece(t, piece);
}

void
Overseer::cancelHashingTorrent(Torrent* t)
{
	hasher->cancelTorrent(t);
}

void
Overseer::getSendablePeers(map<int, Peer*>& m)
{
	pthread_mutex_lock(&mtx_torrents);
	for (map<string, Torrent*>::iterator it = torrents.begin();
	     it != torrents.end(); it++) {
		Torrent* t = it->second;
		t->getSendablePeers(m);
	}
	pthread_mutex_unlock(&mtx_torrents);
}

void
Overseer::signalSender()
{
	sender->signal();
}

/* vim:set ts=2 sw=2: */
