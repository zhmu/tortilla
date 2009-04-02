#include <assert.h>
#include <string.h>
#include "overseer.h"
#include "sender.h"
#include "torrent.h"
#include "tracer.h"

using namespace std;

#define WRITE_UINT32(ptr,offs,val) \
	ptr[(offs) + 0] = (((val) >> 24) & 0xff); \
	ptr[(offs) + 1] = (((val) >> 16) & 0xff); \
	ptr[(offs) + 2] = (((val) >>  8) & 0xff); \
	ptr[(offs) + 3] = (((val)      ) & 0xff);


SenderRequest::SenderRequest(Peer* p, uint32_t piece, uint32_t begin, uint32_t len)
{
	peer = p; length = len + 13; skip_num = 0;
	this->piece = piece; this->offset = begin; this->piece_length = len;

	message = new uint8_t[length];
	WRITE_UINT32(message, 0, len + 9);
	message[4] = PEER_MSGID_PIECE;
	WRITE_UINT32(message, 5, piece);
	WRITE_UINT32(message, 9, offset);
	p->getTorrent()->readChunk(piece, begin, (message + 13), len);
}

SenderRequest::SenderRequest(Peer* p, uint8_t msg, const uint8_t* data, uint32_t len)
{
	peer = p; length = len + 5; skip_num = 0; piece = 0; offset = 0; piece_length = 0;

	message = new uint8_t[length];
	WRITE_UINT32(message, 0, len + 1);
	message[4] = msg;
	memcpy((message + 5), data, len);
}

#undef WRITE_UINT32

const uint8_t* 
SenderRequest::getMessage()
{
	return (const uint8_t*)(message + skip_num);
}

const uint32_t
SenderRequest::getMessageLength() {
	return length - skip_num;
}


SenderRequest::~SenderRequest()
{
	delete[] message;
}

void*
sender_thread(void* ptr)
{
	((Sender*)ptr)->process();
	return NULL;
}

Sender::Sender(Overseer* o)
{
	terminating = false; overseer = o;

	pthread_mutex_init(&mtx_queue, NULL);
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
	pthread_mutex_destroy(&mtx_queue);
}

void
Sender::enqueuePieceRequest(Peer* p, uint32_t piece, uint32_t begin, uint32_t len)
{
	pthread_mutex_lock(&mtx_queue);
	requests.push_back(new SenderRequest(p, piece, begin, len));
	pthread_mutex_unlock(&mtx_queue);

	/* Awaken! */
	pthread_cond_signal(&cv_queue);
}

void
Sender::enqueueMessage(Peer* p, uint8_t msg, uint8_t* data, uint32_t len)
{
	pthread_mutex_lock(&mtx_queue);
	requests.push_back(new SenderRequest(p, msg, data, len));
	pthread_mutex_unlock(&mtx_queue);

	/* Awaken! */
	pthread_cond_signal(&cv_queue);
}

/* Helper for removeRequestsFromPeer */
class peer_match {
public:
	inline peer_match(Peer *p) { peer = p; };
	bool operator () (SenderRequest* ur) {
		return ur->getPeer() == peer;
	}

private:
	const Peer* peer;
};

void
Sender::removeRequestsFromPeer(Peer* p)
{
	pthread_mutex_lock(&mtx_queue);
	requests.remove_if(peer_match(p));
	pthread_mutex_unlock(&mtx_queue);
}

/* Helper for dequeuePieceRequest */
class request_match {
public:
	inline request_match(SenderRequest u) : ur(u) { }
	bool operator () (SenderRequest* u) {
		return (ur.getPeer() == u->getPeer() &&
		        ur.getPiece() == u->getPiece() &&
		        ur.getOffset() == u->getOffset() &&
			      ur.getPieceLength() == u->getPieceLength() &&
		        !u->haveData());
	}

private:
	SenderRequest ur;
};

void
Sender::dequeuePieceRequest(Peer* p, uint32_t piece, uint32_t begin, uint32_t len)
{
	pthread_mutex_lock(&mtx_queue);
	requests.remove_if(request_match(SenderRequest(p, piece, begin, len)));
	pthread_mutex_unlock(&mtx_queue);
}

void
Sender::process()
{
	while(true) {
		pthread_mutex_lock(&mtx_queue);
		if (!terminating && requests.empty())
			pthread_cond_wait(&cv_queue, &mtx_queue);
		if (terminating)
			break;

		while (!terminating && !requests.empty()) {
			/* Grab a request from the queue */
			SenderRequest* request = requests.front();
			pthread_mutex_unlock(&mtx_queue);

			/* Ask the peer to process */
			pthread_mutex_lock(&mtx_queue);
			uint32_t cur_tx = tx_left;
			pthread_mutex_unlock(&mtx_queue);

			uint32_t amount = request->getPeer()->processSenderRequest(request, cur_tx);

			pthread_mutex_lock(&mtx_data);
			if (tx_left > 0)
				tx_left -= amount;
			cur_tx = tx_left;
			pthread_mutex_unlock(&mtx_data);

			/*
			 * If we still have bytes left, re-queue the request for next time.
			 * Otherwise, just get rid of it.
			 */
			if (request->getMessageLength() == 0) {
				delete request;
				pthread_mutex_lock(&mtx_queue);
				requests.pop_front();
				pthread_mutex_unlock(&mtx_queue);
			} else {
				/*
				 * XXX We didn't transfer this chunk in a single go; this means we
				 * should preferably give some other peer a chance to grab data while
				 * we delay this peer...
				 */
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

			pthread_mutex_lock(&mtx_queue);
		}
		pthread_mutex_unlock(&mtx_queue);
	}

	pthread_mutex_unlock(&mtx_queue);
}

void
Sender::setAmountTransferrable(uint32_t amount)
{
	pthread_mutex_lock(&mtx_data);
	tx_left = amount;
	pthread_mutex_unlock(&mtx_data);
}

/* vim:set ts=2 sw=2: */
