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
class PeerManager;

/*! \brief Responsible for overseeing all torrents
 */
class Overseer {
friend void* bandwidth_thread(void* ptr);
friend void* listener_thread(void* ptr);
friend void* heartbeat_thread(void* ptr);
friend class Torrent;
friend class Sender;
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

	//! \brief Go, and oversee!
	void start();

	//! \brief Stop
	void stop();

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
	//! \brief Seperate thread handling bandwidth monitoring
	void bandwidthThread();

	//! \brief Seperate thread handling incoming connections
	void listenerThread();

	//! \brief Seperate thread handling torrent silicon heartbeat
	void heartbeatThread();

	//! \brief Handles a new incoming socket
	void handleIncomingConnection(Connection* c);

	//! \brief Request hashing of a piece
	void queueHashPiece(Torrent* t, uint32_t piece);

	/*! \brief Cleans up the torrent state so it can be deleted
	 *
	 *  This must be called in the destructor of Torrent; this call
	 *  ensures any references to the torrent will be removed.
	 */
	void cleanupTorrent(Torrent* t);

	//! \brief Request a map of file descriptor for sending
	void getSendablePeers(std::list<int>& m);

	//! \brief Used to signal a sender
	void signalSender();

	//! \brief Called if a torrent adds a peer
	void callbackPeerAdded(Peer* p);

	//! \brief Called if a torrent ditches a peer
	void callbackPeerRemoved(Peer* p);

	//! \brief Adds a file to the list of files
	void addFile(File* f);

	//! \brief Removes a file from the list of files
	void removeFile(File* f);

	//! \brief Write to a file
	void writeFile(File* f, off_t offset, const void* buf, size_t len);

	//! \brief Read from a file
	void readFile(File* f, off_t offset, void* buf, size_t len);

	Peer* findPeerByFDAndLock(int fd);

private:
	//! \brief Info hash to torrent mappings
	std::map<std::string, Torrent*> torrents;

	//! \brief are we terminating?
	bool terminating;

	//! \brief Bandwidth monitor thread
	pthread_t thread_bandwidth_monitor;

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

	//! \brief Hasher thread
	Hasher* hasher;

	//! \brief Incoming listener thread
	pthread_t thread_listener;

	//! \brief Heartbeat thread
	pthread_t thread_heartbeat;

	/*! \brief Upload rate, in bytes/second
	 *
	 *  Zero indicates unlimited.
	 */
	uint32_t upload_rate;

	//! \brief Tracer object used
	Tracer* tracer;

	//! \brief File manager used
	FileManager* filemanager;

	//! \brief Peer manager used
	PeerManager* peermanager;
};

#endif /* __TORTILLA_OVERSEER_H__ */
