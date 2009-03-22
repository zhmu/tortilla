#include <assert.h>
#include <string.h>
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
	while(!terminating) {
		/* Wait until some event arrives */
		pthread_mutex_lock(&mtx);
		pthread_cond_wait(&cv, &mtx);
		if (terminating)
			break;

		assert(!hashQueue.empty());
		while (!hashQueue.empty() && !terminating) {
			unsigned int piecenum = hashQueue.front();
			hashQueue.pop();
			pthread_mutex_unlock(&mtx);

			/*
			 * While hashing, let go of the mutex; we'd be holding
			 * it unnecessarily long, as we can happily hash
			 * without it...
			 */
			unsigned int todo;
			if (piecenum == torrent->getNumPieces() - 1) {
				todo = torrent->getTotalSize() % torrent->getPieceLength();
			} else {
				todo = torrent->getPieceLength();
			}

			HashSHA1 h;
			for (unsigned int n = 0; /* true */; n++) {
				uint8_t chunk[HASHER_CHUNK_SIZE];
				uint32_t chunk_len = todo < HASHER_CHUNK_SIZE ? todo : HASHER_CHUNK_SIZE;
				if (chunk_len == 0)
					break;
				torrent->readChunk(piecenum, n * HASHER_CHUNK_SIZE, chunk, chunk_len);
				h.process(chunk, chunk_len);
				todo -= chunk_len;
			}
			bool ok = memcmp(h.getHash(), torrent->getPieceHash(piecenum), TORRENT_HASH_LEN) == 0;
			torrent->callbackCompleteHashing(piecenum, ok);

			pthread_mutex_lock(&mtx);
		}
		pthread_mutex_unlock(&mtx);
	}

	pthread_mutex_unlock(&mtx);
}
