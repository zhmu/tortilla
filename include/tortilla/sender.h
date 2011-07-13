#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include <queue>
#include <stdint.h>
#include "peer.h"
#include "senderrequest.h"

#ifndef __TORTILLA_SENDER_H__
#define __TORTILLA_SENDER_H__

namespace Tortilla {

class Overseer;

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
	boost::mutex mtx_data;

	//! \brief Condition variable used to kick the sender
	boost::condition_variable cv;

	//! \brief Are we terminating?
	bool terminating;

	/*! \brief Number of bytes allowed to be transfered
	 *
	 *  This is protected by mtx_data.
	 */
	uint32_t tx_left;

	//! \brief Overseer object we belong to
	Overseer* overseer;

	//! \brief Thread used by the uploader
	boost::thread thread;
};

}

#endif /* __TORTILLA_SENDER_H__ */
