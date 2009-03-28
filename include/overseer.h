#include <map>
#include <string>
#include "connection.h"
#include "hasher.h"
#include "torrent.h"
#include "uploader.h"

#ifndef __OVERSEER_H__
#define __OVERSEER_H__

/*! \brief Responsible for overseeing all torrents
 */
class Overseer {
friend void* bandwidth_thread(void* ptr);
friend void* listener_thread(void* ptr);
friend void* heartbeat_thread(void* ptr);
friend class Torrent;
public:
	/*! \brief Constructs a new overseer
	 *  \param portnr TCP port number to use for incoming connections
	 */
	Overseer(unsigned int portnr);

	//! \brief Destroys the overseer and all torrents it manages
	~Overseer();

	//! \brief Hook a torrent to the overseer
	void addTorrent(Torrent* t);

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

	//! \brief Waits until all torrents have completed hashing
	void waitHashingComplete();

	//! \brief Are we terminating?
	bool isTerminating() { return terminating; }

	//! \brief Retrieve a list of torrents
	std::list<Torrent*> getTorrents();

protected:
	//! \brief Seperate thread handling bandwidth monitoring
	void bandwidthThread();

	//! \brief Seperate thread handling incoming connections
	void listenerThread();

	//! \brief Seperate thread handling torrent silicon heartbeat
	void heartbeatThread();

	//! \brief Queue a peer upload request
	void queueUploadRequest(Peer* p, uint32_t piece, uint32_t begin, uint32_t len);

	//! \brief Cancels a peer upload request
	void dequeueUploadRequest(Peer* p, uint32_t piece, uint32_t begin, uint32_t len);

	//! \brief Dequeues all requests for a peer
	void dequeuePeer(Peer* p);

	//! \brief Handles a new incoming socket
	void handleIncomingConnection(Connection* c);

	//! \brief Request hashing of a piece
	void queueHashPiece(Torrent* t, uint32_t piece);

	//! \brief Cancel hashing for a torrent
	void cancelHashingTorrent(Torrent* t);

private:
	//! \brief Info hash to torrent mappings
	std::map<std::string, Torrent*> torrents;

	//! \brief are we terminating?
	bool terminating;

	//! \brief Bandwidth monitor thread
	pthread_t thread_bandwidth_monitor;

	//! \brief Mutex used to protect the torrents list
	pthread_mutex_t mtx_torrents;

	//! \brief Peer ID used to identify ourselves
	uint8_t peerid[TORRENT_PEERID_LEN];

	//! \brief Port used for incoming connections
	unsigned int port;

	//! \brief Incoming connections
	Connection* incoming;

	//! \brief Uploader used for all torrents
	Uploader* uploader;

	//! \brief Hasher thread
	Hasher* hasher;

	//! \brief Incoming listener thread
	pthread_t thread_listener;

	//! \brief Heartbeat thread
	pthread_t thread_heartbeat;
};

#endif /* __OVERSEER_H__ */
