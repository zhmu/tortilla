#include <iostream>
#include <fstream>
#include <sstream>
#include <err.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
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

				printf("  (%3u%%) %c-%s, rx/tx: %u / %u,",
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

void
usage()
{
	fprintf(stderr, "usage: yoctorrent [h?] [-u upload] [-p port] file.torrent\n\n");
	fprintf(stderr, "  -h, -?           this help\n");
	fprintf(stderr, "  -u upload        upload rate, in KB/sec\n");
	fprintf(stderr, "  -p port          port to bind to\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char** argv)
{
	unsigned int port = 4000;
	unsigned int upload = 0;
	srand(time(NULL));

	/* XXX */
	signal(SIGPIPE, SIG_IGN);

	int ch;
	while ((ch = getopt(argc, argv, "?hu:p:")) != -1) {
		switch (ch) {
			case '?':
			case 'h':
			default:
				usage();
				/* NOTREACHED */
			case 'u':
				upload = atoi(optarg);
				if (upload <= 0)
					printf( ">> NOTE: upload ratio zero or unparsable, unlimited assumed!\n");
				break;
			case 'p':
				port = atoi(optarg);
				if (port <= 0) {
					fprintf(stderr, "-p must be followed by a positive number\n");
					return EXIT_FAILURE;
				}
				break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	/* XXX handle it if the connection burns */
	//overseer = new Overseer(1024 + rand() % 10000);
	tracer = new Tracer();
	overseer = new Overseer(port, tracer);
	overseer->setUploadRate(upload * 1024);

	ifstream is;
	is.open(argv[0], ios::binary);
	Metadata* md = new Metadata(is);
	overseer->addTorrent(new Torrent(overseer, md));
	delete md;

	signal(SIGINT, sigint);
	printf(">> Waiting for torrents to hash...\n");
	overseer->waitHashingComplete();
	overseer->start();

	run();

	printf(">> Cleaning up...\n");
	overseer->stop();

	delete overseer;
	delete tracer;
	HTTP::cleanup();
	return 0;
}

/* vim:set ts=2 sw=2: */
