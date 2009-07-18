#include <algorithm>
#include <assert.h>
#include <errno.h>
#include <list>
#include "exceptions.h"
#include "peer.h"
#include "peermanager.h"
#include "macros.h"
#include "overseer.h"
#include "tracer.h"

using namespace std;

#define TRACER (overseer->getTracer())

void*
peermanager_thread(void* ptr)
{
	((PeerManager*)ptr)->process();
	return NULL;
}

PeerManager::PeerManager(Overseer* o)
{
	INIT_RWLOCK(data);
	overseer = o; terminating = false;

	pthread_create(&thread, NULL, peermanager_thread, this);
}

PeerManager::~PeerManager()
{
	terminating = true;
	pthread_join(thread, NULL);

	DESTROY_RWLOCK(data);
}

void
PeerManager::addPeer(Peer* p)
{
	WLOCK(data);
	peers.push_back(p);
	fdMap[p->getFD()] = p;
	RWUNLOCK(data);
}

void
PeerManager::removePeer(Peer* p)
{
	WLOCK(data);
	fdMap.erase(p->getFD());
	peers.remove(p);
	RWUNLOCK(data);

	delete p;
}

Peer*
PeerManager::findPeerByFDAndLock(int fd)
{
	RLOCK(data);
	Peer* p = NULL;
	map<int, Peer*>::iterator it = fdMap.find(fd);
	if (it != fdMap.end()) {
		p = it->second;
		p->lockForSending();
	}
	RWUNLOCK(data);
	return p;
}

void
PeerManager::process()
{
	while (!terminating) {
		fd_set readfds, writefds;

		/*
	 	 * Gracefully handle any peers that are going away.
		  */
		WLOCK(data);
		list<Peer*>::iterator peerit = peers.begin();
		while (peerit != peers.end()) {
			Peer* p = *peerit;
			if (!p->isShuttingDown()) {
				peerit++;
				continue;
			}

			peers.erase(peerit);
			fdMap.erase(p->getFD());
			p->getTorrent()->unregisterPeer(p);
			delete p;

			peerit = peers.begin();
		}
		RWUNLOCK(data);

		/*
		 * Construct our file descriptor set; we need to make read/write sets
		 * because the result of a connect(2)-attempt triggers a write event.
		 */
		int maxfd = -1;
		FD_ZERO(&readfds); FD_ZERO(&writefds);
		RLOCK(data);
		for (list<Peer*>::iterator it = peers.begin();
		     it != peers.end(); it++) {
			Peer* p = *it;
			int fd = p->getFD();
			if (maxfd < fd) maxfd = fd;
			if (p->areConnecting())
				FD_SET(fd, &writefds);
			FD_SET(fd, &readfds);
		}
		RWUNLOCK(data);

		/*
		 *
		 * Note that, for busy torrents, this 0.5 second loop will never be reached.
		 */
		struct timeval tv;
		tv.tv_sec = 0; tv.tv_usec = 5000;
		int n = select(maxfd + 1, &readfds, &writefds, (fd_set*)NULL, &tv);
		if (n == 0)
			continue;

		/* If we are terminating, we don't care about any data as we're leaving */
		if (terminating)
			continue;

		/*
		 * Wade through all peers, handle any data to service.
		 */
		RLOCK(data);
		for (list<Peer*>::iterator it = peers.begin();
				 it != peers.end(); it++) {
			Peer* p = (*it);
			int fd = (*it)->getFD();
			if (FD_ISSET(fd, &writefds)) {
				/* Handle with made connections */
				if (p->areConnecting())
					p->connectionDone();
			}
			if (!FD_ISSET(fd, &readfds))
				continue;

			/*
			 * There is data here.
			 */
			uint8_t buf[65536 /* XXX */];
			ssize_t len = ::recv(fd, buf, sizeof(buf), MSG_DONTWAIT);
			if (len <= 0) {
				/* socket lost */
				TRACE(TORRENT, "connection to peer=%s lost, socket closed, errno=%u, len=%ld", p->getID().c_str(), errno, len);
				p->shutdown();
				continue;
			}

			/* Hand the data off to the application */
			if (p->receive(buf, len) == true) {
				/* Need to sever the connection */
				TRACE(TORRENT, "severing connection to peer=%s", p->getID().c_str());
				p->shutdown();
				continue;
			}
		}
		RWUNLOCK(data);
	}
}

void
PeerManager::getSendablePeers(list<int>& m)
{
	RLOCK(data);
	for (list<Peer*>::iterator it = peers.begin();
	     it != peers.end(); it++) {
		Peer* p = *it;
		if (p->isSenderQueueEmpty())
			continue;
		m.push_back(p->getFD());
	}
	RWUNLOCK(data);
}

/* vim:set ts=2 sw=2: */
