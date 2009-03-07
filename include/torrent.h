#include "metadata.h"

#ifndef __TORRENT_H__
#define __TORRENT_H__

//! \brief Implements a single, independant torrent
class Torrent {
public:
	/*! \brief Constructs a new torrent object
	 *  \param md Metadata to use
	 */
	Torrent(Metadata* md);

	//! \brief Destructs the torrent object
	~Torrent();

protected:
	/*! \brief Contact the tracker
	 *  \param event Event to report to the tracker, if any
	 *  \returns Metadata returned by the tracker
	 *
	 *  The caller is responsible for delete-ing the metadata once
	 *  they are done with it.
	 */
	Metadata* contactTracker(std::string event = "");

	/*! \brief Convert an integer to a string
	 *  \param i Integer to use
	 */
	std::string convertInteger(uint64_t i);

private:
	//! \brief Metadata belonging to this torrent
	Metadata* metadata;

	//! \brief Amount of bytes uploaded / downloaded / left
	uint32_t uploaded, downloaded, left;

	//! \brief TCP port for incoming connections
	uint16_t port;

	//! \brief Peer ID used
	std::string peerID;

	/*! \brief Hash of the 'info' dictionary in the metadata
	 *
	 *  This is only cached for efficiency reasons.
	 */
	std::string infoHash;
};

#endif /* __TORRENT_H__ */
