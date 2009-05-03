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
#include "tortilla/sha1.h"
#include "tortilla/http.h"
#include "tortilla/overseer.h"
#include "tortilla/tracer.h"
#include "tortilla/torrent.h"
#include "interface.h"

using namespace std;

Interface* interface = NULL;
Overseer* overseer = NULL;
Tracer* tracer = NULL;

void
sigint(int s)
{
	overseer->terminate();
}

void
usage()
{
	fprintf(stderr, "usage: tortilla [-h?] [-p port] [-u upload] file.torrent ...\n\n");
	fprintf(stderr, "    -h, -?          this help\n");
	fprintf(stderr, "    -u upload       upload limit, in kb/sec\n");
	fprintf(stderr, "    -p port         incoming tcp port to use\n");
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

	if (argc < 1) {
		usage();
		/* NOTREACHED */
	}

	/*
	 * Try to parse metadata of all torrents; this ensures we only do something
	 * once all torrents are OK.
	 */
	vector<Metadata*> metadatas;
	for (int i = 0; i < argc; i++) {
		ifstream is;
		is.open(argv[i], ios::binary);
		metadatas.push_back(new Metadata(is));
	}

	/* XXX handle it if the connection burns */
	//overseer = new Overseer(1024 + rand() % 10000);
	tracer = new Tracer();
	overseer = new Overseer(port, tracer);
	interface = new Interface(overseer);
	overseer->setUploadRate(upload * 1024);

	/*
	 * Add the torrents one by one; we won't need the metadata
	 * anymore after this, so get rid of it.
	 */
	for (vector<Metadata*>::iterator it = metadatas.begin();
	     it != metadatas.end(); it++) {
		Metadata* md = *it;
		overseer->addTorrent(new Torrent(overseer, md));
		delete md;
	}

	signal(SIGINT, sigint);
	overseer->start();

	interface->run();
	overseer->stop();

	delete interface;
	delete overseer;
	delete tracer;
	return 0;
}

/* vim:set ts=2 sw=2: */
