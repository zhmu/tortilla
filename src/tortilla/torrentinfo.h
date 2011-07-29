#include <stdint.h>
#include <string>

#ifndef __TORRENTINFO_H__
#define __TORRENTINFO_H__

namespace Tortilla {
	class Torrent;
};

class TorrentInfo {
	friend class Client;
public:
	TorrentInfo(Tortilla::Torrent* t);

	Tortilla::Torrent* getTorrent() const { return torrent; }
	const uint8_t* getInfoHash() const;
	const std::string& getName() const;
	unsigned int getNumPieces() const;
	unsigned int getNumPiecesCompleted() const;
	unsigned int getNumPiecesHashing() const;
	const uint64_t getTotalSize() const;
	const uint64_t getNumBytesLeft() const;
	const uint64_t getBytesUploaded() const;
	const uint64_t getBytesDownloaded() const;
	unsigned int getNumPeers() const;
	unsigned int getNumPendingPeers() const;

private:
	//! \brief Torrent we belong to
	Tortilla::Torrent* torrent;

	//! \brief Number of pieces completed
	unsigned int num_pieces_completed;

	//! \brief Number of pieces hashing 
	unsigned int num_pieces_hashing;

	//! \brief Number of peers
	unsigned int num_peers;

	//! \brief Number of pending peers
	unsigned int num_pending_peers;
};

#endif /* __TORRENTINFO_H__ */
