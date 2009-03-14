#include <pthread.h>
#include <queue>
#include "torrent.h"

#ifndef __HASHER_H__
#define __HASHER_H__

//! \brief Number of bytes used to calculate hash
#define HASHER_CHUNK_SIZE 8192

//! \brief Implements a hashing thread, which checks the torrent contents hash
class Hasher {
friend	void* hasher_thread(void* ptr);
public:
	/*! \brief Construct a new hasher thread
	 *  \param t Torrent to process
	 */
	Hasher(Torrent* t);

	/*! \brief Destructs the hasher
	 *
	 *  This will remove the hashing thread as well.
	 */
	~Hasher();

	/*! \brief Add a piece to hash
	 *  \param num Piece to hash
	 *
	 *  Once the hashing is complete, 
	 */
	void addPiece(unsigned int num);

protected:
	//! \brief Launch the hashing thread
	void run();

private:
	//! \brief Torrent we are hasing
	Torrent* torrent;

	//! \brief Queue of items that need to be hashed
	std::queue<unsigned int> hashQueue;

	//! \brief Reference to our thread
	pthread_t thread;

	//! \brief Mutex protecting our queue
	pthread_mutex_t mtx;

	//! \brief Condition variable used to awaken the thread
	pthread_cond_t cv;

	//! \brief Are we terminating?
	bool terminating;
};

#endif /* __HASHER_H__ */
