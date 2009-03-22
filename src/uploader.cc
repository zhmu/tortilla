#include <assert.h>
#include "uploader.h"

using namespace std;

void*
upload_thread(void* ptr)
{
	((Uploader*)ptr)->process();
	return NULL;
}

Uploader::Uploader()
{
	terminating = false;

	pthread_mutex_init(&mtx_queue, NULL);
	pthread_cond_init(&cv_queue, NULL);

	/* Off we gooo! */
	pthread_create(&thread, NULL, upload_thread, this);
}

Uploader::~Uploader()
{
	/*
	 * Request termination and wait for the thread to die.
	*/
	terminating = true;
	pthread_cond_signal(&cv_queue);
	pthread_join(thread, NULL);

	pthread_cond_destroy(&cv_queue);
	pthread_mutex_destroy(&mtx_queue);
}

void
Uploader::enqueue(Peer* p, uint32_t piece, uint32_t begin, uint32_t len)
{
	assert(len <= 16384); /* XXX */

	UploadRequest* req = new UploadRequest(p, piece, begin, len);
	pthread_mutex_lock(&mtx_queue);
	requests.push_back(req);
	pthread_mutex_unlock(&mtx_queue);

	/* Awaken! */
	pthread_cond_signal(&cv_queue);
}

/* Helper for removeRequestsFromPeer */
class peer_match {
public:
	inline peer_match(Peer *p) { peer = p; };
	bool operator () (UploadRequest* ur) {
		return ur->getPeer() == peer;
	}

private:
	const Peer* peer;
};

void
Uploader::removeRequestsFromPeer(Peer* p)
{
	pthread_mutex_lock(&mtx_queue);
	requests.remove_if(peer_match(p));
	pthread_mutex_unlock(&mtx_queue);
}

void
Uploader::dequeue(Peer* p, uint32_t piece, uint32_t begin, uint32_t len)
{
	pthread_mutex_lock(&mtx_queue);
	/* XXX rewrite me to remove_if or simular */
	while (true) {
		list<UploadRequest*>::iterator it = requests.begin();
		while (it != requests.end()) {
			UploadRequest* ur = *it;
			if (ur->getPeer() == p && ur->getPiece() == piece &&
			    ur->getOffset() == begin && ur->getLength() == len) {
				delete *it;
				break;
			}
		}
		if (it == requests.end())
			break;
	}
	pthread_mutex_unlock(&mtx_queue);
}

void
Uploader::process()
{
	while(!terminating) {
		pthread_mutex_lock(&mtx_queue);
		pthread_cond_wait(&cv_queue, &mtx_queue);
		if (terminating)
			break;

		while (!terminating && !requests.empty()) {
			/* Grab a request from the queue */
			UploadRequest* request = requests.front();
			requests.pop_front();
			pthread_mutex_unlock(&mtx_queue);

			/* Ask the peer to process */
			request->getPeer()->processUploadRequest(request);

			delete request;
			pthread_mutex_lock(&mtx_queue);
		}
		pthread_mutex_unlock(&mtx_queue);
	}

	pthread_mutex_unlock(&mtx_queue);
}
