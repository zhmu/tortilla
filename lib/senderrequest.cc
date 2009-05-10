#include <assert.h>
#include <string.h>
#include "macros.h"
#include "peer.h"
#include "senderrequest.h"
#include "torrent.h"

using namespace std;

void
SenderRequest::__init(uint32_t len)
{
	length = len; skip_num = 0; piece = 0; offset = 0; piece_length = 0;

	message = new uint8_t[length];
}

SenderRequest::SenderRequest(Torrent* t, uint32_t piece, uint32_t begin, uint32_t len)
{
	__init(len + 13);
	this->piece = piece; this->offset = begin; this->piece_length = len;

	WRITE_UINT32(message, 0, len + 9);
	message[4] = PEER_MSGID_PIECE;
	WRITE_UINT32(message, 5, piece);
	WRITE_UINT32(message, 9, offset);
	t->readChunk(piece, begin, (message + 13), len);
}

SenderRequest::SenderRequest(uint8_t msg, const uint8_t* data, uint32_t len)
{
	__init(len + 5);

	WRITE_UINT32(message, 0, len + 1);
	message[4] = msg;
	memcpy((message + 5), data, len);
}

SenderRequest::SenderRequest(const uint8_t* data, uint32_t len)
{
	__init(len);

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
}


/* vim:set ts=2 sw=2: */
