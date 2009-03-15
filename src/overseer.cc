#include <sys/select.h>
#include <unistd.h>
#include "overseer.h"

using namespace std;

void*
bandwidth_thread(void* ptr)
{
	((Overseer*)ptr)->bandwidthThread();
	return NULL;
}

Overseer::Overseer()
{
	terminating = false;
	
	pthread_mutex_init(&mtx_torrents, NULL);
	pthread_create(&thread_bandwidth_monitor, NULL, bandwidth_thread, this);
}

Overseer::~Overseer()
{
	terminating = true;

	while (true) {
		map<string, Torrent*>::iterator it = torrents.begin();
		if (it == torrents.end())
			break;
		delete it->second;
		torrents.erase(it);
	}

	pthread_join(thread_bandwidth_monitor, NULL);
	pthread_mutex_destroy(&mtx_torrents);
}

void
Overseer::addTorrent(Torrent* t)
{
	string info((const char*)t->getInfoHash(), TORRENT_HASH_LEN);
	pthread_mutex_lock(&mtx_torrents);
	torrents[info] = t;
	pthread_mutex_unlock(&mtx_torrents);
}

void
Overseer::run()
{
	/* Launch all torrents */
	pthread_mutex_lock(&mtx_torrents);
	for (map<string, Torrent*>::iterator it = torrents.begin();
	     it != torrents.end(); it++) {
		Torrent* t = it->second;
		it->second->start();
	}
	pthread_mutex_unlock(&mtx_torrents);

	while (!terminating) {
		/*
		 * Wait for 1 second.
		 */
		struct timeval tv;
		tv.tv_sec = 1; tv.tv_usec = 0;
		select (0, NULL, NULL, NULL, &tv);

		/* XXX some basic info for now... */
		pthread_mutex_lock(&mtx_torrents);
		for (map<string, Torrent*>::iterator it = torrents.begin();
				 it != torrents.end(); it++) {
			Torrent* t = it->second;
			uint32_t rx, tx;
			t->getRateCounters(&rx, &tx);
			printf("torrent: rx %u bytes/sec, tx %u bytes/sec\n",
			 rx, tx);
		}
		pthread_mutex_unlock(&mtx_torrents);
	}
}

void
Overseer::terminate()
{
	terminating = true;
}

void
Overseer::bandwidthThread()
{
	while (!terminating) {
		/* Wait for a second */
		struct timeval tv;
		tv.tv_sec = 1; tv.tv_usec = 0;
		select(0, NULL, NULL, NULL, &tv);

		/* Ask all torrents to update their bandwidth usage */
		pthread_mutex_lock(&mtx_torrents);
		for (map<string, Torrent*>::iterator it = torrents.begin();
				 it != torrents.end(); it++) {
			Torrent* t = it->second;
			t->updateBandwidth();
		}
		pthread_mutex_unlock(&mtx_torrents);
	}
}

/* vim:set ts=2 sw=2: */
