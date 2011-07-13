#include <boost/thread/locks.hpp>
#include <algorithm>
#include <assert.h>
#include <string.h>
#include <algorithm>
#include "hasher.h"
#include "macros.h"
#include "overseer.h"
#include "tracer.h"
#include "sha1.h"

using namespace std;
using namespace boost;
using namespace Tortilla;

#define TRACER (overseer->getTracer())

namespace Tortilla {
	void* hasher_thread(void* ptr)
	{
		((Hasher*)ptr)->run();
		return NULL;
	}
}

Hasher::Hasher(Overseer* o)
	: thread(hasher_thread, this)
{
	terminating = false; overseer = o;
}

Hasher::~Hasher()
{
	/* Request termination, kick the thread and wait till it's gone */
	terminating = true;
	cv.notify_one();
	thread.join();
}

void
Hasher::addPiece(Torrent* t, unsigned int num)
{
	assert (t->getPieceLength() % HASHER_CHUNK_SIZE == 0);

	{
		unique_lock<mutex> lock(mtx_data);
		hashQueue.push_back(HasherItem(t, num));
	}

	/* Get back to work, you slacker! */
	cv.notify_one();
}

void
Hasher::run() {
	while(true) {
		/* If needed, wait until some event arrives */
		unique_lock<mutex> lock(mtx_data);
		if (!terminating && hashQueue.empty())
			cv.wait(lock);
		if (terminating)
			break;

		assert(!hashQueue.empty());
		while (!hashQueue.empty() && !terminating) {
			HasherItem hi = hashQueue.front();
			hashQueue.pop_front();
			/* Unlock the mutex; we don't want to block waiters on hashing */
			lock.unlock();
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
				uint32_t chunk_len = std::min(todo, (unsigned int)HASHER_CHUNK_SIZE);
				if (!torrent->readChunk(piecenum, n * HASHER_CHUNK_SIZE, chunk, chunk_len)) {
					TRACE(HASHER, "torrent=%p,piece=%u,offset=%u,length=%u: read error", torrent, piecenum, n * HASHER_CHUNK_SIZE, chunk_len);
				}
				h.process(chunk, chunk_len);
				todo -= chunk_len; n++;
			}
			bool ok = memcmp(h.getHash(), torrent->getPieceHash(piecenum), TORRENT_HASH_LEN) == 0;
			TRACE(HASHER, "hashing completed: torrent=%p,piece=%u,ok=%u", torrent, piecenum, ok ? 1 : 0);
			torrent->callbackCompleteHashing(piecenum, ok);

			/* Relock the mutex */
			lock.lock();
		}
	}
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
	unique_lock<mutex> lock(mtx_data);
	hashQueue.remove_if(torrent_match(t));
}

/* vim:set ts=2 sw=2: */
