#include <string>
#include <stdint.h>

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

	unsigned int getPieceNum() const { return num; }
	unsigned int getHave() const { return have; }
	unsigned int isHashing() const { return hashing; }
	unsigned int isQueued() const { return queued; }

private:
	unsigned int num;
	bool have, hashing, queued;
};

class PeerInfo {
public:
	PeerInfo(Peer* p);

	bool isSnubbed() const { return snubbed; }
	bool isPeerInterested() const { return peer_interested; }
	bool isPeerChoked() const { return peer_choked; }
	bool areInterested() const { return interested; }
	bool areChoking() const { return choking; }
	bool isIncoming() const { return incoming; }
	uint32_t getRxRate() const { return rx; }
	uint32_t getTxRate() const { return tx; }
	uint32_t getNumPieces() const { return num_pieces; }

	const std::string& getEndpoint() const { return endpoint; }

private:
	bool snubbed, peer_interested, peer_choked, interested, choking, incoming;
	uint32_t rx, tx, num_pieces;
	std::string endpoint;
};

class FileInfo {
public:
	FileInfo(File* f, unsigned int piece, unsigned int num);

	const std::string& getFilename() const { return fname; }
	size_t getLength() const { return length; }

	unsigned int getFirstPieceNum() const { return firstPiece; }
	unsigned int getNumPieces() const { return numPieces; }

private:
	std::string fname;
	size_t length;
	unsigned int firstPiece, numPieces;
};

#endif /* __TORTILLA_INFO_H__ */
