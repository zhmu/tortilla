#include <string>
#include <vector>
#include "connection.h"

#ifndef __PEER_H__
#define __PEER_H__

class Torrent;

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
	/*! \brief Constructs a new peer object
	 *  \param t Torrent the peer is connected to
	 *  \param my_id Our own peer ID
	 *  \param peer_id ID of the peer we are connecting to
	 *  \param peer_host Hostname/IP of the peer
	 *  \param peer_port Port of the peer
	 */
	Peer(Torrent* t, std::string my_id, std::string peer_id, std::string peer_host, uint16_t peer_port);

	//! \brief Retrieve the file descriptor associated with this peer
	inline int getFD() { return connection->getFD(); }

	/*! \brief Called if data is received for this peer
	 *  \returns true if the connection must be severed
	 */
	bool receive(std::string data);

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
	bool msgHave(std::string data);

	//! \brief Handles a 'bitfield' message
	bool msgBitfield(std::string data);

	//! \brief Handles a 'request' message
	bool msgRequest(std::string data);

	//! \brief Handles a 'piece' message
	bool msgPiece(std::string data);

	//! \brief Handles a 'cancel' message
	bool msgCancel(std::string data);

private:
	//! \brief Are we choked?
	bool choked;

	//! \brief Are we interested?
	bool interested;

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

	//! \brief Data that needs to be prepended from the previous time
	std::string prepend;
};

#endif /* __PEER_H__ */
