#include <string>
#include "exceptions.h"
#include "pendingpeer.h"
#include "torrent.h"
#include "tracer.h"

using namespace Tortilla;

#define TRACER (torrent->getTracer())

PendingPeer::PendingPeer(Torrent* t, std::string ip, uint16_t port, std::string peerid)
{
	this->torrent = t; this->ip = ip; this->port = port; this->peerid = peerid;
}

Peer*
PendingPeer::connect()
{
	try {
		TRACE(NETWORK, "trying peer: torrent=%p, address=%s, port=%lu", torrent, ip.c_str(), port);
		Peer* p = new Peer(torrent, peerid, ip, port);
		return p;
	} catch (ConnectionException e) {
		TRACE(NETWORK, "skipping peer: torrent=%p, address=%s, port=%lu, error=%s",  torrent, ip.c_str(), port, e.what());
	}
	return NULL;
}

/* vim:set ts=2 sw=2: */
