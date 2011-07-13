#include "peer.h"
#include "info.h"
#include "file.h"

using namespace Tortilla;

PeerInfo::PeerInfo(Peer* p)
{
	snubbed = p->isPeerSnubbed();
	peer_interested = p->isPeerInterested();
	peer_choked = p->isPeerChoked();
	interested = p->isInterested();
	choking = p->isChoking();
	incoming = p->isIncoming();
	p->getAverageRate(&rx, &tx);
	endpoint = p->getEndpoint();
	num_pieces = p->getNumPeerPieces();
}

FileInfo::FileInfo(File* f, unsigned int piece, unsigned int num)
{
	fname = f->getFilename();
	length = f->getLength();
	firstPiece = piece; numPieces = num;
}

/* vim:set ts=2 sw=2: */
