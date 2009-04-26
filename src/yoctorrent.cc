#include <iostream>
#include <fstream>
#include <sstream>
#include <err.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "tortilla/exceptions.h"
#include "tortilla/metadata.h"
#include "tortilla/http.h"
#include "tortilla/overseer.h"
#include "tortilla/tracer.h"
#include "tortilla/torrent.h"

using namespace std;

Overseer* overseer = NULL;
Tracer* tracer = NULL;

void
sigint(int s)
{
	overseer->terminate();
}

void
run()
{
	while (!overseer->isTerminating()) {
		vector<Torrent*> torrents = overseer->getTorrents();

		for (vector<Torrent*>::iterator it = torrents.begin();
				it != torrents.end(); it++) {
			Torrent* t = *it;
			uint32_t rx, tx;
			t->getRateCounters(&rx, &tx);
			unsigned long long up = t->getBytesUploaded();
			unsigned long long down = t->getBytesDownloaded();

			printf("*** %s (%.2f%%) - RX/TX rate: %u / %u - Total U/D: %llu / %llu\n",
			 t->getName().c_str(),
			 ((float)(t->getTotalSize() - t->getBytesLeft()) / (float)t->getTotalSize()) * 100.0f,
				rx, tx, up, down);

			vector<PieceInfo> pieces = t->getPieceDetails();
			vector<PeerInfo> peers = t->getPeerDetails();
			for (unsigned int i = 0; i < peers.size(); i++) {
				PeerInfo& pi = peers[i];

				printf("  (%u) %c-%s, rx/tx: %u / %u,",
          (int)((pi.getNumPieces() / (float)pieces.size()) * 100.0f),
					pi.isIncoming() ? '>' : '<',
					pi.getEndpoint().c_str(),
					pi.getRxRate(), pi.getTxRate());
				if (pi.isSnubbed()) printf(" s");
				if (pi.isPeerInterested()) printf(" pi");
				if (pi.isPeerChoked()) printf(" pc");
				if (pi.areInterested()) printf(" i");
				if (pi.areChoking()) printf(" c");
				printf("\n");
			}
		}

		struct timeval tv;
		tv.tv_sec = 1; tv.tv_usec = 0;
		select (0, NULL, NULL, NULL, &tv);
	}
}

int
main(int argc, char** argv)
{
	srand(time(NULL));

	/* XXX */
	signal(SIGPIPE, SIG_IGN);

	if (argc != 2) {
		fprintf(stderr, "usage: yoctorrent file.torrent\n");
		return EXIT_FAILURE;
	}

	/* XXX handle it if the connection burns */
	//overseer = new Overseer(1024 + rand() % 10000);
	tracer = new Tracer();
	overseer = new Overseer(4000, tracer);
	//overseer->setUploadRate(16 * 1024);

	ifstream is;
	is.open(argv[1], ios::binary);
	Metadata* md = new Metadata(is);
	overseer->addTorrent(new Torrent(overseer, md));
	delete md;

	signal(SIGINT, sigint);
	printf(">> Waiting for torrents to hash...\n");
	overseer->waitHashingComplete();
	overseer->start();

	run();

	overseer->stop();

	delete overseer;
	delete tracer;
	HTTP::cleanup();
	return 0;
}

/* vim:set ts=2 sw=2: */
