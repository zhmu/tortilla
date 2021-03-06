#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <map>
#include <string>
#include <vector>
#include "file.h"
#include "info.h"
#include "peer.h"
#include "metadata.h"

#ifndef __TORTILLA_TORRENT_H__
#define __TORTILLA_TORRENT_H__

namespace Tortilla {

#define TORRENT_HASH_LEN 20

//! \brief A chunk is an atomically transferable piece of data between peers
#define TORRENT_CHUNK_SIZE 16384

/*! \brief Maximum number of peers in a torrent at any time
 *
 *  More peers than this will be rejected.
 */
#define TORRENT_MAX_PEERS (TORRENT_DESIRED_PEERS * 2)

//! \brief Length of a peer ID
#define TORRENT_PEERID_LEN 20

//! \brief Desired number of peers per torrent
#define TORRENT_DESIRED_PEERS 30

//! \brief Delta in seconds between running (un)choking algorithm
#define TORRENT_DELTA_CHOKING_ALGO 10

//! \brief Percentage completed when we enter endgame mode
#define TORRENT_ENDGAME_PERCENTAGE	95

//! \brief Maximum number of peers unchoked by us at any time per torrent
#define TORRENT_MAX_UNCHOKED_PEERS	4
    
class Connection;
class Peer;
class HTTPRequest;
class Overseer;
class PendingPeer;
class SenderRequest;
class TrackerTalker;
class Tracer;

/*! \brief Implements a single, independant torrent
 *
 *  Since we have multiple threads at work here, variables are marked:
 *  [R]   for read only variables, that will never change.
 *  [M=x] variable protected by mutex x
 */
class Torrent {
friend class Peer;
friend class Receiver;
friend class Overseer;
friend class Hasher;
friend class SenderRequest;
friend class TrackerTalker;
public:
	/*! \brief Constructs a new torrent object
	 *  \param o Overseer to use
	 *  \param md Metadata to use
	 *  \param path Path under which the files will be created
	 */
	Torrent(Overseer* o, Metadata* md, std::string path);

	//! \brief Destructs the torrent object
	~Torrent();

	//! \brief Fetch the info hash
	const uint8_t* getInfoHash() const { return infoHash; }

	//! \brief Retrieve the number of pieces
	unsigned int getNumPieces() const { return numPieces; }

	//! \brief Retrieve the size of a single piece
	unsigned int getPieceLength() const { return pieceLen; }

	/*! \brief Returns the first chunk we don't have of a piece
	 *  \param p Peer to check
	 *  \param piece Piece to check
	 *  \returns The missing piece, or -1 if we have all
	 */
	int getMissingChunk(Peer* p, unsigned int piece);

	//! \brief Do we have a piece?
	bool hasPiece(unsigned int piece) const;

	//! \brief How much data is in this torrent?
	const uint64_t getTotalSize() const { return total_size; }

	/*! \brief Retrieve the hash of a piece
	 *  \param piece Piece number to use
	 */
	const uint8_t* getPieceHash(unsigned int piece) const;

	/*! \brief Returns the number of pieces for a given chunk */
	unsigned int calculateChunksInPiece(unsigned int piece) const;

	/*! \brief Retrieve the torrent's peer ID */
	const uint8_t* getPeerID() const;

	/*! \brief Add an peer to us
	 *  \param p Peer
	 */
	void registerPeer(Peer* p);

	//! \brief Retrieve the number of pieces we are hashing
	unsigned int getNumPiecesHashing() const;

	//! \brief Retrieve the number of pieces we have complete
	unsigned int getNumPiecesComplete() const;

	//! \brief Retrieve how many bytes are left
	uint64_t getBytesLeft() const { return left; }

	//! \brief Retrieve how many bytes have been uploaded
	uint64_t getBytesUploaded() const { return uploaded; }

	//! \brief Retrieve how many bytes have been downloaded
	uint64_t getBytesDownloaded() const { return downloaded; }

	/*! \brief Retrieves the receive/transmit rates
	 *  \param rx Receive rate, in bytes/second
	 *  \param tx Transmit rate, in bytes/second
	 */
	void getRateCounters(uint32_t* rx, uint32_t* tx);

	//! \brief Retrieve the torrent name
	const std::string& getName() const { return name; }

	/*! \brief Fetch the number of peers */
	unsigned int getNumPeers() const;

	//! \brief Retrieve the number of pending peers
	unsigned int getNumPendingPeers() const;

	//! \brief Retrieve the tracer object to use
	Tracer* getTracer() const;

	/*! \brief Retrieve details on all pieces */
	std::vector<PieceInfo> getPieceDetails() const;

	/*! \brief Retrieve details on all peers */
	std::vector<PeerInfo> getPeerDetails() const;

	/*! \brief Retrieve details on all files */
	std::vector<FileInfo> getFileDetails() const;

	//! \brief Can we accept yet another peer?
	bool canAcceptPeer() const;

	//! \brief Retrieve the torrent's message log
	std::list<std::string> getMessageLog() const;

	//! \brief Clears the torrent's message log
	void clearMessageLog();

	//! \brief Are we currently in endgame mode?
	inline bool isEndgameMode() const { return endgame_mode; }

	//! \brief Are we terminating?
	inline bool isTerminating() const { return terminating; }

	//! \brief At which time were we terminating?
	inline time_t getTerminationTime() const { return terminateTime; }

	/*! \brief Can we be deleted?
	 *
	 *  This will be set if the torrent was requested to terminate and
	 *  it has heard from the tracker.
	 */
	inline bool canBeDeleted() const { return removeOK; }

	//! \brief Perform a debugging dump of the entire torrent status
	void debugDump(FILE* f) const;

	/*! \brief Requests a torrent to cleanup
	 *
	 *  This will initiate cleaning up; the overseer will actually remove
	 *  the torrent itself.
	 */
	void shutdown();

	//! \brief Ordering operators, can be used for sorting
	bool operator<(Torrent* rhs) { return getName() < rhs->getName(); }
	bool operator<(Torrent  rhs) { return getName() < rhs.getName(); }

	/*! \brief Returns the torrent metadata with current status
	 *  \returns Metadata structure consistant with torrent structure
	 */
	Metadata* storeStatus() const;

	/*! \brief Obtain a torrent info hash from metadata
	 *  \param md Metadata to use
	 *  \param hash Information hash storage
	 *  \return true on success
	 */
	static bool constructInfoHash(Metadata* md, uint8_t* hash);

	/*! \brief Comparison function for sorting by name
	 *  \param a First torrent object
	 *  \param b Second torrent object
	 *  \return true if a < b
	 */
	static bool compareTorrentNames(const Torrent* a, const Torrent* b);

	//! \brief User pointer
	void* user_ptr;

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

	//! \brief Called by a peer if it changes interest
	void callbackPeerChangedInterest(Peer* p);

	//! \brief Called by a peer if it becomes choked or unchoked
	void callbackPeerChangedChoking(Peer* p);

	//! \brief Called once the tracker reply is in
	void callbackTrackerReply(std::string result, bool error);

	//! \brief Called periodically to update bandwidth use
	void updateBandwidth();

	/*! \brief Called approximately every 1 second
	 *
	 *  This should implement choking/unchoking of peers.
	 */
	void heartbeat();

	/*! \brief Unregister the peer
	 *  \param p Peer to unregister
	 *
	 *  Once completed, any reference to the peer will be removed.
	 */
	void unregisterPeer(Peer* p);

	/*! \brief Retrieves a piece from the output files
	 *  \param piece Piece number to read
	 *  \param offset Byte offset within piece
	 *  \param buf Buffer containing data to read to
	 *  \param length Length of the chunk
	 *  \returns true on success
	 */
	bool readChunk(unsigned int piece, unsigned int offset, uint8_t* buf, size_t length);

	//! \brief Increment the uploaded byte counter
	void incrementUploadedBytes(uint64_t amount);

	/*! \brief Handle status update to peers
	 *
	 *  This function will update all connected peers that we have gained
	 *  or lost interest in them. It will also attempt to ditch anyone who
	 *  is also a seeder to give leechers a better chance of getting a
	 *  seeder (plus, there's nothing we gain by connecting to a seeder
	 *  when we have all data anyway)
	 */
	void processPeerStatus();

	/*! \brief Ask for new pieces from a peer
	 *  \param p Peer to use
	 *
	 *  This should be called when a peer gives us the go-ahead.
	 */
	void schedulePeerRequests(Peer* p);

	//! \brief Request the sender to awaken
	void signalSender() const;

	/*! \brief Log a message
	 *  \param p If set, use this peer
	 *  \param fmt Format specifier
	 */
	void log(Peer* p, const char* fmt, ...);

	//! \brief Adds a request to the overseer
	void addRequest(HTTPRequest* r);

	/*! \brief Parses a status metadata structure
	 *  \returns true on success
	 */
	bool restoreStatus(const MetaDictionary* status);

	/*! \brief Sets the new path of the torrent files
	 *  \param path New path to use
	 *  \returns true on success
	 *
	 *  This will move all files in place; either all files will be
	 *  moved, or none at all.
	 */
	bool setFilePath(std::string path);

private:
	/*! \brief Contact the tracker
	 *  \param event Event to report to the tracker
	 *
	 *  This function initiates tracker contact and
	 *  results in callbackTrackerReply() being called.
	 */
	void contactTracker(std::string event);

	/*! \brief Handle a chunk from or to our output files
	 *  \param piece Piece number to write
	 *  \param offset Byte offset within piece
	 *  \param buf Buffer containing data to write
	 *  \param length Length of the chunk
	 *  \param writing Write the chunk if true, otherwise read
	 *  \returns true on success
	 */
	bool handleChunk(unsigned int piece, unsigned int offset, uint8_t* buf, size_t length, bool writing);

	/*! \brief Writes a chunk to our output files
	 *  \param piece Piece number to write
	 *  \param offset Byte offset within piece
	 *  \param buf Buffer containing data to write
	 *  \param length Length of the chunk
	 *  \returns true on success
	 */
	bool writeChunk(unsigned int piece, unsigned int offset, const uint8_t* buf, size_t length);

	/*! \brief Handles the reply of the tracker */
	void handleTrackerReply(std::string reply);

	/*! \brief Request hashing of a piece
	 *  \param piece Piece number to hash
	 *  \param registerHashing If true, hashing must complete before torrent launches
	 */
	void scheduleHashing(unsigned int piece, bool registerHashing = false);

	/*! \brief Convert an integer to a string
	 *  \param i Integer to use
	 */
	static std::string convertInteger(uint64_t i);

	//! \brief Runs the optimistic unchoking algorithm
	void handleUnchokingAlgorithm();

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

	/*! \brief Which chunks are requested?
	 *
	 *  We keep track per chunk which peers we have requested the chunk of;
	 *  this is needed in endgame mode, where we don't want to flood a peer
	 *  with requests.
	 */
	std::vector<PeerList> /* [M=data] */ haveRequestedChunk;

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

	//! \brief Are we terminating?
	bool terminating;

	//! \brief At which time were we scheduling terminating?
	time_t terminateTime;

	//! \brief Is it OK to remove us?
	bool removeOK;

	//! \brief Mutex protecting the peers list
	mutable boost::shared_mutex rwl_peers;

	//! \brief Mutex protecting the files list
	mutable boost::shared_mutex rwl_files;

	//! \brief Mutex protecting the data
	mutable boost::mutex mtx_data;

	//! \brief Mutex protecting the log
	mutable boost::mutex mtx_log;

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

	//! \brief Algorithm's 'optimistic unchoked peer'
	Peer* optimisticUnchokedPeer;

	//! \brief Key presented by the tracker
	std::string tracker_key;

	//! \brief Torrent name
	std::string name;

	//! \brief Are we in endgame mode?
	bool endgame_mode;

	//! \brief List of pending peers we may try to use
	std::list<PendingPeer*> /* [M=data] */ pendingPeers;

	/*! \brief Number of pieces currently hashing
	 *
	 *  This is used at startup; the torrent won't request
	 *  any new pieces until it's done hashing.
	 */
	unsigned int numPiecesHashing;

	//! \brief Class used to communicate with the tracker
	TrackerTalker* trackerTalker;

	//! \brief Log of messages
	std::list<std::string> /* [M=log] */ messageLog;

	//! \brief Pending request, if any
	HTTPRequest* pendingRequest;

	//! \brief Metadata dictionary of the torrent
	MetaDictionary* torrentDictionary;
};

}

#endif /* __TORTILLA_TORRENT_H__ */
