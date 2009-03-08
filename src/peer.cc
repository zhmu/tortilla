#include <string.h>
#include "connection.h"
#include "peer.h"
#include "torrent.h"

using namespace std;

Peer::Peer(Torrent* t, std::string my_id, std::string peer_id, std::string peer_host, uint16_t peer_port)
{
	torrent = t; choked = true; interested = false; handshaking = true;
	prepend = ""; peerID = peerID; 

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

bool
Peer::receive(std::string data)
{
	if (handshaking) {
		const char* c = data.c_str();
		int pstrlen = strlen(PEER_PSTR);

		/*
		 * Safety measure: ensure the handshake is at least present in our string.
		 * XXX this breaks when peers don't send the greeter in a single go!
		 */
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
			/* No more data left */
			return false;
	}

	/*
	 * If we got here, input looks like:
	 * [4 bytes] = length, if 0, it's a keepalive
	 * [1 byte ] = command
	 * [ ...   ] = <varying>
	 */
	string fulldata = prepend + data;
	const char* c = fulldata.c_str();
	if (fulldata.size() < 4) {
		/* Too little data */
		prepend += data;
		return false;
	}

	uint32_t len = c[0] << 24 | c[1] << 16 | c[2] << 8 | c[3];
	if (len > fulldata.size() - 4) {
		/*
		 * We do not have all data; prepend our data buffer with the next
		 * data, in the hopes we'll get a full message next time.
		 */
		prepend += data;
		return false;
	}

	/*
	 * We have a complete message; throw away the length prefix as we have
	 * stored it already.
	 */
	fulldata = fulldata.substr(4);
	if (fulldata.size() > len) {
		/*
		 * We have more data than we need; better handle it next time.
		 */
		prepend = fulldata.substr(len);
	} else {
		/*
		 * We have all the data we need - remove it from the prepend part, if any.
		 */
		prepend = "";
	}

	/* Extract the message ID and data payload */
	uint8_t msg = fulldata.c_str()[0];
	fulldata = fulldata.substr(1);
	switch (msg) {
		case PEER_MSGID_CHOKE:
			return msgChoke();
		case PEER_MSGID_UNCHOKE:
			return msgUnchoke();
		case PEER_MSGID_INTERESTED:
			return msgInterested();
		case PEER_MSGID_NOTINTERESTED:
			return msgNotInterested();
		case PEER_MSGID_HAVE:
			return msgHave(fulldata);
		case PEER_MSGID_BITFIELD:
			return msgBitfield(fulldata);
		case PEER_MSGID_REQUEST:
			return msgRequest(fulldata);
		case PEER_MSGID_PIECE:
			return msgPiece(fulldata);
		case PEER_MSGID_CANCEL:
			return msgCancel(fulldata);
	}

	/* ? */

#if 0
	printf("datalen=%u,len=%u\n", fulldata.size(), len);
	for (int i = 0; i < len; i++) {
		printf("%02x ", (unsigned char)fulldata[i]);
	}
	printf("\n");
#endif

	return false;
}

bool
Peer::msgChoke()
{
	cout << peerID + ": " + "choke" << endl;
	return false;
}

bool
Peer::msgUnchoke()
{
	cout << peerID + ": " + "unchoke" << endl;
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
Peer::msgHave(string data)
{
	cout << peerID + ": " + "have" << endl;
	return false;
}

bool
Peer::msgBitfield(string data)
{
	cout << peerID + ": " + "bitfield" << endl;
	return false;
}

bool
Peer::msgRequest(string data)
{
	cout << peerID + ": " + "request" << endl;
	return false;
}

bool
Peer::msgPiece(string data)
{
	cout << peerID + ": " + "piece" << endl;
	return false;
}

bool
Peer::msgCancel(string data)
{
	cout << peerID + ": " + "cancel" << endl;
	return false;
}

/* vim:set ts=2 sw=2: */
