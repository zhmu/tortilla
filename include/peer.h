#include <string>
#include "connection.h"

#ifndef __PEER_H__
#define __PEER_H__

class Torrent;

#define PEER_PSTR "BitTorrent protocol"

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

private:
	//! \brief Are we choked?
	bool choked;

	//! \brief Are we interested?
	bool interested;

	//! \brief Are we waiting for the protocol handshake?
	bool handshaking;

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
