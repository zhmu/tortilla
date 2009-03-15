#include <map>
#include <string>
#include "torrent.h"

#ifndef __OVERSEER_H__
#define __OVERSEER_H__

/*! \brief Responsible for overseeing all torrents
 */
class Overseer {
friend void* bandwidth_thread(void* ptr);
friend class Torrent;
public:
	//! \brief Constructs a new overseer
	Overseer();

	//! \brief Destroys the overseer and all torrents it manages
	~Overseer();

	//! \brief Hook a torrent to the overseer
	void addTorrent(Torrent* t);

	//! \brief Go, and oversee!
	void run();

	//! \brief Request termination
	void terminate();

protected:
	//! \brief Seperate thread handling bandwidth monitoring
	void bandwidthThread();

private:
	//! \brief Info hash to torrent mappings
	std::map<std::string, Torrent*> torrents;

	//! \brief are we terminating?
	bool terminating;

	//! \brief Bandwdith monitor thread
	pthread_t thread_bandwidth_monitor;

	//! \brief Mutex used to protect the torrents list
	pthread_mutex_t mtx_torrents;
};

#endif /* __OVERSEER_H__ */
