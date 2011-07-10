#include <boost/thread/locks.hpp>
#include <sys/select.h>
#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "callbacks.h"
#include "filemanager.h"
#include "receiver.h"
#include "macros.h"
#include "overseer.h"
#include "httprequest.h"
#include "peer.h"
#include "tracer.h"

using namespace std;
using namespace boost;

#define TRACER (tracer)

void*
overseer_thread(void* ptr)
{
	((Overseer*)ptr)->overseerThread();
	return NULL;
}

Overseer::Overseer(unsigned int portnum, Tracer* tr, Callbacks* cb)
{
	terminating = false; port = portnum; tracer = tr;
	if (cb == NULL)
		callbacks = new Callbacks();
	else
		callbacks = cb;
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

	incoming = new Connection(port);
	receiver = new Receiver(this);
	hasher = new Hasher(this);
	sender = new Sender(this);
	filemanager = new FileManager(this, 64 /* XXX make me configurable! */);

	/* Block SIGPIPE - the appropriate thread will notice this anyway */
	sigset_t sm;
	sigemptyset(&sm);
	sigaddset(&sm, SIGPIPE);
	pthread_sigmask(SIG_BLOCK, &sm, NULL);

	thread = new boost::thread(overseer_thread, this);
}

Overseer::~Overseer()
{
	terminating = true;

	/* Ask all torrents to shutdown; if we haven't done so already */
	for(map<string, Torrent*>::iterator it = torrents.begin();
	    it != torrents.end(); it++) {
		Torrent* t =  it->second;
		if (!t->isTerminating())
			t->shutdown();
	}

	/* Remove the overseer thread */
	thread->join();
	delete thread;

	/* The overseer thread should have removed all torrents by now */
	assert(torrents.size() == 0);

	delete hasher;
	delete sender;
	delete receiver;
	delete incoming;
	delete filemanager;
	delete callbacks;
}

void
Overseer::addTorrent(Torrent* t)
{
	string info((const char*)t->getInfoHash(), TORRENT_HASH_LEN);
	shared_lock<shared_mutex> lock(rwl_torrents);
	torrents[info] = t;
}

void
Overseer::removeTorrent(Torrent* t)
{
	string info((const char*)t->getInfoHash(), TORRENT_HASH_LEN);

	shared_lock<shared_mutex> lock(rwl_torrents);
	map<string, Torrent*>::iterator it = torrents.find(info);
	if (it != torrents.end()) {
		/*
		 * Ask the torrent politely to shutdown; it'll chat with the
		 * tracker and everything. We give the torrent some time to
		 * finish up communication, after which we will just blast it
		 * away. This is handled by the overseer thread.
		 */
		Torrent* t = it->second;
		if (!t->isTerminating())
			t->shutdown();
	}
}

void
Overseer::terminate()
{
	terminating = true;

	/* Request all torrents to shutdown */
	{
		shared_lock<shared_mutex> lock(rwl_torrents);
		for (map<string, Torrent*>::iterator it = torrents.begin();
				 it != torrents.end(); it++) {
			it->second->shutdown();
		}
	}
}

vector<Torrent*>
Overseer::getTorrents()
{
	vector<Torrent*> l;

	{
		shared_lock<shared_mutex> lock(rwl_torrents);
		for (map<string, Torrent*>::iterator it = torrents.begin();
				 it != torrents.end(); it++) {
			l.push_back(it->second);
		}
	}

	return l;
}

void
Overseer::overseerThread()
{
	while (true) {
		/* Wait for a second */
		struct timeval tv;
		tv.tv_sec = 1; tv.tv_usec = 0;
		select(0, NULL, NULL, NULL, &tv);

		/* Replenish amount of data transferrable */
		sender->setAmountTransferrable(upload_rate);

		/*
		 * Tell all torrents to heartbeat and update their
	 	 * bandwidth usage. This implies heartbeat() may not
		 * block or stall.
		 */
		int numTerminating = 0;
		{
			shared_lock<shared_mutex> lock(rwl_torrents);
			for (map<string, Torrent*>::iterator it = torrents.begin();
					 it != torrents.end(); it++) {
				Torrent* t = it->second;
				if (!t->isTerminating()) {
					t->updateBandwidth();
					t->heartbeat();
				} else {
					numTerminating++;
				}
			}
		}

		/*
		 * If we are scheduled for termination, yet no torrents need to terminate,
		 * we have removed all of them and can happily terminate.
		 */
		if (terminating && !numTerminating) {
			assert(torrents.size() == 0);
			break;
		}

		if (!numTerminating)
			continue;

		/*
		 * We have terminating torrents; monitor their status. If they
		 * take too much time, we just kill them off.
		 */
		time_t now = time(NULL);
		{
			unique_lock<shared_mutex> lock(rwl_torrents);
			map<string, Torrent*>::iterator it = torrents.begin();
			while (it != torrents.end()) {
				Torrent* t = it->second;
				if (t->isTerminating() &&
					 (t->canBeDeleted() || t->getTerminationTime() + OVERSEER_TORRENT_SHUTDOWN_TIMEOUT < now)) {
					/*
					 * Either the torrent wants to be removed, or it has run out of time.
					 * Run the callback first, */
					callbacks->removingTorrent(t);
					delete t;
					torrents.erase(it);
					it = torrents.begin();
				} else {
					it++;
					continue;
				}
			}
		}
	}
}

void
Overseer::handleIncomingConnection(Connection* c)
{
	fd_set fds;
	struct timeval tv;
	int fd = c->getFD();

	tv.tv_sec = 3; tv.tv_usec = 0;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	if (select(fd + 1, &fds, NULL, NULL, &tv) <= 0 || !FD_ISSET(fd, &fds)) {
		/* No data! So sad... */
		TRACE(NETWORK, "timeout waiting for handshake from %s", c->getEndpoint().c_str());
		delete c;
		return;
	}

	/*
	 * Attempt to grab part one of the handshake; this consists of everything but
	 * the peer id.
	 */
	uint8_t handshake[1024 /* XXX */];
	c->read((void*)handshake, 1 + strlen(PEER_PSTR) + 8 + TORRENT_HASH_LEN, true);
	TRACE(NETWORK, "got handshake (part 1): connection=%s", c->getEndpoint().c_str());
	
	/* So, we have part one of the handshake; dissect and validate it */
	uint8_t reserved[8];
	memcpy(reserved, (const char*)(handshake + 1 + strlen(PEER_PSTR)), 8);
	string info((const char*)(handshake + 1 + strlen(PEER_PSTR) + 8), TORRENT_HASH_LEN);
	if (handshake[0] != strlen(PEER_PSTR) ||
			memcmp((handshake + 1), PEER_PSTR, strlen(PEER_PSTR))) {
		TRACE(NETWORK, "got bad protocol version from %s, dropping!", c->getEndpoint().c_str());
		delete c;
		return;
	}

	/* Find the torrent that belongs to this info hash */
	Torrent* t = NULL;
	{
		shared_lock<shared_mutex> lock(rwl_torrents);
		map<string, Torrent*>::iterator it = torrents.find(info);
		if (it != torrents.end())
			t = it->second;
	}
	if (t == NULL) {
		TRACE(TORRENT, "connection %s: peer requests unknown info hash, dropping", c->getEndpoint().c_str());
		delete c;
		return;
	}

	/* Ensure the torrent can still accept a new peer; if not, ditch the connection */
	if (!t->canAcceptPeer()) {
		TRACE(TORRENT, "connection %s: rejected by torrent, dropping", c->getEndpoint().c_str());
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
	TRACE(NETWORK, "got handshake (part 2): connection=%p", c->getEndpoint().c_str());
	string peer((const char*)(handshake), TORRENT_PEERID_LEN);
	if (!memcmp(handshake, peerid, TORRENT_PEERID_LEN)) {
		TRACE(NETWORK, "handshake aborted: connection=%p,peer=%s is us", c, c->getEndpoint().c_str());
		delete p;
		return;
	}
	p->setPeerID(peer);
	TRACE(NETWORK, "handshake completed: connection=%p,peer=%s", c, p->getID().c_str());

	/* We accept! We have no choice! */
	t->registerPeer(p);
	receiver->addPeer(p);
	TRACE(NETWORK, "accepted peer %p for torrent %p",
	 c, t);

	/*
	 * Only now can we send our handshake / bitfield - the Sender will only
	 * consider peers known to the Torrent, so it may skip the peer in the first
	 * iteration.
	 */
	p->sendHandshake();
	p->sendBitfield();
}

Torrent*
Overseer::findTorrent(const uint8_t* hash)
{
	string hash_string((const char*)hash, TORRENT_HASH_LEN);
	Torrent* t = NULL;

	{
		shared_lock<shared_mutex> lock(rwl_torrents);
		map<string, Torrent*>::iterator it = torrents.find(hash_string);
		if (it != torrents.end())
			t = it->second;
	}

	return t;
}

/*
 * Below are principe of least knowledge functions which just forward the call to the
 * appropriate object.
 */

void
Overseer::queueHashPiece(Torrent* t, uint32_t piece)
{
	hasher->addPiece(t, piece);
}

void
Overseer::cancelHashing(Torrent* t)
{
	hasher->cancelTorrent(t);
}

void
Overseer::signalSender()
{
	sender->signal();
}

void
Overseer::addPeer(Peer* p)
{
	receiver->addPeer(p);
}

void
Overseer::removePeer(Peer* p)
{
	receiver->removePeer(p);
}

void
Overseer::addRequest(HTTPRequest* r)
{
	receiver->addRequest(r);
}

void
Overseer::removeRequest(HTTPRequest* r)
{
	receiver->removeRequest(r);
}

Peer*
Overseer::findPeerByFDAndLock(int fd)
{
	return receiver->findPeerByFDAndLock(fd);
}

void 
Overseer::removePeerByFD(int fd)
{
	return receiver->removePeerByFD(fd);
}

void
Overseer::getSendablePeers(list<int>& m)
{
	return receiver->getSendablePeers(m);
}


void
Overseer::addFile(File* f)
{
	filemanager->addFile(f);
}

void
Overseer::removeFile(File* f)
{
	filemanager->removeFile(f);
}

void
Overseer::writeFile(File* f, off_t offset, const void* buf, size_t len)
{
	filemanager->writeFile(f, offset, buf, len);
}

void
Overseer::readFile(File* f, off_t offset, void* buf, size_t len)
{
	filemanager->readFile(f, offset, buf, len);
}

/* vim:set ts=2 sw=2: */
