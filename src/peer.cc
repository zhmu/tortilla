#include "peer.h"

Peer::Peer(Torrent* t, std::string peer_id, std::string peer_host, uint16_t peer_port)
{
	torrent = t; choked = true; interested = false;
	peerID = peerID;
}
