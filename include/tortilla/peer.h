#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <list>
#include <stdint.h>
#include <string>
#include <vector>
#include "connection.h"

#ifndef __TORTILLA_PEER_H__
#define __TORTILLA_PEER_H__

namespace Tortilla {

class Torrent;
class SenderRequest;
class Overseer;
class Receiver;

//! \brief This is the number of requests we attempt to keep on the wire
#define PEER_MAX_OUTSTANDING_REQUESTS	20

//! \brief Amount of seconds that must pass before we snub a peer
#define PEER_SNUBBED_SECONDS 30

//! \brief Amount of seconds that must pass become we kick a peer
#define PEER_KICK_SECONDS 120

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

//! \brief Describes an outstanding request
class OutstandingChunkRequest {
public:
	OutstandingChunkRequest (unsigned int num, unsigned int begin, unsigned int len) {
		piece = num; offset = begin; length = len;
	}

	bool operator == (OutstandingChunkRequest r) const {
		return piece == r.getPiece() &&
		       offset == r.getOffset() &&
		       length == r.getLength();
	}

	unsigned int getPiece() const { return piece; }
	unsigned int getOffset() const { return offset; }
	unsigned int getLength() const { return length; }

private:
	unsigned int piece, offset, length;
};

/*! \brief A single bittorrent peer
 */
class Peer {
friend class Torrent;
friend class Sender;
friend class Overseer;
friend class Receiver;
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
	 *  \param c Connection used
	 */
	Peer(Torrent* t, Connection* c);

	//! \brief Destructs the peer
	~Peer();

	//! \brief Retrieve the file descriptor associated with this peer
	inline int getFD() const { return connection->getFD(); }

	/*! \brief Called if data is received for this peer
	 *  \returns true if the connection must be severed
	 */
	bool receive(const uint8_t* data, uint32_t data_len);

	//! \brief Retrieve the piece map of the peer
	const std::vector<bool>& getPieceMap() const { return havePiece; }

	//! \brief Retrieve the peer ID
	const std::string& getPeerID() const { return peerID; }

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
	uint32_t getRxRate() const { return rx_bytes; }

	//! \brief Retrieve transmit rate, in bytes/second
	uint32_t getTxRate() const { return tx_bytes; }

	//! \brief Retrieves the average send/transmit rate, in bytes/second
	void getAverageRate(uint32_t* rx, uint32_t* tx);

	//! \brief Must be called every second
	void timer();

	//! \brief Is this peer snubbed?
	bool isPeerSnubbed();

	//! \brief Is this peer interested?
	bool isPeerInterested() const { return peer_interested; }

	//! \brief Is this peer choked?
	bool isPeerChoked() const { return peer_choked; }

	//! \brief Are we interested in this peer?
	bool isInterested() const { return am_interested; }

	//! \brief Are we choking this peer?
	bool isChoking() const { return am_choked; }

	//! \brief Compares two peers based on upload rate
	static bool compareByUpload(Peer* a, Peer* b);

	//! \brief Called if the peer should be choked
	void choke();

	//! \brief Called if the peer should be unchoked
	void unchoke();

	//! \brief Inform a peer that we own a certain piece
	void have(unsigned int piece);

	//! \brief Check if the peer has all pieces
	bool isSeeder();

	//! \brief Request the peer to shutdown
	void shutdown();

	//! \brief Are we terminating?
	bool isShuttingDown() const { return terminating; }

	//! \brief Set the peer ID
	void setPeerID(std::string peer_id);

	//! \brief Is this an incoming connection?
	bool isIncoming() const { return incoming; }

	//! \brief Retrieve the endpoint in human-readable notation
	const std::string& getEndpoint() const { return connection->getEndpoint(); }

	//! \brief Retrieve the peer ID in human-readable notation
	std::string getID();

	//! \brief Retrieves the torrent corresponding to this peer
	Torrent* getTorrent() const { return torrent; }

	//! \brief Retrieve the number of pieces this peer has
	unsigned int getNumPeerPieces() const { return numPeerPieces; }

	//! \brief Is the peer still connecting to the endpoint?
	bool areConnecting() const { return connection->areConnecting(); }

	//! \brief Signal that we finished connecting
	void connectionDone();

	/*! \brief Cancel a request for a certain chunk in a piece */
	void cancelChunk(uint32_t piece, uint32_t offset, uint32_t len);

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

	//! \brief Queues a sending request
	void queueSenderRequest(SenderRequest* sr);

	/*! \brief Processes the first sender requeue item
	 *  \param max_length Maximum number of bytes to send, zero for unlimited
	 *  \returns Number of bytes transmitted
	 *
	 *  If the result had to be split, the resulting request will be modified.
	 */
	size_t processSenderQueue(ssize_t max_length);

	/*! \brief Cancel request for a chunk
	 *  \param piece Piece number to cancel
	 *  \param offset Offset within the piece
	 *  \param length Length of the chunk
	 */
	void cancelChunkRequest(unsigned int piece, unsigned int offset, unsigned int length);

	//! \brief Do we have something to send?
	bool isSenderQueueEmpty();

	//! \brief Process

	//! \brief Send our handshake to the peer
	void sendHandshake();

	/*! \brief Send our bitfield of available pieces to the peer
	 *
	 *  This function does nothing if no pieces are available.
	 */
	void sendBitfield();

	/*! \brief Send requests for chunks in piece
	 *  \param piece Piece to request
	 *  \returns Number of requested chunks
	 *
	 *  This function returns -1 if too much requests were outstanding, otherwise
	 *  the number of requests is returned. Zero indicates the entire chunk is
	 *  already requested.
	 */
	int sendPieceRequest(unsigned int piece);

	//! \brief Locks a peer for sending
	void lockForSending();

private:
	/*! \brief Initializes basic peer parameters based on a torrent
	 *  \param t Torrent the peer is connected to
	 *
	 *  This is designed to be called from the constructor, since C++
	 *  doesn't seem to completely support delegated constructors yet.
	 */
	void __init(Torrent* t);

	//! \brief Construct a request
	std::string constructRequest(uint32_t index, uint32_t begin, uint32_t length);

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

	//! \brief ID of the peer
	std::string peerID;

	//! \brief Torrent we are linked to
	Torrent* torrent;

	//! \brief Connection to the peer
	Connection* connection;

	//! \brief Current command buffer
	uint8_t command_buffer[PEER_BUFFER_SIZE];

	//! \brief Number of bytes in the command buffer
	uint32_t command_buffer_readpos, command_buffer_writepos;

	//! \brief Amount of data sent / recieved during the last cycle
	uint32_t tx_bytes, rx_bytes;

	//! \brief Amount of data sent / received during the peers lifetime
	uint64_t tx_total, rx_total;

	//! \brief Timestamp of peer launch
	time_t launchTime;

	//! \brief Timestamp when we last heard from the peer
	time_t lastTime;

	//! \brief Total number of pieces this peer has
	unsigned int numPeerPieces;

	//! \brief Is this peer terminating?
	bool terminating;

	//! \brief Is this an incoming connection?
	bool incoming;

	//! \brief Chunks we are currently requesting
	std::list<OutstandingChunkRequest> chunk_requests;

	//! \brief Requests that need to be sent
	std::list<SenderRequest*> send_queue;

	//! \brief Mutex protecting the peer data
	boost::mutex mtx_data;

	/*! \brief Mutex indicating we are sending
	 *
	 *  This mutex is used to safely remove a Peer; if the sender
	 *  is currently transmitting something, this mutex is used to wait
	 *  until it's gone.
	 */
	boost::mutex mtx_sending;

	//! \brief Mutex protecting the queue
	boost::shared_mutex rwl_send_queue;
};

//! \brief Helper object, used to determine whether a peer matches
class peervector_matches {
public:
	peervector_matches(Peer* p) { peer = p; }

	bool operator() (const Peer* p) { return p == peer; }

private:
	Peer* peer;
};

//! \brief List of peers
typedef std::list<Peer*> PeerList;

}

#endif /* __TORTILLA_PEER_H__ */
