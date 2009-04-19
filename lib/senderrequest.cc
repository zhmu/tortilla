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

SenderRequest::SenderRequest(Peer* p, uint32_t piece, uint32_t begin, uint32_t len)
{
	peer = p; length = len + 13; skip_num = 0; cancelled = false;
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
	cancelled = false;

	message = new uint8_t[length];
	WRITE_UINT32(message, 0, len + 1);
	message[4] = msg;
	memcpy((message + 5), data, len);
}

SenderRequest::SenderRequest(Peer* p, const uint8_t* data, uint32_t len)
{
	peer = p; length = len; skip_num = 0; piece = 0; offset = 0; piece_length = 0;
	cancelled = false;

	message = new uint8_t[length];
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
