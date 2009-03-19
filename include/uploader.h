#include <pthread.h>
#include <queue>
#include "peer.h"

#ifndef __UPLOADER_H__
#define __UPLOADER_H__

/*! \brief A chunk to be uploaded */
class UploadRequest {
public:
	//! \brief Construct a new request to upload a piece
	inline UploadRequest(Peer* p, uint32_t piece, uint32_t begin, uint32_t len) {
		peer = p; this->piece = piece; offset = begin; length = len;
	}

	//! \brief Retrieve the peer to upload to
	Peer* const getPeer() { return peer; }

	//! \brief Retrieves the piece to download
	const uint32_t getPiece() { return piece; }

	//! \brief Retrieve the offset to download
	const uint32_t getOffset() { return offset; }

	//! \brief Retrieve the length of the piece to download
	const uint32_t getLength() { return length; }

private:
	//! \brief Peer we should be uploading to
	Peer* peer;

	//! \brief Piece to upload
	uint32_t piece;

	//! \brief Begin offset
	uint32_t offset;

	//! \brief Number of bytes to upload
	uint32_t length;
};

/*! \brief Handles uploading chunks to peers
 *
 *  This resides in a seperate thread to monitor the available bandwidth.
 */
class Uploader {
friend	void* upload_thread(void* ptr);
public:
	//! \brief Constructs an uploader
	Uploader();

	//! \brief Destroys the uploader
	~Uploader();

	//! \brief Queue a request
	void enqueue(Peer* p, uint32_t piece, uint32_t begin, uint32_t len);

	//! \brief Cancels a request
	void dequeue(Peer* p, uint32_t piece, uint32_t begin, uint32_t len);

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
	 */
	std::list<UploadRequest*> requests;
};

#endif /*  __UPLOADER_H__ */
