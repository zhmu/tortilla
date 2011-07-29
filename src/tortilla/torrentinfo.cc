#include "torrentinfo.h"
#include "tortilla/torrent.h"

TorrentInfo::TorrentInfo(Tortilla::Torrent* t)
	: torrent(t),
	  num_pieces_completed(0), num_pieces_hashing(0),
	  num_peers(0), num_pending_peers(0)
{
}

/* Simple torrent wrappers - these need no locks */
const uint8_t* TorrentInfo::getInfoHash() const { return torrent->getInfoHash(); }
const std::string& TorrentInfo::getName() const { return torrent->getName(); }
unsigned int TorrentInfo::getNumPieces() const { return torrent->getNumPieces(); }
const uint64_t TorrentInfo::getTotalSize() const { return torrent->getTotalSize(); }
const uint64_t TorrentInfo::getNumBytesLeft() const { return torrent->getBytesLeft(); }
const uint64_t TorrentInfo::getBytesUploaded() const { return torrent->getBytesUploaded(); }
const uint64_t TorrentInfo::getBytesDownloaded() const { return torrent->getBytesDownloaded(); }

/* Things we cache in our own object */
unsigned int TorrentInfo::getNumPiecesCompleted() const { return num_pieces_completed; }
unsigned int TorrentInfo::getNumPiecesHashing() const { return num_pieces_hashing; }
unsigned int TorrentInfo::getNumPeers() const { return num_peers; } 
unsigned int TorrentInfo::getNumPendingPeers() const { return num_pending_peers; }

/* vim:set ts=2 sw=2: */
