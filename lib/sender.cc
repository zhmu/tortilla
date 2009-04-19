#include <assert.h>
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

void*
sender_thread(void* ptr)
{
	((Sender*)ptr)->process();
	return NULL;
}

Sender::Sender(Overseer* o)
{
	terminating = false; overseer = o;

	pthread_rwlock_init(&rwl_queue, NULL);
	pthread_mutex_init(&mtx_data, NULL);
	pthread_cond_init(&cv_queue, NULL);

	/* Off we gooo! */
	pthread_create(&thread, NULL, sender_thread, this);
}

Sender::~Sender()
{
	/*
	 * Request termination and wait for the thread to die.
	*/
	terminating = true;
	pthread_cond_signal(&cv_queue);
	pthread_join(thread, NULL);

	pthread_cond_destroy(&cv_queue);
	pthread_rwlock_destroy(&rwl_queue);
}

void
Sender::enqueueSenderRequest(SenderRequest* sr)
{
	WLOCK(queue);
	requests.push_back(sr);
	RWUNLOCK(queue);

	/* Awaken! */
	pthread_cond_signal(&cv_queue);
}

void
Sender::removeRequestsFromPeer(Peer* p)
{
	/*
	 * As we do not know whether requests are currently being serviced, we
	 * traverse the outgoing queue and cancel any request belonging to this
	 * peer.
	 */
	RLOCK(queue);
	for (list<SenderRequest*>::iterator it = requests.begin();
	     it != requests.end(); it++) {
		SenderRequest* sr = *it;
		if (sr->getPeer() == p)
			sr->cancel();
	}
	RWUNLOCK(queue);
}

void
Sender::dequeuePieceRequest(Peer* p, uint32_t piece, uint32_t begin, uint32_t len)
{
	RLOCK(queue);
	for (list<SenderRequest*>::iterator it = requests.begin();
	     it != requests.end(); it++) {
		SenderRequest* sr = *it;
		if (sr->getPeer() == p &&
		    sr->getPiece() == piece &&
		    sr->getOffset() == begin &&
		    sr->getPieceLength() == len &&
		    !sr->haveData())
			sr->cancel();
	}
	RWUNLOCK(queue);
}

void
Sender::dequeueRequestForChunk(Torrent* t, uint32_t piece, uint32_t begin, uint32_t len)
{
	RLOCK(queue);
	for (list<SenderRequest*>::iterator it = requests.begin();
	     it != requests.end(); it++) {
		SenderRequest* sr = *it;
		if (sr->isCancelled())
			continue;
		if (sr->getPeer()->getTorrent() == t &&
		    sr->getPiece() == piece &&
		    sr->getOffset() == begin &&
		    sr->getPieceLength() == len &&
		    !sr->haveData())
			sr->cancel();
	}
	RWUNLOCK(queue);
}

void
Sender::process()
{
	while(true) {
		/*
		 * See if the queue is empty; we need this to determine whether we have to
		 * wait for the queue to fill up.
		 */
		RLOCK(queue);
		bool queueEmpty = requests.empty();
		RWUNLOCK(queue);

		/* Wait for the queue to fill up, if needed */
		pthread_mutex_lock(&mtx_data);
		if (!terminating && queueEmpty)
			pthread_cond_wait(&cv_queue, &mtx_data);
		pthread_mutex_unlock(&mtx_data);
		if (terminating)
			break;

		while (!terminating) {
			/* Try to fetch a request from the queue */
			WLOCK(queue);
			SenderRequest* request = NULL;
			while (!requests.empty()) {
				request = requests.front();
				requests.pop_front();
				if (!request->isCancelled())
					break;
				delete request;
				request = NULL;
			}
			RWUNLOCK(queue);
			if (request == NULL)
				/* Queue is empty; try again later */
				break;

			/*
			 * If the peer is still connecting, or if the previous request was
			 * partial (i.e. we couldn't send all data in one go) but we aren't
			 * sending the remaining data of it, we should stall the request.
			 */
			if (!request->isCancelled())
				if ( request->getPeer()->areConnecting() ||
						(request->getPeer()->wasLastSendIncomplete() && !request->isPartialRequest())) {
					WLOCK(queue);
					requests.push_back(request);
					RWUNLOCK(queue);
					continue;
				}

			/* Fetch the amount of data we may still transfer */
			pthread_mutex_lock(&mtx_data);
			uint32_t cur_tx = tx_left;
			pthread_mutex_unlock(&mtx_data);

			/*
			 * Ask the peer to process.
			 */
			ssize_t amount = request->getPeer()->processSenderRequest(request, cur_tx);

			pthread_mutex_lock(&mtx_data);
			if (tx_left > 0 && amount > 0)
				tx_left -= amount;
			cur_tx = tx_left;
			pthread_mutex_unlock(&mtx_data);

			/*
			 * If we still have bytes left, re-queue the request for next time.
			 * Otherwise, just get rid of it.
			 */
			if (request->getMessageLength() == 0) {
				delete request;
			} else {
				/*
				 * We didn't transfer this chunk in a single go; this means we should
				 * preferably give some other peer a chance to grab data while we delay
				 * this peer. In order to accomplish this, we just put it at the end of
				 * the queue and deal with it some other time (if the request was
				 * partial, we'll skip any request until we find this one again)
				 */
				if (!request->isCancelled()) {
					WLOCK(queue);
					requests.push_back(request);
					RWUNLOCK(queue);
				}
			}

			if (overseer->getUploadRate() > 0 && cur_tx == 0) {
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
}

void
Sender::setAmountTransferrable(uint32_t amount)
{
	pthread_mutex_lock(&mtx_data);
	tx_left = amount;
	pthread_mutex_unlock(&mtx_data);
}

/* vim:set ts=2 sw=2: */
