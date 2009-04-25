#include <assert.h>
#include <poll.h>
#include <string.h>
#include "overseer.h"
#include "sender.h"
#include "senderrequest.h"
#include "torrent.h"
#include "tracer.h"

using namespace std;

#define RLOCK(x)    pthread_rwlock_rdlock(&rwl_## x);
#define WLOCK(x)    pthread_rwlock_wrlock(&rwl_## x);
#define RWUNLOCK(x) pthread_rwlock_unlock(&rwl_## x);

#define TRACER (overseer->getTracer())

void*
sender_thread(void* ptr)
{
	((Sender*)ptr)->process();
	return NULL;
}

Sender::Sender(Overseer* o)
{
	terminating = false; overseer = o;

	pthread_mutex_init(&mtx_data, NULL);
	pthread_cond_init(&cv, NULL);

	/* Off we gooo! */
	pthread_create(&thread, NULL, sender_thread, this);
}

Sender::~Sender()
{
	/*
	 * Request termination and wait for the thread to die.
	*/
	terminating = true;
	pthread_cond_signal(&cv);
	pthread_join(thread, NULL);

	pthread_cond_destroy(&cv);
}

void
Sender::process()
{
	while(!terminating) {
		/* Figure out to which peers we must send */
		map<int, Peer*> fdMap;
		fdMap.clear();
		overseer->getSendablePeers(fdMap);

		/*
		 * Wait until we can write to any of these peers. We use poll(2) to query
		 * this, as it gives us the means to distinguish between a socket closing
		 * and a socket that actually is ready (closed sockets indicate the peer
		 * was deleted, so we shouldn't try to send to them.
		 */
		struct pollfd pfds[TORRENT_DESIRED_PEERS];
		unsigned int cur_pfd = 0;
		for (map<int, Peer*>::iterator it = fdMap.begin();
		     it != fdMap.end(); it++) {
			pfds[cur_pfd].fd = it->first;
			pfds[cur_pfd].events = POLLOUT;
			pfds[cur_pfd].revents = 0;
			cur_pfd++;
		}

		/* If there is nothing to wait for, rest and try again */
		if (cur_pfd == 0) {
			pthread_mutex_lock(&mtx_data);
			pthread_cond_wait(&cv, &mtx_data);
			pthread_mutex_unlock(&mtx_data);
			continue;
		}

		if (poll(pfds, cur_pfd, 5000) <= 0)
			continue;

		/* Ask each peer to request */
		for (unsigned int pfd = 0; pfd < cur_pfd; pfd++) {
			if (!(pfds[pfd].revents & POLLOUT))
				continue;

			/*
			 * Update amount of bandwidth left; if this is zero, we stop as we can't
			 * send anymore.
			 */
			pthread_mutex_lock(&mtx_data);
			uint32_t cur_tx = tx_left;
			pthread_mutex_unlock(&mtx_data);
			if (overseer->getUploadRate() > 0 && cur_tx == 0)
				break;

			/*
			 * Ask the peer to process and update bandwidth use.
			 */
			Peer* p = fdMap[pfds[pfd].fd];
			ssize_t amount = p->processSenderQueue(overseer->getUploadRate() > 0 ? cur_tx : -1);

			pthread_mutex_lock(&mtx_data);
			if (tx_left > 0 && amount > 0)
				tx_left -= amount;
			cur_tx = tx_left;
			pthread_mutex_unlock(&mtx_data);
		}

		if (overseer->getUploadRate() > 0 && tx_left == 0) {
			/*
		 	 * We've run out of bandwidth to use! Wait for a second for it to
			 * replenish.
		 	 */
			struct timeval tv;
			tv.tv_sec = 1; tv.tv_usec = 0;
			select(0, NULL, NULL, NULL, &tv);
		}
	}

}

void
Sender::setAmountTransferrable(uint32_t amount)
{
	pthread_mutex_lock(&mtx_data);
	tx_left = amount;
	pthread_mutex_unlock(&mtx_data);
}

void
Sender::signal()
{
	pthread_cond_signal(&cv);
}

/* vim:set ts=2 sw=2: */
