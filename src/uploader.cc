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
	pthread_mutex_lock(&mtx_queue);
	requests.push_back(UploadRequest(p, piece, begin, len));
	pthread_mutex_unlock(&mtx_queue);

	/* Awaken! */
	pthread_cond_signal(&cv_queue);
}

/* Helper for removeRequestsFromPeer */
class peer_match {
public:
	inline peer_match(Peer *p) { peer = p; };
	bool operator () (UploadRequest ur) {
		return ur.getPeer() == peer;
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

/* Helper for removeRequestsFromPeer */
class request_match {
public:
	inline request_match(UploadRequest u) : ur(u) { }
	bool operator () (UploadRequest u) {
		return (ur.getPeer() == u.getPeer() &&
		        ur.getPiece() == u.getPiece() &&
		        ur.getOffset() == u.getOffset() &&
			ur.getLength() == u.getLength());
	}

private:
	UploadRequest ur;
};

void
Uploader::dequeue(Peer* p, uint32_t piece, uint32_t begin, uint32_t len)
{
	pthread_mutex_lock(&mtx_queue);
	requests.remove_if(request_match(UploadRequest(p, piece, begin, len)));
	pthread_mutex_unlock(&mtx_queue);
}

void
Uploader::process()
{
	while(true) {
		pthread_mutex_lock(&mtx_queue);
		if (!terminating && requests.empty())
			pthread_cond_wait(&cv_queue, &mtx_queue);
		if (terminating)
			break;

		while (!terminating && !requests.empty()) {
			/* Grab a request from the queue */
			UploadRequest request = requests.front();
			requests.pop_front();
			pthread_mutex_unlock(&mtx_queue);

			/* Ask the peer to process */
			request.getPeer()->processUploadRequest(&request);

			pthread_mutex_lock(&mtx_queue);
		}
		pthread_mutex_unlock(&mtx_queue);
	}

	pthread_mutex_unlock(&mtx_queue);
}
