#include <pthread.h>
#include <queue>
#include "peer.h"
#include "senderrequest.h"

class Overseer;

#ifndef __TORTILLA_SENDER_H__
#define __TORTILLA_SENDER_H__

/*! \brief Handles sending data to peers
 *
 *  This resides in a seperate thread to monitor the available bandwidth and
 *  gracefully deal with write timeouts risking blocking the torrent itself.
 */
class Sender {
friend	void* sender_thread(void* ptr);
friend  class Overseer;
public:
	/*! \brief Constructs an uploader
	 *  \param o Overseer to use
	 */
	Sender(Overseer* o);

	//! \brief Destroys the uploader
	~Sender();

	/*! \brief Set the number of bytes we may upload this interval
	 *
	 *  Zero means anything goes.
	 */
	void setAmountTransferrable(uint32_t amount);

protected:
	//! \brief Handles processing of the queue
	void process();

	//! \brief Request the sender to process
	void signal();

private:
	//! \brief Mutex protecting our local data
	pthread_mutex_t mtx_data;

	//! \brief Condition variable used to kick the sender
	pthread_cond_t cv;

	//! \brief Thread used by the uploader
	pthread_t thread;

	//! \brief Are we terminating?
	bool terminating;

	/*! \brief Number of bytes allowed to be transfered
	 *
	 *  This is protected by mtx_data.
	 */
	uint32_t tx_left;

	//! \brief Overseer object we belong to
	Overseer* overseer;
};

#endif /* __TORTILLA_SENDER_H__ */
