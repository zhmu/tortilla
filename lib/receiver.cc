#include <boost/thread/locks.hpp>
#include <algorithm>
#include <assert.h>
#include <errno.h>
#include <list>
#include "exceptions.h"
#include "httprequest.h"
#include "peer.h"
#include "receiver.h"
#include "macros.h"
#include "overseer.h"
#include "tracer.h"

using namespace std;
using namespace boost;

#define TRACER (overseer->getTracer())

void*
receiver_thread(void* ptr)
{
	((Receiver*)ptr)->process();
	return NULL;
}

Receiver::Receiver(Overseer* o)
	: overseer(o), terminating(false),
	  thread(receiver_thread, this)
{
}

Receiver::~Receiver()
{
	terminating = true;
	thread.join();
}

void
Receiver::addPeer(Peer* p)
{
	unique_lock<shared_mutex> lock(rwl_data);
	peers.push_back(p);
	fdMap[p->getFD()] = p;
}

void
Receiver::removePeer(Peer* p)
{
	{
		unique_lock<shared_mutex> lock(rwl_data);
		fdMap.erase(p->getFD());
		peers.remove(p);
	}

	delete p;
}

void
Receiver::addRequest(HTTPRequest* r)
{
	unique_lock<shared_mutex> lock(rwl_data);
	requests.push_back(r);
}

void
Receiver::removeRequest(HTTPRequest* r)
{
	{
		unique_lock<shared_mutex> lock(rwl_data);
		requests.remove(r);
	}

	delete r;
}

Peer*
Receiver::findPeerByFDAndLock(int fd)
{
	Peer* p = NULL;
	{
		shared_lock<shared_mutex> lock(rwl_data);
		map<int, Peer*>::iterator it = fdMap.find(fd);
		if (it != fdMap.end()) {
			p = it->second;
			p->lockForSending();
		}
	}
	return p;
}

void
Receiver::removePeerByFD(int fd)
{
	/*
	 * This is called by the sender, once it determines that a peer is dead and
	 * should be removed. We can never actually remove the peer here, since there
	 * may still be references to it -- we just schedule it for termination and
	 * have the receiver do the dirty work.
	 */
	shared_lock<shared_mutex> lock(rwl_data);
	map<int, Peer*>::iterator it = fdMap.find(fd);
	if (it != fdMap.end()) {
		it->second->shutdown();
	}
}

void
Receiver::process()
{
	while (!terminating) {
		fd_set readfds, writefds;

		/*
		 * Gracefully handle any peers that are going away
		 * XXX this is O(|peers|) which we can often skip if no peers are shutting down
		 */
		{
			unique_lock<shared_mutex> lock(rwl_data);
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

			/* Remove any requests that need to go, too */
			list<HTTPRequest*>::iterator reqit = requests.begin();
			while (reqit != requests.end()) {
				HTTPRequest* r = *reqit;
				if (!r->mustTerminate()) {
					reqit++;
					continue;
				}
				requests.erase(reqit);
				delete r;

				reqit = requests.begin();
			}
		}

		/*
		 * Construct our file descriptor set; we need to make read/write sets
		 * because the result of a connect(2)-attempt triggers a write event.
		 */
		int maxfd = -1;
		FD_ZERO(&readfds); FD_ZERO(&writefds);
		{
			shared_lock<shared_mutex> lock(rwl_data);
			for (list<Peer*>::iterator it = peers.begin();
					 it != peers.end(); it++) {
				Peer* p = *it;
				int fd = p->getFD();
				if (maxfd < fd) maxfd = fd;
				if (p->areConnecting())
					FD_SET(fd, &writefds);
				FD_SET(fd, &readfds);
			}

			/* Handle requests, too */
			for (list<HTTPRequest*>::iterator it = requests.begin();
					 it != requests.end(); it++) {
				HTTPRequest* r = *it;
				int fd = r->getFD();
				if (maxfd < fd) maxfd = fd;
				if (r->isWaitingForRead())
					FD_SET(fd, &readfds);
				if (r->isWaitingForWrite())
					FD_SET(fd, &writefds);
			}
		}

		/*
		 * Add the listener socket; it makes absolutely no sense to monitor this
		 * socket seperately from the rest.
		 */
		int listenerFD = overseer->getIncoming()->getFD();
		FD_SET(listenerFD, &readfds);
		if (maxfd < listenerFD)
			maxfd = listenerFD;

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
		{
			shared_lock<shared_mutex> lock(rwl_data);
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

			for (list<HTTPRequest*>::iterator it = requests.begin();
					 it != requests.end(); it++) {
				HTTPRequest* r = *it;
				int fd = r->getFD();

				if (FD_ISSET(fd, &readfds) || FD_ISSET(fd, &writefds))
					r->process();
			}
		}

		/* If we need to accept new connections, handle that */
		if (FD_ISSET(listenerFD, &readfds) && !terminating) {
			Connection* c = overseer->getIncoming()->acceptConnection();
			if (c != NULL) {
				overseer->handleIncomingConnection(c);
			}
		}
	}
}

void
Receiver::getSendablePeers(list<int>& m)
{
	shared_lock<shared_mutex> lock(rwl_data);
	for (list<Peer*>::iterator it = peers.begin();
	     it != peers.end(); it++) {
		Peer* p = *it;
		if (p->isSenderQueueEmpty())
			continue;
		m.push_back(p->getFD());
	}
}

/* vim:set ts=2 sw=2: */
