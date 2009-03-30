#include <pthread.h>
#include <queue>
#include "peer.h"

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

	//! \brief Retrieve the data to upload
	const uint8_t* getMessage() { return message; }

	//! \brief Retrieve the number of bytes to upload
	const uint32_t getMessageLength() { return length; }

	//! \brief Retrieves the piece to download
	const uint32_t getPiece() { return piece; }

	//! \brief Retrieve the offset to download
	const uint32_t getOffset() { return offset; }

	//! \brief Retrieve the length of the piece to download
	const uint32_t getPieceLength() { return piece_length; }

	//! \brief Is this a request to send data?
	const bool haveData() { return (message == NULL); }

private:
	//! \brief Peer we should be uploading to
	Peer* peer;

	//! \brief Number of bytes to upload
	uint32_t length;

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
 *  gracefully deal with withouts without risking blocking the torrent itself.
 */
class Sender {
friend	void* sender_thread(void* ptr);
public:
	//! \brief Constructs an uploader
	Sender();

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

protected:
	//! \brief Handles processing of the queue
	void process();

private:
	//! \brief Mutex protecting the queue
	pthread_mutex_t mtx_queue;

	//! \brief Condition variable used to kick the queue
	pthread_cond_t cv_queue;

	//! \brief Thread used by the uploader
	pthread_t thread;

	//! \brief Are we terminating?
	bool terminating;

	/*! \brief Queue of items to be handeled
	 *
	 *  This is implemented as a list as we need to remove items when a
	 *  peer vanishes.
	 */
	std::list<SenderRequest*> requests;
};

#endif /*  __UPLOADER_H__ */
