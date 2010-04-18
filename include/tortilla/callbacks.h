#include <string>

#ifndef __TORTILLA_CALLBACKS_H__
#define __TORTILLA_CALLBACKS_H__

class Torrent;
class Peer;

/*! \brief Implements callback functions for specific torrent events
 *
 *  Ideally, these functions will be called from a seperate thread
 *  so they can block if necessary, but this is not possible. Thus,
 *  these functions must avoid blocking as this will stall torrent
 *  progress.
 *
 *  Note that these functions may be called from any context.
 */
class Callbacks {
public:
	/*! \brief Called if a torrent got a tracker update
	 *  \param t Torrent being reported
	 *  \param newPeers Number of new peers, or -1 on error
	 *  \param message Message reported by the tracker, if any
	 */
	virtual void gotTrackerReply(Torrent* t, int newPeers, std::string message) { };

	/*! \brief Called if a piece was correctly transferred
	 *  \param t Torrent being reported
	 *  \param piece Piece number being reportedz
	 *
	 *  This will only be called if the piece hash checks out.
	 */
	virtual void completedPiece(Torrent* t, int piece) { };

	/*! \brief Called if a torrent was completely transferred
	 *  \param t Torrent being reported
	 */
	virtual void completedTorrent(Torrent* t) { };

	/*! \brief Called just before a torrent is completely removed
	 *  \param t Torrent being removed
	 *
	 *  This function will be called right before the torrent itself
	 *  is removed from the list, and allows for status storage. The
	 *  attempt cannot be cancelled.
	 */
	virtual void removingTorrent(Torrent* t) { };

	/*! \brief Called after a peer was added to the torrent
	 *  \param t Torrent being reported
	 *  \param p Peer being added
	 */
	virtual void addedPeer(Torrent* t, Peer* p) { }

	/*! \brief Called after a peer was removed from the torrent
	 *  \param t Torrent being reported
	 *  \param p Peer being removed
	 */
	virtual void removingPeer(Torrent* t, Peer* p) { };
};

#endif /* __TORTILLA_CALLBACKS_H__ */
