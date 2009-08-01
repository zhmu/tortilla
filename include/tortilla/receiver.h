#include <pthread.h>
#include <list>
#include <map>
#include "file.h"

#ifndef __TORTILLA_RECEIVER_H__
#define __TORTILLA_RECEIVER_H__

class Overseer;
class Peer;
class HTTPRequest;

/*! \brief Handles receiver of data to peers / torrents
 *
 *  This object will also keep track of all peers that are known to any
 *  torrent; this includes handling incoming data and passing it to the
 *  appropriate peer and closing of a connection.
 *
 *  The use of this object greatly simplifies locking,
 *  since because it keeps track of all peers, it can safely
 *  decide once it's safe to remove any.
 */
class Receiver {
friend	void* receiver_thread(void* ptr);
public:
	/*! \brief Constructs a new downloader
	 *  \param o Overseer object to use
	 */
	Receiver(Overseer* o);

	/*! \brief Destroys the downloader
	 *
	 *  This will not delete any peers managed; this is up to the torrent.
	 */
	~Receiver();

	//! \brief Adds a peer to handle downloading from
	void addPeer(Peer* p);

	//! \brief Removes a peer
	void removePeer(Peer* p);

	//! \brief Adds a request to monitor
	void addRequest(HTTPRequest* r);

	//! \brief Removes a request
	void removeRequest(HTTPRequest* r);

	//! \brief Handles incoming data and disconnecting peers
	void process();

	/*! \brief Retrieve a peer by file descriptor and lock it for transfer
	 *  \param fd File descriptor to find
	 *  \returns Peer object, or NULL
	 *
	 *  If this function returns, the peer will be locked for transfer and
	 *  will not be removed.
	 */
	Peer* findPeerByFDAndLock(int fd);

	//! \brief Request a map of file descriptor for sending
	void getSendablePeers(std::list<int>& m);

private:
	//! \brief Our overseer object
	Overseer* overseer;

	//! \brief Lock used to protect our data
	pthread_rwlock_t rwl_data;

	//! \brief Peers managed by us
	std::list<Peer*> /* [M=peers] */ peers;

	//! \brief Outstanding HTTP requests
	std::list<HTTPRequest*> requests;

	//! \brief Hash used to map file descriptors -> peers
	std::map<int, Peer*> fdMap;

	//! \brief Thread used by the peer manager
	pthread_t thread;

	//! \brief Are we terminating?
	bool terminating;
};

#endif /* __TORTILLA_FILEMANAGER_H__ */
