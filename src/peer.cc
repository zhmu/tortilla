#include <string.h>
#include "connection.h"
#include "peer.h"
#include "torrent.h"

using namespace std;

Peer::Peer(Torrent* t, std::string my_id, std::string peer_id, std::string peer_host, uint16_t peer_port)
{
	torrent = t; choked = true; interested = false; handshaking = true;
	prepend = ""; peerID = peerID; 

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

bool
Peer::receive(std::string data)
{
	if (handshaking) {
		const char* c = data.c_str();
		int pstrlen = strlen(PEER_PSTR);

		/* Safety measure: ensure the handshake is at least present in our string */
		if (data.size() < 1 + pstrlen + 8 + TORRENT_HASH_LEN + TORRENT_HASH_LEN)
			return true;
		if (c[0] != pstrlen)
			return true;
		if (strncmp((const char*)(c + 1), PEER_PSTR, pstrlen))
			return true;
		/* XXX ignore 8 feature bytes for now */
		if (data.substr(1 + pstrlen + 8, TORRENT_HASH_LEN) != torrent->getInfoHash())
			return true;
		/*
		 * We don't check the peer ID, since Azureus seem to possibly anonymize it.
		 * If we reach this point, we know the protocol and the torrent info hash
		 * matches, which should be enough for us. To this extent, we just nuke the
		 * handshake string and see if there is anything else to handle.
		 */
		handshaking = false;
		data = data.substr(1 + pstrlen + 8 + TORRENT_HASH_LEN + TORRENT_HASH_LEN);
		if (data.size() == 0)
			return false;
	}

	/*
	 * If we got here, input looks like:
	 * [4 bytes] = length, if 0, it's a keepalive
	 * [1 byte ] = command
	 * [ ...   ] = <varying>
	 */
	data = prepend + data;
	const char* c = data.c_str();
	if (data.size() < 4) {
		/* Too little data */
		prepend += data;
		return false;
	}
	uint32_t len = c[0] << 24 | c[1] << 16 | c[2] << 8 | c[3];
	if (len > data.size() - 4) {
		/*
		 * We do not have all data; prepend our data buffer with the next
		 * data, in the hopes we'll get a full message next time.
		 */
		prepend += data;
		return false;
	}
	data = data.substr(4);

	/* Seems we have a complete message */
	if (data.size() - 4 >= len) {
		/* We have data that we can pass next time! */
		prepend += data.substr(len);
	} else {
		/*
		 * XXX We have all the data we need - remove it from the prepend part, if
		 * any.
		 */
		prepend = "";
	}

	for (int i = 0; i < data.size(); i++) {
		printf("%02x ", (unsigned char)data[i]);
	}

	return false;
}

/* vim:set ts=2 sw=2: */
