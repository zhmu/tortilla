#include <map>
#include <pthread.h>
#include <string>
#include <vector>
#include "file.h"
#include "metadata.h"

#ifndef __TORRENT_H__
#define __TORRENT_H__

#define TORRENT_HASH_LEN 20

//! \brief A chunk is an atomically transferable piece of data between peers
#define TORRENT_CHUNK_SIZE 16384

//! \brief Maximum number of requested pieces per client
#define TORRENT_PEER_MAX_REQUESTS 5

//! \brief Length of a peer ID
#define TORRENT_PEERID_LEN 20

//! \brief Desired number of peers per torrent
#define TORRENT_DESIRED_PEERS 30

//! \brief Delta in seconds between running (un)choking algorithm
#define TORRENT_DELTA_CHOKING_ALGO 10

//! \brief Size of a peer in compact form
#define TORRENT_COMPACTPEER_SIZE 6
    
class Connection;
class Peer;
class Overseer;

/*! \brief Implements a single, independant torrent
 *
 *  Since we have multiple threads at work here, variables are marked:
 *  [R]   for read only variables, that will never change.
 *  [M=x] variable protected by mutex x
 */
class Torrent {
friend void* torrent_thread(void* ptr);
friend class Peer;
friend class Overseer;
friend class Hasher;
public:
	/*! \brief Constructs a new torrent object
	 *  \param o Overseer to use
	 *  \param md Metadata to use
	 */
	Torrent(Overseer* o, Metadata* md);

	//! \brief Destructs the torrent object
	~Torrent();

	//! \brief Starts the torrent thread
	void start();

	//! \brief Stops the torrent thread
	void stop();

	//! \brief Is the torrent started?
	inline bool isStarted() {  return haveThread; }

	//! \brief Fetch the info hash
	const uint8_t* getInfoHash() { return infoHash; }

	//! \brief Retrieve the number of pieces
	unsigned int getNumPieces() { return numPieces; }

	//! \brief Retrieve the size of a single piece
	unsigned int getPieceLength() { return pieceLen; }

	/*! \brief Returns the first chunk we don't have of a piece
	 *  \param piece Piece to check
	 *  \returns The missing piece, or -1 if we have all
	 */
	int getMissingChunk(unsigned int piece);

	/*! \brief Set a chunk as requested or not */
	void setChunkRequested(unsigned int piece, unsigned int chunk, bool requested);

	//! \brief Do we have a piece?
	bool hasPiece(unsigned int piece);

	//! \brief How much data is in this torrent?
	const uint64_t getTotalSize() { return total_size; }

	void dump();

	/*! \brief Retrieve the hash of a piece
	 *  \param piece Piece number to use
	 */
	const uint8_t* getPieceHash(unsigned int piece);

	/*! \brief Returns the number of pieces for a given chunk */
	unsigned int calculateChunksInPiece(unsigned int piece);

	/*! \brief Retrieve the torrent's peer ID */
	const uint8_t* getPeerID();

	/*! \brief Add an peer to us
	 *  \param p Peer
	 */
	void addPeer(Peer* p);

	//! \brief Retrieve the number of pieces we are hashing
	unsigned int getNumPiecesHashing();

	//! \brief Retrieve how many bytes are left
	uint64_t getBytesLeft() { return left; }

	//! \brief Retrieve how many bytes have been uploaded
	uint64_t getBytesUploaded() { return uploaded; }

	//! \brief Retrieve how many bytes have been downloaded
	uint64_t getBytesDownloaded() { return downloaded; }

	/*! \brief Retrieves the receive/transmit rates
	 *  \param rx Receive rate, in bytes/second
	 *  \param tx Transmit rate, in bytes/second
	 */
	void getRateCounters(uint32_t* rx, uint32_t* tx);

protected:
	/*! \brief Called by a peer if pieces are added to the map */
	void callbackPiecesAdded(Peer* p, std::vector<unsigned int>& pieces);

	/*! \brief Called by a peer if pieces are removed from the map */
	void callbackPiecesRemoved(Peer* p, std::vector<unsigned int>& pieces);

	/*! \brief Called by a peer if a chunk is completed */
	void callbackCompleteChunk(Peer* p, unsigned int piece, uint32_t offset, const uint8_t* data, uint32_t len);

	/*! \brief Called by a peer if a piece is completed */
	void callbackCompletePiece(Peer* p, unsigned int piece);

	/*! \brief Called by the hasher if piece hashing results are in */
	void callbackCompleteHashing(unsigned int piece, bool result);

	//! \brief Called if the torrent download is complete
	void callbackCompleteTorrent();

	//! \brief Called by a peer just before it is gone
	void callbackPeerGone(Peer* p);

	//! \brief Called by a peer if it changes interest
	void callbackPeerChangedInterest(Peer* p);

	/*! \brief Go, speedracer, go -- handles the torrent activites
	 *
 	 *  This generally resides in an own thread.
	 */
	void go();

	//! \brief Called periodically to update bandwidth use
	void updateBandwidth();

	/*! \brief Called approximately every 1 second
	 *
	 *  This should implement choking/unchoking of peers.
	 */
	void heartbeat();

	//! \brief Queue a peer upload request
	void queueUploadRequest(Peer* p, uint32_t piece, uint32_t begin, uint32_t len);

	//! \brief Dequeue a peer upload request
	void dequeueUploadRequest(Peer* p, uint32_t piece, uint32_t begin, uint32_t len);

	/*! \brief Retrieves a piece from the output files
	 *  \param piece Piece number to read
	 *  \param offset Byte offset within piece
	 *  \param buf Buffer containing data to read to
	 *  \param length Length of the chunk
	 */
	void readChunk(unsigned int piece, unsigned int offset, uint8_t* buf, size_t length);

	//! \brief Increment the uploaded byte counter
	void incrementUploadedBytes(uint64_t amount);

	/*! \brief Handle status update to peers
	 *
	 *  This function will inform peers that do not have any pieces we 
	 *  want that we lost interest in them. If we are a seeder, we
	 *  ditch anyone who is also a seeder to give leechers a better
	 *  chance of getting a seeder (we don't need the slot anyway)
	 */
	void processCurrentPeers();

private:
	/*! \brief Contact the tracker
	 *  \param event Event to report to the tracker
	 *  \returns Metadata returned by the tracker
	 *
	 *  The caller is responsible for delete-ing the metadata once
	 *  they are done with it.
	 */
	Metadata* contactTracker(std::string event);

	/*! \brief Writes a chunk to our output files
	 *  \param piece Piece number to write
	 *  \param offset Byte offset within piece
	 *  \param buf Buffer containing data to write
	 *  \param length Length of the chunk
	 */
	void writeChunk(unsigned int piece, unsigned int offset, const uint8_t* buf, size_t length);

	/*! \brief Handle periodic update to the tracker
	 *  \param event Event to report to the tracker, if any
	 */
	void handleTracker(std::string event = "");

	//! \brief Ask for new pieces from each peer
	void scheduleRequests();

	//! \brief Request hashing of a piece
	void scheduleHashing(unsigned int piece);

	/*! \brief Convert an integer to a string
	 *  \param i Integer to use
	 */
	std::string convertInteger(uint64_t i);

	/*! \brief Finds a peer to download a piece from
	 *  \param piece Piece to download
	 *  \return Peer that has the piece, or NULL if no peers have it
	 */
	Peer* findPeerForPiece(uint32_t piece);

	//! \brief Runs the optimistic unchoking algorithm
	void handleUnchokingAlgorithm();

	/*! \brief Picks a random peer
	 *  \param skiplist Peers not to consider
	 *  \return A peer, or NULL if nothing matches the criterium
	 *
	 *  A value of -1 is don't care, 0 is false and 1 is true.
	 */
	Peer* pickRandomPeer(int choked, int interested, std::vector<Peer*>& skiplist);

	//! \brief Amount of bytes uploaded / downloaded / left
	uint64_t /* [M=data] */ uploaded, downloaded, left;

	/*! \brief Hash of the 'info' dictionary in the metadata
	 *
	 *  This is only cached for efficiency reasons.
	 */
	uint8_t /* [R] */ infoHash[TORRENT_HASH_LEN];

	/*! \brief Number of pieces in the torrent */
	unsigned int /* [R] */ numPieces;

	/*! \brief Piece chunk length */
	unsigned int /* [R] */ pieceLen;

	/*! \brief Total size of the torrent, in bytes */
	uint64_t /* [R] */ total_size;

	//! \brief Announce URL of the tracker
	std::string /* [R] */ announceURL;

	/*! \brief Contains the hash values for each piece */
	uint8_t* /* [R] */ pieceHash;

	/*! \brief Which pieces do we have?
	 *
	 *  This refers to the BitTorrent definition of pieces, i.e.
	 *  this vector contains numPieces booleans.
	 */
	std::vector<bool> /* [M=data] */ havePiece;

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
	std::vector<bool> /* [M=data] */ haveChunk;

	//! \brief Which chunks are requested?
	std::vector<bool> /* [M=data] */ haveRequestedChunk;

	//! \brief Which pieces have we requested?
	std::vector<Peer*> /* [M=data] */ requestedPiece;

	//! \brief Which pieces are being hashed?
	std::vector<bool> /* [M=data] */ hashingPiece;

	/*! \brief Stores the cardinality of each piece
	 *
	 *  The cardinality of a piece of defined as the numer of peers that
	 *  have the piece 
	 */
	std::vector<unsigned int> /* [M=data] */ pieceCardinality;

	/*! \brief List of peers
	 *
	 *  A peers ID isn't always known, so just use a vector. We bound the
	 *  upper number of peers anyway.
	 */
	std::vector<Peer*> /* [M=peers] */ peers;

	//! \brief Stores the files in the torrent
	std::vector<File*> /* [R] */ files;

	//! \brief Overseer object
	Overseer* /* [R] */ overseer;

	//! \brief Torrent thread
	pthread_t thread;

	//! \brief Have we created a torrent thread?
	bool haveThread;

	//! \brief Are we terminating?
	bool terminating;

	//! \brief Mutex protecting the peers list
	pthread_mutex_t mtx_peers;

	//! \brief Mutex protecting the data
	pthread_mutex_t mtx_data;

	//! \brief Receive rate, in bytes
	uint32_t rx_rate;

	//! \brief Transmit rate, in bytes
	uint32_t tx_rate;

	//! \brief Is the torrent complete?
	bool complete;

	/*! \brief Last tracker contact interval
	 *
	 *  Failed communication is also considered contact.
	 */
	time_t lastTrackerContact;

	//! \brief Interval in which the tracker must be contacted
	uint32_t tracker_interval;

	//! \brief Minimum interval in which the tracker can be contacted
	uint32_t tracker_min_interval;

	//! \brief Last (un)choking algorithm interval
	time_t lastChokingAlgorithm;

	//! \brief Current unchoking round
	int unchokingRound;

	//! \brief Algorithm's 'optimistic unchoked peer'
	Peer* optimisticUnchokedPeer;

	//! \brief Key presented by the tracker
	std::string tracker_key;
};

#endif /* __TORRENT_H__ */
