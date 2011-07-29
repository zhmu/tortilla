#include <string>
#include <vector>
#include <stdint.h>
#include "tortilla/callbacks.h"

#ifndef __CLIENT_H__
#define __CLIENT_H__

namespace Tortilla {
	class Overseer;
	class Tracer;
};

class Callbacks;
class Interface;
class TorrentInfo;

typedef std::vector<TorrentInfo*> TorrentInfoVector;

class Client : public Tortilla::Callbacks {
public:
	Client(int port);
	~Client();
	void run();
	void addTorrent(std::string filename);
	void removeTorrent(TorrentInfo* ti);
	void setUploadRate(int upload);
	int getUploadRate() const;

	void terminate();
	bool isTerminating() const;

	TorrentInfoVector&	getTorrents() { return torrents; }

	void handleResize();
	void handleSignalInt();

protected:

	void gotTrackerReply(Tortilla::Torrent* t, int newPeers, std::string message);
 	void completedPiece(Tortilla::Torrent* t, int piece);
	void completedTorrent(Tortilla::Torrent* t);
	void addedTorrent(Tortilla::Torrent* t);
	void removingTorrent(Tortilla::Torrent* t);
	void addedPeer(Tortilla::Torrent* t, Tortilla::Peer* p);
	void removingPeer(Tortilla::Torrent* t, Tortilla::Peer* p);

private:
	//! \brief Client interface
	Interface*		interface;

	//! \brief Lord overseer of the church of torrent
	Tortilla::Overseer*	overseer;

	//! \brief Tracer used to record logs
	Tortilla::Tracer*	tracer;

	//! \brief Torrents managed by the client
	TorrentInfoVector	torrents;
};

#endif /* __CLIENT_H__ */
