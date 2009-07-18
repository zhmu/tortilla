#include <pthread.h>
#include <list>
#include <map>
#include "file.h"

#ifndef __PEERMANAGER_H__
#define __PEERMANAGER_H__

class Overseer;
class Peer;

/*! \brief Responsible for handling the pool of torrent peers
 *
 *  This object keeps track of all peers that are known to any
 *  torrent; this includes handling incoming data and passing it
 *  to the appropriate peer and closing of a connection.
 *
 *  The use of this object greatly simplifies locking,
 *  since because it keeps track of all peers, it can safely
 *  decide one it's safe to remove any.
 */
class PeerManager {
friend	void* peermanager_thread(void* ptr);
public:
	/*! \brief Constructs a new peer manager
	 *  \param o Overseer object to use
	 */
	PeerManager(Overseer* o);

	/*! \brief Destroys the peer manager
	 *
	 *  This will not delete any peers managed; this is up to the torrent.
	 */
	~PeerManager();

	//! \brief Adds a peer to the manager
	void addPeer(Peer* p);

	//! \brief Removes a peer
	void removePeer(Peer* p);

	//! \brief Handles incoming data and disconnecting peers
	void process();

	/*! \brief Retrieve a peer by file descriptor and lock it for sending
	 *  \param fd File descriptor to find
	 *  \returns Peer object, or NULL
	 *
	 *  If this function returns, the peer will be locked for sending and
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

	//! \brief Hash used to map file descriptors -> peers
	std::map<int, Peer*> fdMap;

	//! \brief Thread used by the peer manager
	pthread_t thread;

	//! \brief Are we terminating?
	bool terminating;
};

#endif /* __FILEMANAGER_H__ */
