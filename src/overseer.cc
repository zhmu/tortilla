#include <unistd.h>
#include "overseer.h"

using namespace std;

Overseer::Overseer()
{
	terminating = false;
}

Overseer::~Overseer()
{
	while (true) {
		map<string, Torrent*>::iterator it = torrents.begin();
		if (it == torrents.end())
			break;
		delete it->second;
		torrents.erase(it);
	}
}

void
Overseer::addTorrent(Torrent* t)
{
	string info((const char*)t->getInfoHash(), TORRENT_HASH_LEN);
	torrents[info] = t;
}

void
Overseer::run()
{
	/* Launch all torrents */
	for (map<string, Torrent*>::iterator it = torrents.begin();
	     it != torrents.end(); it++) {
		Torrent* t = it->second;
		it->second->start();
	}

	while (!terminating) {
		sleep(1);
	}
}

void
Overseer::terminate()
{
	terminating = true;
}

/* vim:set ts=2 sw=2: */
