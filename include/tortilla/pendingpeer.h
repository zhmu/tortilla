#include <stdint.h>
#include <string>
#include "peer.h"

#ifndef __TORTILLA_PENDINGPEER_H__
#define __TORTILLA_PENDINGPEER_H__

/*! \brief Implements a pending peer
 *
 *  A pending peer is a peer reported by the tracker, but not yet used; they
 *  are used to keep a list of potentiel peers should we need more.
 */
class PendingPeer {
public:
	//! \brief Construct a new peer
	PendingPeer(Torrent* t, std::string ip, uint16_t port, std::string peerid);

	/*! \brief Connect to the peer
	 *  \returns A Peer object
	 *
	 *  The new peer will be connecting to the endpoint, handing it off to
	 *  the torrent peer list will be fine.
	 */
	Peer* connect();

private:
	//! \brief Torrent object the peer is bound to
	Torrent* torrent;

	//! \brief IP adres of the peer
	std::string ip;

	//! \brief Port number to use
	uint16_t port;

	//! \brief ID of the new peer, if known (may be blank)
	std::string peerid;
};

#endif /* __TORTILLA_PENDINGPEER_H__ */
