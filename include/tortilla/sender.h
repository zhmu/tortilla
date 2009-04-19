#include <pthread.h>
#include <queue>
#include "peer.h"
#include "senderrequest.h"

class Overseer;

#ifndef __UPLOADER_H__
#define __UPLOADER_H__

/*! \brief Handles sending data to peers
 *
 *  This resides in a seperate thread to monitor the available bandwidth and
 *  gracefully deal with write timeouts risking blocking the torrent itself.
 */
class Sender {
friend	void* sender_thread(void* ptr);
friend  class Overseer;
public:
	/*! \brief Constructs an uploader
	 *  \param o Overseer to use
	 */
	Sender(Overseer* o);

	//! \brief Destroys the uploader
	~Sender();

	//! \brief Queue a sender request
	void enqueueSenderRequest(SenderRequest* sr);

	//! \brief Cancels a piece request
	void dequeuePieceRequest(Peer* p, uint32_t piece, uint32_t begin, uint32_t len);

	/*! \brief Cancels a request for a chunk
	 *
	 *  This is used in endgame mode; if we have requests queued for a chunk that
	 *  we have yet to send (i.e. we already have the chunk, but haven't asked the peer to
	 *  send it), this will cancel such a request.
	 */
	void dequeueRequestForChunk(Torrent* t, uint32_t piece, uint32_t begin, uint32_t len);

	//! \brief Remove all requests by a specific peer
	void removeRequestsFromPeer(Peer* p);

	/*! \brief Set the number of bytes we may upload this interval
	 *
	 *  Zero means anything goes.
	 */
	void setAmountTransferrable(uint32_t amount);

protected:
	//! \brief Handles processing of the queue
	void process();

private:
	/*! \brief Mutex protecting the queue
	 *
	 *  This is a r/w lock because we need to scan the list from
	 *  other threads (i.e. the torrent itself may cancel
	 *  requests) yet the Sender itself modifies the list when
	 *  requests vanish...
	 */
	pthread_rwlock_t rwl_queue;

	//! \brief Mutex protecting our local data
	pthread_mutex_t mtx_data;

	//! \brief Condition variable used to kick the queue
	pthread_cond_t cv_queue;

	//! \brief Thread used by the uploader
	pthread_t thread;

	//! \brief Are we terminating?
	bool terminating;

	/*! \brief Number of bytes allowed to be transfered
	 *
	 *  This is protected by mtx_data.
	 */
	uint32_t tx_left;
	
	/*! \brief Queue of items to be handeled
	 *
	 *  This is implemented as a list as we need to remove items when a
	 *  peer vanishes.
	 */
	std::list<SenderRequest*> requests;

	//! \brief Overseer object we belong to
	Overseer* overseer;
};

#endif /*  __UPLOADER_H__ */
