#include <map>
#include <string>
#include "torrent.h"

#ifndef __OVERSEER_H__
#define __OVERSEER_H__

/*! \brief Responsible for overseeing all torrents
 */
class Overseer {
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

private:
	//! \brief Info hash to torrent mappings
	std::map<std::string, Torrent*> torrents;

	//! \brief are we terminating?
	bool terminating;
};

#endif /* __OVERSEER_H__ */
