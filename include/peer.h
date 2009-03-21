#include <list>
#include <stdint.h>
#include <string>
#include <vector>
#include "connection.h"

#ifndef __PEER_H__
#define __PEER_H__

class Torrent;
class UploadRequest;

#define PEER_MAX_OUTSTANDING_REQUESTS	5

//! \brief Amount of seconds that must pass before we snub a peer
#define PEER_SNUBBED_SECONDS 30

/*! \brief Length of the buffer used to cache incomplete commands
 *
 *  This should be 2 * max command length.
 */
#define PEER_BUFFER_SIZE		(131072)

#define PEER_PSTR "BitTorrent protocol"

#define PEER_MSGID_CHOKE		0x0
#define PEER_MSGID_UNCHOKE		0x1
#define PEER_MSGID_INTERESTED		0x2
#define PEER_MSGID_NOTINTERESTED	0x3
#define PEER_MSGID_HAVE			0x4
#define PEER_MSGID_BITFIELD		0x5
#define PEER_MSGID_REQUEST		0x6
#define PEER_MSGID_PIECE		0x7
#define PEER_MSGID_CANCEL		0x8
#define PEER_MSGID_PORT			0x9

/*! \brief A single bittorrent peer
 */
class Peer {
public:	
	/*! \brief Constructs a new peer object for an outgoing connection
	 *  \param t Torrent the peer is connected to
	 *  \param peer_id ID of the peer we are connecting to
	 *  \param peer_host Hostname/IP of the peer
	 *  \param peer_port Port of the peer
	 */
	Peer(Torrent* t, std::string peer_id, std::string peer_host, uint16_t peer_port);

	/*! \brief Constructs a new peer object for an incoming connection
	 *  \param t Torrent the peer is connected to
	 *  \param peer_id Peer ID of the peer
	 *  \param c Connection used
	 */
	Peer(Torrent* t, std::string peer_id, Connection* c);

	//! \brief Destructs the peer
	~Peer();

	//! \brief Retrieve the file descriptor associated with this peer
	inline int getFD() { return connection->getFD(); }

	/*! \brief Called if data is received for this peer
	 *  \returns true if the connection must be severed
	 */
	bool receive(const uint8_t* data, uint32_t data_len);

	//! \brief Retrieve the piece map of the peer
	std::vector<bool>& getPieceMap() { return havePiece; }

	//! \brief How much pieces has this peer requested?
	unsigned int getNumRequests();

	//! \brief Called if we should express interest in this peer
	void claimInterest();

	//! \brief Called if we should revoke interest in this peer
	void revokeInterest();

	/*! \brief Called if we should request a piece from this peer
	 *  \param num Piece to request
	 */
	void requestPiece(unsigned int num);

	/*! \brief Called if we should cancel a piece from this peer
	 *  \param num Piece to cancel
	 */
	void cancelPiece(unsigned int num);

	//! \brief Check if the peer has a specific piece
	bool hasPiece(unsigned int num);

	//! \brief Retrieve receive rate, in bytes/second
	uint32_t getRxRate() { return rx_bytes; }

	//! \brief Retrieve transmit rate, in bytes/second
	uint32_t getTxRate() { return tx_bytes; }

	//! \brief Processes an upload request
	void processUploadRequest(UploadRequest* request);

	//! \brief Must be called every second
	void timer();

	//! \brief Is this peer snubbed?
	bool isPeerSnubbed();

	//! \brief Is this peer interested?
	bool isPeerInterested() { return peer_interested; }

	//! \brief Is this peer choked?
	bool isPeerChoked() { return peer_choked; }

	//! \brief Compares two peers based on upload rate
	static bool compareByUpload(Peer* a, Peer* b);

	//! \brief Called if the peer should be unchoked
	void unchoke();

	void dump();

protected:
	//! \brief Handles a 'choke' message
	bool msgChoke();

	//! \brief Handles an 'unchoke' message
	bool msgUnchoke();

	//! \brief Handles an 'interested' message
	bool msgInterested();

	//! \brief Handles a 'notinterested' message
	bool msgNotInterested();

	//! \brief Handles a 'have' message
	bool msgHave(const uint8_t* msg, uint32_t len);

	//! \brief Handles a 'bitfield' message
	bool msgBitfield(const uint8_t* msg, uint32_t len);

	//! \brief Handles a 'request' message
	bool msgRequest(const uint8_t* msg, uint32_t len);

	//! \brief Handles a 'piece' message
	bool msgPiece(const uint8_t* msg, uint32_t len);

	//! \brief Handles a 'cancel' message
	bool msgCancel(const uint8_t* msg, uint32_t len);

	/*! \brief Send a message to the peer
	 *  \param msg Message to send
	 *  \param data Payload of the message, if any
	 *  \param len Length of message
	 */
	void sendMessage(uint8_t msg, const uint8_t* data, size_t len);

	//! \brief Send our handshake to the peer
	void sendHandshake();

	/*! \brief Send our bitfield of available pieces to the peer
	 *
	 *  This function does nothing if no pieces are available.
	 */
	void sendBitfield();

private:
	/*! \brief Initializes basic peer parameters based on a torrent
	 *  \param t Torrent the peer is connected to
	 *
	 *  This is designed to be called from the constructor, since C++
	 *  doesn't seem to compeletely support delegated constructors yet.
	 */
	void __init(Torrent* t);

	//! \brief Construct a request
	std::string constructRequest(uint32_t index, uint32_t begin, uint32_t length);

	//! \brief Send request for a piece, if any
	void sendPieceRequest();

	/*! \brief Sends data to peer
	 *  \param data Data to send
	 *  \param len Length of the data, in bytes
	 */
	void send(const uint8_t* data, size_t len);

	//! \brief Are we choked?
	bool am_choked;

	//! \brief Are we interested?
	bool am_interested;

	//! \brief Is the peer choked?
	bool peer_choked;

	//! \brief Is the peer interested?
	bool peer_interested;

	//! \brief Are we waiting for the protocol handshake?
	bool handshaking;

	//! \brief Which pieces does this peer have?
	std::vector<bool> havePiece;

	//! \brief Which pieces are we requesting from this peer?
	std::list<unsigned int> requestedPieces;

	//! \brief ID of the peer
	std::string peerID;

	//! \brief Torrent we are linked to
	Torrent* torrent;

	//! \brief Connection to the peer
	Connection* connection;

	//! \brief Number of outstanding requests
	int numOutstandingRequests;

	//! \brief Current command buffer
	uint8_t command_buffer[PEER_BUFFER_SIZE];

	//! \brief Number of bytes in the command buffer
	uint32_t command_buffer_readpos, command_buffer_writepos;

	//! \brief Amount of data recieved during the last cycle
	uint32_t rx_bytes;

	//! \brief Amount of data sent during the last cycle
	uint32_t tx_bytes;

	/*! \brief Amount of seconds this peer has left before it's snubbed.
	 *
	 *  A snubbed peer is a peer that hasn't sent any block
	 *  in the last 30 seconds.
	 */
	unsigned int snubbedLeftoverCounter;
};

#endif /* __PEER_H__ */
