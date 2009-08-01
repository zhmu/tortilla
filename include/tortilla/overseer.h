#include <map>
#include <string>
#include "connection.h"
#include "hasher.h"
#include "torrent.h"
#include "sender.h"

#ifndef __TORTILLA_OVERSEER_H__
#define __TORTILLA_OVERSEER_H__

class Tracer;
class FileManager;
class Receiver;

/*! \brief Responsible for overseeing all torrents
 */
class Overseer {
friend void* overseer_thread(void* ptr);
friend class Torrent;
friend class Sender;
friend class Receiver;
public:
	/*! \brief Constructs a new overseer
	 *  \param portnr TCP port number to use for incoming connections
	 *  \param tr Tracer object to use, or NULL
	 */
	Overseer(unsigned int portnr, Tracer* tr);

	//! \brief Destroys the overseer and all torrents it manages
	~Overseer();

	//! \brief Hook a torrent to the overseer
	void addTorrent(Torrent* t);

	//! \brief Remove a torrent from the overseer
	void removeTorrent(Torrent* t);

	//! \brief Request termination
	void terminate();

	//! \brief Obtain the peer ID
	const uint8_t* getPeerID() { return peerid; }

	//! \brief Obtain the listening port number
	const unsigned int getListeningPort() { return port; }

	//! \brief Are we terminating?
	bool isTerminating() { return terminating; }

	//! \brief Retrieve a list of torrents
	std::vector<Torrent*> getTorrents();

	/*! \brief Set the upload rate, in bytes/second
	 *
	 *  An upload rate of zero indicates no upload rate throtteling is
	 *  performed.
	 */
	inline void setUploadRate(uint32_t rate) { upload_rate = rate; }

	//! \brief Retrieve the upload rate, in bytes/second
	inline uint32_t getUploadRate() { return upload_rate; }

	//! \brief Retrieve our tracer object
	Tracer* getTracer() { return tracer; } 

protected:
	//! \brief Seperate thread handling torrent silicon heartbeat
	void overseerThread();

	//! \brief Handles a new incoming socket
	void handleIncomingConnection(Connection* c);

	//! \brief Retrieve the incoming socket
	Connection* getIncoming() { return incoming; }

	/*
 	 * All functions below here are designed to honor the
	 * principe of least knowledge; all they do is simply
	 * pass the call to the objects that implement them.
	 */

	/** Hasher **/

	//! \brief Request hashing of a piece
	void queueHashPiece(Torrent* t, uint32_t piece);

	//! \brief Cancels any hashing scheduled for a torrent
	void cancelHashing(Torrent* t);

	/** Sender **/

	//! \brief Used to signal a sender
	void signalSender();

	/** Receiver **/

	//! \brief Add a peer
	void addPeer(Peer* p);

	//! \brief Remove a peer
	void removePeer(Peer* p);

	//! \brief Add a request
	void addRequest(HTTPRequest* r);

	//! \brief Remove a request
	void removeRequest(HTTPRequest* r);

	//! \brief Find a peer by file descriptor and lock the peer for sending
	Peer* findPeerByFDAndLock(int fd);

	//! \brief Request a map of file descriptor for sending
	void getSendablePeers(std::list<int>& m);

	/** FileManager **/

	//! \brief Adds a file to the list of files
	void addFile(File* f);

	//! \brief Removes a file from the list of files
	void removeFile(File* f);

	//! \brief Write to a file
	void writeFile(File* f, off_t offset, const void* buf, size_t len);

	//! \brief Read from a file
	void readFile(File* f, off_t offset, void* buf, size_t len);

private:
	//! \brief Info hash to torrent mappings
	std::map<std::string, Torrent*> torrents;

	//! \brief are we terminating?
	bool terminating;

	//! \brief Mutex used to protect the torrents list
	pthread_mutex_t mtx_torrents;

	//! \brief Mutex used to protect the data fields
	pthread_mutex_t mtx_data;

	//! \brief Peer ID used to identify ourselves
	uint8_t peerid[TORRENT_PEERID_LEN];

	//! \brief Port used for incoming connections
	unsigned int port;

	//! \brief Incoming connections
	Connection* incoming;

	//! \brief Sender object used for all torrents
	Sender* sender;

	//! \brief Receiver object used for all torrents
	Receiver* receiver;

	//! \brief Hasher thread
	Hasher* hasher;

	//! \brief Overseer thread
	pthread_t thread;

	/*! \brief Upload rate, in bytes/second
	 *
	 *  Zero indicates unlimited.
	 */
	uint32_t upload_rate;

	//! \brief Tracer object used
	Tracer* tracer;

	//! \brief File manager used
	FileManager* filemanager;
};

#endif /* __TORTILLA_OVERSEER_H__ */
