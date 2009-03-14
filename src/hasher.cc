#include <assert.h>
#include "hasher.h"
#include "sha1.h"

using namespace std;

void*
hasher_thread(void* ptr)
{
	((Hasher*)ptr)->run();
	return NULL;
}

Hasher::Hasher(Torrent* t)
{
	torrent = t; terminating = false;
	assert (torrent->getPieceLength() % HASHER_CHUNK_SIZE == 0);

	pthread_mutex_init(&mtx, NULL);
	pthread_cond_init(&cv, NULL);

	pthread_create(&thread, NULL, hasher_thread, this);
}

Hasher::~Hasher()
{
	/* Request termination, kick the thread and wait till it's gone */
	terminating = true;
	pthread_cond_signal(&cv);
	pthread_join(thread, NULL);

	pthread_cond_destroy(&cv);
	pthread_mutex_destroy(&mtx);
}

void
Hasher::addPiece(unsigned int num)
{
	pthread_mutex_lock(&mtx);
	hashQueue.push(num);
	pthread_mutex_unlock(&mtx);

	/* Get back to work, you slacker! */
	pthread_cond_signal(&cv);
}

void
Hasher::run() {
	while(true) {
		/* Wait until some event arrives */
		pthread_cond_wait(&cv, &mtx);
		if (terminating)
			break;

		assert(!hashQueue.empty());
		while (!hashQueue.empty()) {
			unsigned int piecenum = hashQueue.front();
			hashQueue.pop();
			pthread_mutex_unlock(&mtx);

			/*
			 * While hashing, let go of the mutex; we'd be holding
			 * it unnecessarily long, as we can happily hash
			 * without it...
			 */
			unsigned int todo = torrent->getPieceLength();

			HashSHA1 h;
			for (unsigned int n = 0;
			     n < torrent->getPieceLength() / HASHER_CHUNK_SIZE;
			     n++) {
				uint8_t chunk[HASHER_CHUNK_SIZE];
				torrent->readChunk(piecenum, n * HASHER_CHUNK_SIZE, chunk, HASHER_CHUNK_SIZE);
				h.process(chunk, HASHER_CHUNK_SIZE);
			}
			string ourhash = h.getHash();
			string wanthash = torrent->getPieceHash(piecenum);
			torrent->callbackCompleteHashing(piecenum, ourhash == wanthash);

			pthread_mutex_lock(&mtx);
		}
		pthread_mutex_unlock(&mtx);
	}

	pthread_mutex_unlock(&mtx);
}
