#include <string.h>
#include "peer.h"
#include "senderrequest.h"
#include "torrent.h"

using namespace std;

#define WRITE_UINT32(ptr,offs,val) \
	ptr[(offs) + 0] = (((val) >> 24) & 0xff); \
	ptr[(offs) + 1] = (((val) >> 16) & 0xff); \
	ptr[(offs) + 2] = (((val) >>  8) & 0xff); \
	ptr[(offs) + 3] = (((val)      ) & 0xff);

#define LOCK(x)     pthread_mutex_lock(&mtx_ ## x);
#define UNLOCK(x)   pthread_mutex_unlock(&mtx_ ## x);

void
SenderRequest::__init(Peer* p, uint32_t len)
{
	peer = p; length = len; skip_num = 0; piece = 0; offset = 0; piece_length = 0;
	cancelled = false;

	pthread_mutex_init(&mtx_data, NULL);
	message = new uint8_t[length];
}

SenderRequest::SenderRequest(Peer* p, uint32_t piece, uint32_t begin, uint32_t len)
{
	__init(p, len + 13);
	this->piece = piece; this->offset = begin; this->piece_length = len;

	WRITE_UINT32(message, 0, len + 9);
	message[4] = PEER_MSGID_PIECE;
	WRITE_UINT32(message, 5, piece);
	WRITE_UINT32(message, 9, offset);
	p->getTorrent()->readChunk(piece, begin, (message + 13), len);
}

SenderRequest::SenderRequest(Peer* p, uint8_t msg, const uint8_t* data, uint32_t len)
{
	__init(p, len + 5);

	WRITE_UINT32(message, 0, len + 1);
	message[4] = msg;
	memcpy((message + 5), data, len);
}

SenderRequest::SenderRequest(Peer* p, const uint8_t* data, uint32_t len)
{
	__init(p, len);

	memcpy(message, data, len);
}

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
	pthread_mutex_destroy(&mtx_data);
}


void
SenderRequest::cancel()
{
	LOCK(data);
	cancelled = true;
	UNLOCK(data);
}

//! \brief Is the request cancelled?
bool
SenderRequest::isCancelled()
{
	LOCK(data);
	bool c = cancelled;
	UNLOCK(data);
	return c;
}

Peer* const
SenderRequest::getPeer()
{
	LOCK(data);
	Peer* p = cancelled ? NULL : peer;
	UNLOCK(data);
	return p;
}

bool
SenderRequest::mustBeSkipped()
{
	LOCK(data);	
	bool b;
	if (cancelled)
		b = true;
	else if (peer->areConnecting())
		b = true;
	else
		/*
		 * If the request isn't partial, but the last send was incomplete, it means
		 * this request would interleave with the incomplete one, so it must be
		 * skipped.
		 */
		b = !isPartialRequest() && peer->wasLastSendIncomplete();
	UNLOCK(data);
	return b;
}

/* vim:set ts=2 sw=2: */
