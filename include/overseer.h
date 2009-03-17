#include <map>
#include <string>
#include "connection.h"
#include "torrent.h"

#ifndef __OVERSEER_H__
#define __OVERSEER_H__

/*! \brief Responsible for overseeing all torrents
 */
class Overseer {
friend void* bandwidth_thread(void* ptr);
friend void* listener_thread(void* ptr);
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
	void run();

	//! \brief Request termination
	void terminate();

	//! \brief Obtain the peer ID
	const uint8_t* getPeerID() { return peerid; }

	//! \brief Obtain the listening port number
	const unsigned int getListeningPort() { return port; }

	//! \brief Waits until all torrents have completed hashing
	void waitHashingComplete();

protected:
	//! \brief Seperate thread handling bandwidth monitoring
	void bandwidthThread();

	//! \brief Seperate thread handling incoming connections
	void listenerThread();

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

	//! \brief Incoming listener thread
	pthread_t thread_listener;
};

#endif /* __OVERSEER_H__ */
