#include <assert.h>
#include <string.h>
#include "connection.h"
#include "peer.h"
#include "torrent.h"

using namespace std;

#define MIN(a,b) \
	(((a) < (b)) ? (a) : (b))

#define WRITE_UINT32(ptr,offs,val) \
	ptr[(offs) + 0] = (((val) >> 24) & 0xff); \
	ptr[(offs) + 1] = (((val) >> 16) & 0xff); \
	ptr[(offs) + 2] = (((val) >>  8) & 0xff); \
	ptr[(offs) + 3] = (((val)      ) & 0xff);

#define READ_UINT32(ptr,offs) \
	(uint32_t)((((ptr)[(offs) + 0]) << 24) | \
	           (((ptr)[(offs) + 1]) << 16) | \
	           (((ptr)[(offs) + 2]) <<  8) | \
	           (((ptr)[(offs) + 3])      ))

Peer::Peer(Torrent* t, std::string my_id, std::string peer_id, std::string peer_host, uint16_t peer_port)
{
	torrent = t; am_choked = true; am_interested = false; handshaking = true;
	peer_choked = true; peer_interested = false; numOutstandingRequests = 0;
	command_buffer_readpos = 0; command_buffer_writepos = 0;
	peerID = peerID;

	/* Assume the peer doesn't have any pieces */
	havePiece.reserve(t->getNumPieces());
	for (int i = 0; i < t->getNumPieces(); i++)
		havePiece.push_back(false);

	connection = new Connection(peer_host, peer_port);

	/*
	 * Construct the handshake: length, protocolid, capabilities, info hash
	 *                          our id
	 */
	string handshake;
	unsigned char ch;
	ch = strlen(PEER_PSTR);
	handshake.append((const char*)&ch, 1);
	handshake += PEER_PSTR;
	ch = 0;
	for (int i = 0; i < 8; i++)
		handshake.append((const char*)&ch, 1);
	handshake += torrent->getInfoHash();
	handshake += my_id;

	/* Hi! */
	connection->write(handshake.c_str(), handshake.size());
}

#define DATA_LEFT \
		((command_buffer_readpos < command_buffer_writepos) ? \
			(command_buffer_writepos - command_buffer_readpos) : \
			((PEER_BUFFER_SIZE - command_buffer_readpos) + command_buffer_writepos))

bool
Peer::receive(const uint8_t* data, uint32_t data_len)
{
	assert (data_len > 0);

	/*
	 * First of all, check how much data we can still place into the buffer. If we run
	 * out, this means the client is trying to send a too large message, and we'd
	 * better give up.
	 */
	if (((PEER_BUFFER_SIZE - command_buffer_writepos) + command_buffer_readpos) < data_len) {
		cerr << "out of space to store data received from peer, closing connection" << endl;
		return true;
	}

	/*
	 * Need to store the peer's data now. First of all, try to use the chunk from
	 * write_pos ... buffer_size.
	 */
	uint32_t write_chunk = MIN((PEER_BUFFER_SIZE - command_buffer_writepos), data_len);
	memcpy((uint8_t*)(command_buffer + command_buffer_writepos), data, write_chunk);
	data_len -= write_chunk; data += write_chunk;
	command_buffer_writepos = (command_buffer_writepos + write_chunk) % PEER_BUFFER_SIZE;

	/*
	 * If we still have data to write, we this means we need to write to the
	 * part 0 ... read_pos, which is unused as well.
	 */
	if (data_len > 0) {
		memcpy((uint8_t*)(command_buffer + command_buffer_writepos), data, data_len);
		command_buffer_writepos = (command_buffer_writepos + data_len) % PEER_BUFFER_SIZE;
	}

	/* All data is in place; try to handle commands! */
	while (1) {
		uint32_t data_left = DATA_LEFT;
		if (handshaking) {
			/* Only continue if we have at least the entire handshake string */
			int pstrlen = strlen(PEER_PSTR);
			uint32_t handshake_len = 1 + pstrlen + 8 + TORRENT_HASH_LEN + TORRENT_HASH_LEN;
			if (data_left < handshake_len)
				return true;

			/* XXX we assume the buffer doesn't wrap yet */
			data = (uint8_t*)(command_buffer + command_buffer_readpos);
			if (data[0] != pstrlen)
				return true;
			if (memcmp((data + 1), PEER_PSTR, pstrlen))
				return true;
			/* XXX ignore 8 feature bytes for now */
			if (memcmp((uint8_t*)(data + 1 + pstrlen + 8), torrent->getInfoHash().c_str(), TORRENT_HASH_LEN))
				return true;

			/*
			 * We don't check the peer ID, since Azureus seem to possibly anonymize it.
			 * If we reach this point, we know the protocol and the torrent info hash
			 * matches, which should be enough for us. To this extent, we just nuke the
			 * handshake string and see if there is anything else to handle.
			 */
			handshaking = false;
			command_buffer_readpos = (command_buffer_readpos + handshake_len) % PEER_BUFFER_SIZE;
			data_left -= handshake_len;
		}

		/* Only try something if we have at least the length */
		if (data_left < 4)
			break;

		/*
		 * readpos points the following:
		 *  [4 bytes] = length, if 0, it's a keepalive
		 *  [1 byte ] = command            \
		 *  [ ...   ] = <varying>          / total [length] bytes
		 *
		 * Below, we grab the length byte (note that we *mustn't* remove it from
		 * the buffer, since we don't yet know if we have received a complete
		 * command.
		 */
		uint32_t len = 
			(uint32_t)(
				(command_buffer[(command_buffer_readpos + 0) % PEER_BUFFER_SIZE] << 24) |
				(command_buffer[(command_buffer_readpos + 1) % PEER_BUFFER_SIZE] << 16) |
				(command_buffer[(command_buffer_readpos + 2) % PEER_BUFFER_SIZE] <<  8) |
				(command_buffer[(command_buffer_readpos + 3) % PEER_BUFFER_SIZE])
			);
		if (len == 0) {
			/* Keepalive; skip the length bytes and continue */
			command_buffer_readpos = (command_buffer_readpos + 4) % PEER_BUFFER_SIZE;
			continue;
		}

		/* Find out how much data we have available in this buffer */
		if (data_left < len + 4 /* 4 because we haven't skipped the header yet! */) {
			/* We need more data */
			return false;
		}

		/*
		 * We have a complete message of at least one byte. Throw away the length
		 * prefix as we have stored it already.
		 */
		command_buffer_readpos = (command_buffer_readpos + 4) % PEER_BUFFER_SIZE;

		/* Extract the message ID and data payload */
		uint8_t msg = command_buffer[command_buffer_readpos];
		command_buffer_readpos = (command_buffer_readpos + 1) % PEER_BUFFER_SIZE;

		len--; /* skip command */

		const uint8_t* ptr = (const uint8_t*)(command_buffer + command_buffer_readpos);
		if (command_buffer_readpos + len > PEER_BUFFER_SIZE) {
			/*
			 * This message has arguments that expand past the end of the buffer
			 * (i.e. they are stored at the beginning). As we need to pass the
			 * payload sequentially, we need to copy the two parts in place.
			 *
			 *          +----------------+
			 *          |2222222222222222|
			 *          +----------------+ <-- mark = (readpos + len) % SIZE
			 *          |                |
			 *          |                | mark <= writepos <= readpos
			 *          |                |
			 *          +----------------+ <-- readpos
			 *          |1111111111111111| pre = SIZE - readpos
			 *          +----------------+
			 *
			 * We need to rewrite this to:
			 *
			 *          +----------------+
			 *          |                |
			 *          |                |
			 *          |1111111111111111| <-- length = pre
			 *          |2222222222222222| <-- length = mark
			 *          +----------------+
			 *
			 * Note: mark + pre == len
			 */
			uint32_t pre = PEER_BUFFER_SIZE - command_buffer_readpos;
			uint32_t mark = (command_buffer_readpos + len) % PEER_BUFFER_SIZE;

			/* Ensure we will not overwrite unprocessed written data */
			assert(command_buffer_writepos < PEER_BUFFER_SIZE - len);

			/*
			 * Place the entire packet at size - len ... size of the buffer; this is
			 * guaranteed to be unused by the previous assertion.
			 */
			memcpy((uint8_t*)(command_buffer + PEER_BUFFER_SIZE - len), (command_buffer + command_buffer_readpos), pre);
			memcpy((uint8_t*)(command_buffer + PEER_BUFFER_SIZE - len + pre), (command_buffer), mark);
			ptr = (const uint8_t*)(command_buffer + PEER_BUFFER_SIZE - len);
		}

		bool disconnect;
		switch (msg) {
			case PEER_MSGID_CHOKE:
				disconnect = msgChoke();
				break;
			case PEER_MSGID_UNCHOKE:
				disconnect = msgUnchoke();
				break;
			case PEER_MSGID_INTERESTED:
				disconnect = msgInterested();
				break;
			case PEER_MSGID_NOTINTERESTED:
				disconnect = msgNotInterested();
				break;
			case PEER_MSGID_HAVE:
				disconnect = msgHave(ptr, len);
				break;
			case PEER_MSGID_BITFIELD:
				disconnect = msgBitfield(ptr, len);
				break;
			case PEER_MSGID_REQUEST:
				disconnect = msgRequest(ptr, len);
				break;
			case PEER_MSGID_PIECE:
				disconnect = msgPiece(ptr, len);
				break;
			case PEER_MSGID_CANCEL:
				disconnect = msgCancel(ptr, len);
				break;
			default:
				/* Unknown command, ignore */
				disconnect = false;
				assert(0);
				break;
		}

		/* If we can disconnect the user, can't find a reason not to */
		if (disconnect)
			return true;

		/* Remove message payload from the input buffer */
		command_buffer_readpos = (command_buffer_readpos + len) % PEER_BUFFER_SIZE;

		/* If we ran out of data, wait until more arrives */
		if (command_buffer_readpos == command_buffer_writepos) {
			/*
			 * Reset the buffer to the start. While this makes no difference from a
			 * correctness point of view, it generally removes the need for message
			 * reconstruction since they will be stored sequentially as much as
			 * possible.
			 */
			command_buffer_readpos = 0; command_buffer_writepos = 0;
			break;
		}
	}
	return false;
}

#undef DATA_LEFT

bool
Peer::msgChoke()
{
	cout << peerID + ": " + "choke" << endl;

	am_choked = true;

#if 1
	/* XXX why are we choked?? */
	dump();
	torrent->dump();
#endif
	return false;
}

bool
Peer::msgUnchoke()
{
	cout << peerID + ": " + "unchoke" << endl;

	/*
	 * We are unchoked! This means we can request pieces from this peer; so send
	 * the first few.
	 */
	am_choked = false;
	sendPieceRequest();
	return false;
}

bool
Peer::msgInterested()
{
	cout << peerID + ": " + "interested" << endl;
	return false;
}

bool
Peer::msgNotInterested()
{
	cout << peerID + ": " + "notinterested" << endl;
	return false;
}

bool
Peer::msgHave(const uint8_t* msg, uint32_t len)
{
	cout << peerID + ": " + "have" << endl;
	return false;
}

bool
Peer::msgBitfield(const uint8_t* msg, uint32_t len)
{
	cout << peerID + ": " + "bitfield" << endl;

	/*
	 * This message indicates the peer wants to inform us about the pieces it
	 * has. First of all, ensure the number of pieces it reports match up with
	 * ours; if not, we must sever the connection.
	 */
	unsigned int numPieces = torrent->getNumPieces();
	if (len != (numPieces / 8) + ((numPieces % 8) ? 1 : 0))
		/* Size mismatch! */
		return true;

	/*
	 * Wade through the bits and wack our own as needed: bit 7 represents
	 * piece n, bit 6 piece n + 1 etc, so we generally need to invert the
	 * pattern (this is the reason we use a vector of bool, so we don't need
	 * to worry about this stuff elsewhere)
	 */
	vector<unsigned int> newPieces;
	for (unsigned int i = 0; i < numPieces; i++) {
		havePiece[i] = msg[i / 8] & (1 << (7 - (i % 8)));
		newPieces.push_back(i);
	}

	/* Inform the torrent class of all added pieces */
	torrent->callbackPiecesAdded(this, newPieces);
	return false;
}

bool
Peer::msgRequest(const uint8_t* msg, uint32_t len)
{
	cout << peerID + ": " + "request" << endl;
	return false;
}

bool
Peer::msgPiece(const uint8_t* msg, uint32_t len)
{
	cout << peerID + ": " + "piece" << endl;
	if (len < 9)
		return true;

	uint32_t index = READ_UINT32(msg, 0);
	uint32_t begin = READ_UINT32(msg, 4);
	const uint8_t* data = (msg + 8);
	len -= 8;
	cout << "got index "; cout << index;
	cout << " begin "; cout << begin;
	cout << " len "; cout << len;
	cout << endl;

	if (numOutstandingRequests > 0)
		numOutstandingRequests--;

	if (len > TORRENT_CHUNK_SIZE || begin % TORRENT_CHUNK_SIZE != 0) {
		/* Not what we hoped for... */
		cout << "piece fail: ";
		if (len > TORRENT_CHUNK_SIZE)
			cout << "length ";
		if (begin % TORRENT_CHUNK_SIZE != 0)
			cout << "begin";
		cout << endl;
		sendPieceRequest();
		return false;
	}
	torrent->callbackCompleteChunk(this, index, begin, data, len);

	/* Thanks! Try to get more! */
	sendPieceRequest();
	return false;
}

bool
Peer::msgCancel(const uint8_t* msg, uint32_t len)
{
	cout << peerID + ": " + "cancel" << endl;
	return false;
}

void
Peer::claimInterest()
{
	/* If we are already interested, don't bother expressing this again */
	if (am_interested)
		return;

	cout << peerID + ": " + "claimed interest" << endl;

	sendMessage(PEER_MSGID_INTERESTED, NULL, 0);
	am_interested = true;
}

void
Peer::requestPiece(unsigned int num)
{
	requestedPieces.push_back(num);
}

void
Peer::sendMessage(uint8_t msg, const uint8_t* buf, size_t len)
{
	unsigned char hdr[5];

	assert(buf == NULL || len > 0);

	/* Construct a length header and message */
	WRITE_UINT32(hdr, 0, len + 1);
	hdr[4] = msg;

	/* Write them both */
	connection->write(hdr, 5);
	if (len > 0)
		connection->write(buf, len);
}

bool
Peer::hasPiece(unsigned int num)
{
	return havePiece[num];
}

unsigned int
Peer::getNumRequests()
{
	return requestedPieces.size();
}

void
Peer::sendPieceRequest()
{
	/*
	 * If we are choked, there's no reason to try to send requests, so don't do
	 * it.
	 */
	if (am_choked)
		return;

	/* Don't flood a peer with requests */
	if (numOutstandingRequests >= PEER_MAX_OUTSTANDING_REQUESTS)
		return;

	while (requestedPieces.size() > 0 && numOutstandingRequests < PEER_MAX_OUTSTANDING_REQUESTS) {
		unsigned int piece = requestedPieces.front();

		/* If we are requesting a piece, the peer should have it */
		assert(havePiece[piece] == true);

		/*
		 * Find the first chunk of this piece we are missing.
		 */
		int missingChunk = torrent->getMissingChunk(piece);
		if (missingChunk < 0) {
			/* We have the entire chunk - nuke it from the list */
			requestedPieces.pop_front();

			/* Inform the torrent, may be more to schedule */
			torrent->callbackCompletePiece(this, piece);

			/* Try the next chunk */
			continue;
		}

		/* If we have there piece here, we fail! */
		assert(torrent->havePiece[piece] == false);

		/* Fetch boy, fetch */
		uint8_t msg[12];
		WRITE_UINT32(msg, 0, piece);
		WRITE_UINT32(msg, 4, missingChunk * TORRENT_CHUNK_SIZE);
		WRITE_UINT32(msg, 8, TORRENT_CHUNK_SIZE);
		torrent->setChunkRequested(piece, missingChunk, true);
		sendMessage(PEER_MSGID_REQUEST, msg, 12);

		numOutstandingRequests++;
	}
}

void
Peer::dump()
{
	cerr << "outstanding requests: "; cerr << numOutstandingRequests; cerr << endl;
}

#undef WRITE_UINT32

/* vim:set ts=2 sw=2: */
