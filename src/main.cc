#include <iostream>
#include <fstream>
#include <sstream>
#include <err.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "metadata.h"
#include "sha1.h"
#include "overseer.h"
#include "tracer.h"
#include "torrent.h"

using namespace std;

Overseer* overseer = NULL;
Tracer* tracer = NULL;	

void
sigint(int s)
{
	overseer->terminate();
}

std::string
formatNumber(uint64_t n) {
	char tmp[64 /* XXX */ ];

#define FORMAT_NUMBER(i, range,spec) \
	if ((i) >= (range)) { \
		snprintf(tmp, sizeof(tmp), "%.2f %s", \
		 ((float)(i) / (float)(range)), spec); \
		return string(tmp); \
	}

	FORMAT_NUMBER(n, 1024 * 1024 * 1024, "GB");
	FORMAT_NUMBER(n, 1024 * 1024,        "MB");
	FORMAT_NUMBER(n, 1024,               "KB");

	snprintf(tmp, sizeof(tmp), "%llu bytes", (unsigned long long)n);
	return string(tmp);
}

std::string
formatHex(const uint8_t* hex, unsigned int len)
{
	string result = "";

	for (unsigned int i = 0; i < len; i++)
		for (int j = 1; j >= 0; j--)
			result += "0123456789abcdef"[(hex[i] >> j * 4) & 0xf];
	return result;
}

int
main(int argc, char** argv)
{
	srand(time(NULL));
	tracer = new Tracer();

	if (argc != 2) {
		fprintf(stderr, "usage: tortilla file.torrent\n");
		return EXIT_FAILURE;
	}

	ifstream is;
	is.open(argv[1], ios::binary);
	Metadata md(is);

	/* XXX handle it if the connection burns */
	overseer = new Overseer(1024 + rand() % 10000);

	overseer->addTorrent(new Torrent(overseer, &md));

	signal(SIGINT, sigint);
	overseer->waitHashingComplete();

	overseer->start();
	while (!overseer->isTerminating()) {
		/*
		 * Wait for 1 second.
		 */
		struct timeval tv;
		tv.tv_sec = 1; tv.tv_usec = 0;
		select (0, NULL, NULL, NULL, &tv);

		/* XXX some basic info for now... */
		list<Torrent*> torrents = overseer->getTorrents();
		for (list<Torrent*>::iterator it = torrents.begin();
				 it != torrents.end(); it++) {
			Torrent* t = *it;
			uint32_t rx, tx;
			t->getRateCounters(&rx, &tx);
			printf("torrent: rx %s/sec, tx %s/sec,  %.02f%% complete (%s up, %s down)\n",
			 formatNumber(rx).c_str(), formatNumber(tx).c_str(),
			 ((float)(t->getTotalSize() - t->getBytesLeft()) / (float)t->getTotalSize()) * 100.0f,
			 formatNumber(t->getBytesUploaded()).c_str(),
			 formatNumber(t->getBytesDownloaded()).c_str());
		}

	}
	overseer->stop();

	delete overseer;
	delete tracer;
	return 0;
}

/* vim:set ts=2 sw=2: */
