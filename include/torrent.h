#include <map>
#include <string>
#include <vector>
#include "metadata.h"

#ifndef __TORRENT_H__
#define __TORRENT_H__

#define TORRENT_HASH_LEN 20

//! \brief A chunk is an atomically transferable piece of data between peers
#define TORRENT_CHUNK_SIZE 16384

//! \brief Maximum number of requested pieces per client
#define TORRENT_PEER_MAX_REQUESTS 5

class Peer;

/*! \brief Implements a single, independant torrent
 *
 *
 */
class Torrent {
friend class Peer;
public:
	/*! \brief Constructs a new torrent object
	 *  \param md Metadata to use
	 */
	Torrent(Metadata* md);

	//! \brief Destructs the torrent object
	~Torrent();

	//! \brief Go, speedracer, go!
	void go();

	//! \brief Fetch the info hash
	std::string getInfoHash() { return infoHash; }

	//! \brief Retrieve the number of pieces
	unsigned int getNumPieces() { return numPieces; }

	/*! \brief Returns the first chunk we don't have of a piece
	 *  \param piece Piece to check
	 *  \returns The missing piece, or -1 if we have all
	 */
	int getMissingChunk(unsigned int piece);

	/*! \brief Set a chunk as requested or not */
	void setChunkRequested(unsigned int piece, unsigned int chunk, bool requested);

	//! \brief Do we have a piece?
	bool hasPiece(unsigned int piece);

	void dump();

protected:
	/*! \brief Called by a peer if pieces are added to the map */
	void callbackPiecesAdded(Peer* p, std::vector<unsigned int>& pieces);

	/*! \brief Called by a peer if pieces are removed from the map */
	void callbackPiecesRemoved(Peer* p, std::vector<unsigned int>& pieces);

	/*! \brief Called by a peer if a chunk is completed */
	void callbackCompleteChunk(Peer* p, unsigned int piece, unsigned int chunk, const uint8_t* data, uint32_t len);

	/*! \brief Called by a peer if a piece is completed */
	void callbackCompletePiece(Peer* p, unsigned int piece);

private:
	/*! \brief Contact the tracker
	 *  \param event Event to report to the tracker, if any
	 *  \returns Metadata returned by the tracker
	 *
	 *  The caller is responsible for delete-ing the metadata once
	 *  they are done with it.
	 */
	Metadata* contactTracker(std::string event = "");

	//! \brief Handle periodic update to the tracker
	void handleTracker();

	//! \brief Ask for new pieces from each peer
	void scheduleRequests();

	/*! \brief Convert an integer to a string
	 *  \param i Integer to use
	 */
	std::string convertInteger(uint64_t i);

	//! \brief Amount of bytes uploaded / downloaded / left
	uint32_t uploaded, downloaded, left;

	//! \brief TCP port for incoming connections
	uint16_t port;

	//! \brief Peer ID used
	std::string peerID;

	/*! \brief Hash of the 'info' dictionary in the metadata
	 *
	 *  This is only cached for efficiency reasons.
	 */
	std::string infoHash;

	/*! \brief Number of pieces in the torrent */
	unsigned int numPieces;

	/*! \brief Piece chunk length */
	unsigned int pieceLen;

	//! \brief Announce URL of the tracker
	std::string announceURL;

	/*! \brief Contains the hash values for each piece */
	std::vector<std::string> pieceHash;

	/*! \brief Which pieces do we have?
	 *
	 *  This refers to the BitTorrent definition of pieces, i.e.
	 *  this vector contains numPieces booleans.
	 */
	std::vector<bool> havePiece;

	/*! \brief Which chunks do we have?
	 *
	 *  We consider a chunk data that can atomically be moved between two
	 *  peers (atomically as in: give me this data, and we either get all
	 *  of it or we don't get it) Therefore, every BitTorrent piece
	 *  consists of a fixed number of chunks. Using this vector, we keep
	 *  track of the chunks we need in order to complete a piece.
	 *
	 *  Note that  this vector can be used to compute havePiece, which we
	 *  won't do for efficiency reasons.
	 */
	std::vector<bool> haveChunk;

	//! \brief Which chunks are requested?
	std::vector<bool> haveRequestedChunk;

	//! \brief Which pieces have we requested?
	std::vector<Peer*> requestedPiece;

	/*! \brief Stores the cardinality of each piece
	 *
	 *  The cardinality of a piece of defined as the numer of peers that
	 *  have the piece 
	 */
	std::vector<unsigned int> pieceCardinality;

	/*! \brief List of peers
	 *
	 *  A map is used as we consider the peer ID to be unique and identify
	 *  peers using it, which is an O(log n) operation. For other
	 *  operations, we need the entire list anyway which is O(n), so using
	 *  a map will work out for us.
	 */
	std::map<std::string, Peer*> peers;

	int fd;
};

#endif /* __TORRENT_H__ */
