#include <pthread.h>
#include <queue>
#include "peer.h"

class Overseer;

#ifndef __UPLOADER_H__
#define __UPLOADER_H__

/*! \brief A message to be sent */
class SenderRequest {
public:
	//! \brief Construct a new request to upload a piece
	SenderRequest(Peer* p, uint32_t piece, uint32_t begin, uint32_t len);

	//! \brief Constructs a request to send a message
	SenderRequest(Peer* p, uint8_t msg, const uint8_t* data, uint32_t len);

	//! \brief Obliterates the request
	~SenderRequest();

	//! \brief Retrieve the peer to upload to
	Peer* const getPeer() { return peer; }

	//! \brief Is the request cancelled?
	bool isCancelled() { return cancelled; }

	//! \brief Retrieve the data to upload
	const uint8_t* getMessage();

	//! \brief Retrieve the number of bytes to upload
	const uint32_t getMessageLength();

	//! \brief Retrieves the piece to download
	const uint32_t getPiece() { return piece; }

	//! \brief Retrieve the offset to download
	const uint32_t getOffset() { return offset; }

	//! \brief Retrieve the length of the piece to download
	const uint32_t getPieceLength() { return piece_length; }

	//! \brief Is this a request to send data?
	const bool haveData() { return (message == NULL); }

	//! \brief Is this request partial?
	bool isPartialRequest() { return skip_num > 0; }

	//! \brief Skip a specific number of bytes
	void skip(uint32_t l) { skip_num += l; }

	/*! \brief Cancel the request
	 *
	 *  If a request is cancelled, this means the peer may or may not
	 *  be available anymore; either way, we should not rely on the
	 *  peer being available.
	 */
	void cancel() { cancelled = true; }

private:
	//! \brief Peer we should be uploading to
	Peer* peer;

	//! \brief Is the request cancelled?
	bool cancelled;

	//! \brief Number of bytes to upload
	uint32_t length;

	//! \brief Number of bytes to skip when sending data
	uint32_t skip_num;

	//! \brief Message to send
	uint8_t* message;

	/*!
	 * Fields below are only applicable when uploading pieces.
	 */

	//! \brief Piece to upload
	uint32_t piece;

	//! \brief Begin offset
	uint32_t offset;

	//! \brief Number of bytes to upload
	uint32_t piece_length;
};

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

	//! \brief Queue a piece sender request
	void enqueuePieceRequest(Peer* p, uint32_t piece, uint32_t begin, uint32_t len);

	//! \brief Queue a message
	void enqueueMessage(Peer* p, uint8_t msg, uint8_t* data, uint32_t len);

	//! \brief Cancels a piece request
	void dequeuePieceRequest(Peer* p, uint32_t piece, uint32_t begin, uint32_t len);

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
