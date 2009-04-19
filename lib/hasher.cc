#include <assert.h>
#include <string.h>
#include <algorithm>
#include "hasher.h"
#include "overseer.h"
#include "tracer.h"
#include "sha1.h"

using namespace std;

#define TRACER (overseer->getTracer())

void*
hasher_thread(void* ptr)
{
	((Hasher*)ptr)->run();
	return NULL;
}

Hasher::Hasher(Overseer* o)
{
	terminating = false; overseer = o;

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
Hasher::addPiece(Torrent* t, unsigned int num)
{
	assert (t->getPieceLength() % HASHER_CHUNK_SIZE == 0);

	pthread_mutex_lock(&mtx);
	hashQueue.push_back(HasherItem(t, num));
	pthread_mutex_unlock(&mtx);

	/* Get back to work, you slacker! */
	pthread_cond_signal(&cv);
}

void
Hasher::run() {
	while(true) {
		/* If needed, wait until some event arrives */
		pthread_mutex_lock(&mtx);
		if (!terminating && hashQueue.empty())
			pthread_cond_wait(&cv, &mtx);
		if (terminating)
			break;

		assert(!hashQueue.empty());
		while (!hashQueue.empty() && !terminating) {
			HasherItem hi = hashQueue.front();
			hashQueue.pop_front();
			pthread_mutex_unlock(&mtx);
			TRACE(HASHER, "hashing started: torrent=%p,piece=%u", hi.getTorrent(), hi.getPiece());

			/*
			 * While hashing, let go of the mutex; we'd be holding
			 * it unnecessarily long, as we can happily hash
			 * without it...
			 */
			Torrent* torrent = hi.getTorrent();
			unsigned int piecenum = hi.getPiece();
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
			TRACE(HASHER, "hashing completed: torrent=%p,piece=%u,ok=%u", torrent, piecenum, ok ? 1 : 0);
			torrent->callbackCompleteHashing(piecenum, ok);

			pthread_mutex_lock(&mtx);
		}
		pthread_mutex_unlock(&mtx);
	}

	pthread_mutex_unlock(&mtx);
}

/* Helper for removeRequestsFromPeer */
class torrent_match {
public:
	torrent_match(Torrent* t) { torrent = t; }
	bool operator () (HasherItem& hi) const {
		return hi.getTorrent() == torrent;
	}

private:
	Torrent* torrent;
};

void
Hasher::cancelTorrent(Torrent* t)
{
	pthread_mutex_lock(&mtx);
	hashQueue.remove_if(torrent_match(t));
	pthread_mutex_unlock(&mtx);
}

/* vim:set ts=2 sw=2: */
