#include <pthread.h>
#include <queue>
#include "torrent.h"

#ifndef __HASHER_H__
#define __HASHER_H__

//! \brief Number of bytes used to calculate hash
#define HASHER_CHUNK_SIZE 8192

class HasherItem {
public:
	inline HasherItem(Torrent* t, unsigned int p) {
		torrent = t; piecenum = p;
	}

	inline Torrent* getTorrent() { return torrent; }
	inline unsigned int getPiece() { return piecenum; }

private:
	Torrent* torrent;
	unsigned int piecenum;
};

class Overseer;

//! \brief Implements a hashing thread, which checks the torrent contents hash
class Hasher {
friend	void* hasher_thread(void* ptr);
public:
	/*! \brief Construct a new hasher thread
	 *  \param o Overseer we belong to
	 */
	Hasher(Overseer* o);

	/*! \brief Destructs the hasher
	 *
	 *  This will remove the hashing thread as well.
	 */
	~Hasher();

	/*! \brief Add a piece to hash
	 *  \param t Torrent to hash for
	 *  \param num Piece to hash
	 */
	void addPiece(Torrent* t, unsigned int num);

	//! \brief Cancels hashing of all pieces of a torrent
	void cancelTorrent(Torrent* t);

protected:
	//! \brief Launch the hashing thread
	void run();

private:
	/*! \brief Queue of items that need to be hashed
	 *
	 *  This is implemented as a list, as we need to remove items
	 *  from torrents that no longer exist.
	 */
	std::list<HasherItem> hashQueue;

	//! \brief Reference to our thread
	pthread_t thread;

	//! \brief Mutex protecting our queue
	pthread_mutex_t mtx;

	//! \brief Condition variable used to awaken the thread
	pthread_cond_t cv;

	//! \brief Are we terminating?
	bool terminating;

	//! \brief Overseer we are bound to
	Overseer* overseer;
};

#endif /* __HASHER_H__ */
