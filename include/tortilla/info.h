#include <string>

class Peer;
class File;

#ifndef __TORTILLA_INFO_H__
#define __TORTILLA_INFO_H__

class PieceInfo {
public:
	PieceInfo(unsigned int num, bool have, bool hashing, bool queued) {
		this->num = num; this->have = have;
		this->hashing = hashing; this->queued = queued;
	}

	unsigned int getPieceNum() { return num; }
	unsigned int getHave() { return have; }
	unsigned int isHashing() { return hashing; }
	unsigned int isQueued() { return queued; }

private:
	unsigned int num;
	bool have, hashing, queued;
};

class PeerInfo {
public:
	PeerInfo(Peer* p);

	bool isSnubbed() { return snubbed; }
	bool isPeerInterested() { return peer_interested; }
	bool isPeerChoked() { return peer_choked; }
	bool areInterested() { return interested; }
	bool areChoking() { return choking; }
	bool isIncoming() { return incoming; }
	uint32_t getRxRate() { return rx; }
	uint32_t getTxRate() { return tx; }
	uint32_t getNumPieces() { return num_pieces; }

	std::string getEndpoint() { return endpoint; }

private:
	bool snubbed, peer_interested, peer_choked, interested, choking, incoming;
	uint32_t rx, tx, num_pieces;
	std::string endpoint;
};

class FileInfo {
public:
	FileInfo(File* f, unsigned int piece, unsigned int num);

	std::string getFilename() { return fname; }
	size_t getLength() { return length; }

	unsigned int getFirstPieceNum() { return firstPiece; }
	unsigned int getNumPieces() { return numPieces; }

private:
	std::string fname;
	size_t length;
	unsigned int firstPiece, numPieces;
};

#endif /* __TORTILLA_INFO_H__ */
