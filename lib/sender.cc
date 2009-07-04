#include <assert.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include "macros.h"
#include "overseer.h"
#include "sender.h"
#include "senderrequest.h"
#include "torrent.h"
#include "tracer.h"
#include <algorithm>

using namespace std;

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

	INIT_MUTEX(data);
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
	DESTROY_MUTEX(data);
}

void
Sender::process()
{
	unsigned int pfds_max = TORRENT_MAX_PEERS;
	struct pollfd* pfds = (struct pollfd*)malloc(sizeof(struct pollfd) * pfds_max);
	assert(pfds != NULL); /* crude */

	while(!terminating) {
		/* Figure out to which peers we must send */
		list<int> fdMap;
		fdMap.clear();
		overseer->getSendablePeers(fdMap);
		if (fdMap.size() > pfds_max) {
			pfds = (struct pollfd*)realloc(pfds, sizeof(struct pollfd) * fdMap.size());
			assert(pfds != NULL); /* crude */
			pfds_max = fdMap.size();
		}

		/*
		 * Wait until we can write to any of these peers. We use poll(2) to query
		 * this, as it gives us the means to distinguish between a socket closing
		 * and a socket that actually is ready (closed sockets indicate the peer
		 * was deleted, so we shouldn't try to send to them.
		 */
		unsigned int cur_pfd = 0;
		for (list<int>::iterator it = fdMap.begin();
		     it != fdMap.end(); it++) {
			pfds[cur_pfd].fd = *it;
			pfds[cur_pfd].events = POLLOUT;
			pfds[cur_pfd].revents = 0;
			cur_pfd++;
		}

		/* If there is nothing to wait for, rest and try again */
		if (cur_pfd == 0) {
			LOCK(data);
			pthread_cond_wait(&cv, &mtx_data);
			UNLOCK(data);
			continue;
		}

		if (poll(pfds, cur_pfd, 5000) <= 0)
			continue;

		/*
		 * At least a single peer can be sent to; construct a list of peers
		 * we can send to, and randomize it. This prevents us from using all
		 * available bandwidth on the first peer in the list.
		 */
		vector<unsigned int>peerFDs;
		for (unsigned int pfd = 0; pfd < cur_pfd; pfd++) {
			if (!(pfds[pfd].revents & POLLOUT))
				continue;

			peerFDs.push_back(pfds[pfd].fd);
		}
		random_shuffle(peerFDs.begin(), peerFDs.end());

		/* Ask each peer to request */
		for (vector<unsigned int>::iterator it = peerFDs.begin();
		    it != peerFDs.end(); it++) {
			unsigned int fd = *it;

			/*
			 * Update amount of bandwidth left; if this is zero, we stop as we can't
			 * send anymore.
			 */
			LOCK(data);
			uint32_t cur_tx = tx_left;
			UNLOCK(data);
			if (overseer->getUploadRate() > 0 && cur_tx == 0)
				break;

			/*
			 * Ask the peer to process and update bandwidth use; if we find a peer,
			 * we'll acquire a send lock which means it won't be removed. Note that
	 		 * processSenderQueue() releases this lock, so we cannot touch 'p' anymore
		 	 * after it finished!
			 */
			Peer* p = overseer->findPeerByFDAndLock(fd);
			if (p != NULL) {
				ssize_t amount = p->processSenderQueue(overseer->getUploadRate() > 0 ? cur_tx : -1);

				LOCK(data);
				if (tx_left > 0 && amount > 0)
					tx_left -= amount;
				cur_tx = tx_left;
				UNLOCK(data);
			}
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
	LOCK(data);
	tx_left = amount;
	UNLOCK(data);
}

void
Sender::signal()
{
	pthread_cond_signal(&cv);
}

/* vim:set ts=2 sw=2: */
