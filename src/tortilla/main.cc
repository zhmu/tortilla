#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "tortilla/exceptions.h"
#include "tortilla/overseer.h"
#include "tortilla/tracer.h"
#include "tortilla/torrent.h"
#include "interface.h"

using namespace std;

Interface* interface = NULL;
Tortilla::Overseer* overseer = NULL;
Tortilla::Tracer* tracer = NULL;

void
sigint(int s)
{
	overseer->terminate();
}

void
usage()
{
	fprintf(stderr, "usage: tortilla [-h?] [-p port] [-u upload] [file.torrent ...]\n\n");
	fprintf(stderr, "    -h, -?          this help\n");
	fprintf(stderr, "    -u upload       upload limit, in kb/sec\n");
	fprintf(stderr, "    -p port         incoming tcp port to use\n");
	exit(EXIT_FAILURE);
}


void
handle_resize(int)
{
	interface->handleResize();
	signal(SIGWINCH, handle_resize);
}

int
main(int argc, char** argv)
{
	unsigned int port = 4000;
	unsigned int upload = 0;
	srand(time(NULL));

	/* XXX */
	signal(SIGPIPE, SIG_IGN);
	signal(SIGWINCH, handle_resize);

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

	/* XXX handle it if the connection burns */
	//overseer = new Overseer(1024 + rand() % 10000);
	tracer = new Tortilla::Tracer();
	overseer = new Tortilla::Overseer(port, tracer);
	interface = new Interface(overseer);
	overseer->setUploadRate(upload * 1024);

	/*
	 * Add the torrents one by one.
	 */
	for (int i = 0; i < argc; i++) {
		try {
			interface->addTorrent(argv[i]);
		} catch (exception e) {
			/* XXX handle me */
		}
	}

	signal(SIGINT, sigint);

	interface->run();

	delete interface;
	delete overseer;
	delete tracer;
	return 0;
}

/* vim:set ts=2 sw=2: */
