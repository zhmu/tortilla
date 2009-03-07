#include <string>

#ifndef __PEER_H__
#define __PEER_H__

class Torrent;

/*! \brief A single bittorrent peer
 */
class Peer {
public:	
	/*! \brief Constructs a new peer object
	 *  \param t Torrent the peer is connected to
	 *  \param peer_id ID of the peer we are connecting to
	 *  \param peer_host Hostname/IP of the peer
	 *  \param peer_port Port of the peer
	 */
	Peer(Torrent* t, std::string peer_id, std::string peer_host, uint16_t peer_port);

private:
	//! \brief Are we choked?
	bool choked;

	//! \brief Are we interested?
	bool interested;

	//! \brief ID of the peer
	std::string peerID;

	//! \brief Torrent we are linked to
	Torrent* torrent;
};

#endif /* __PEER_H__ */
