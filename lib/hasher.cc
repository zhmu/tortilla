#include <assert.h>
#include <string.h>
#include <algorithm>
#include "hasher.h"
#include "macros.h"
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

	INIT_MUTEX(data);
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
	DESTROY_MUTEX(data);
}

void
Hasher::addPiece(Torrent* t, unsigned int num)
{
	assert (t->getPieceLength() % HASHER_CHUNK_SIZE == 0);

	LOCK(data);
	hashQueue.push_back(HasherItem(t, num));
	UNLOCK(data);

	/* Get back to work, you slacker! */
	pthread_cond_signal(&cv);
}

void
Hasher::run() {
	while(true) {
		/* If needed, wait until some event arrives */
		LOCK(data);
		if (!terminating && hashQueue.empty())
			pthread_cond_wait(&cv, &mtx_data);
		if (terminating)
			break;

		assert(!hashQueue.empty());
		while (!hashQueue.empty() && !terminating) {
			HasherItem hi = hashQueue.front();
			hashQueue.pop_front();
			UNLOCK(data);
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
			unsigned int n = 0;
			while (todo > 0) {
				uint8_t chunk[HASHER_CHUNK_SIZE];
				uint32_t chunk_len = MIN(todo, HASHER_CHUNK_SIZE);
				if (!torrent->readChunk(piecenum, n * HASHER_CHUNK_SIZE, chunk, chunk_len)) {
					TRACE(HASHER, "torrent=%p,piece=%u,offset=%u,length=%u: read error", torrent, piecenum, n * HASHER_CHUNK_SIZE, chunk_len);
				}
				h.process(chunk, chunk_len);
				todo -= chunk_len; n++;
			}
			bool ok = memcmp(h.getHash(), torrent->getPieceHash(piecenum), TORRENT_HASH_LEN) == 0;
			TRACE(HASHER, "hashing completed: torrent=%p,piece=%u,ok=%u", torrent, piecenum, ok ? 1 : 0);
			torrent->callbackCompleteHashing(piecenum, ok);

			LOCK(data);
		}
		UNLOCK(data);
	}

	UNLOCK(data);
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
	LOCK(data);
	hashQueue.remove_if(torrent_match(t));
	UNLOCK(data);
}

/* vim:set ts=2 sw=2: */
