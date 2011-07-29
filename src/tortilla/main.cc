#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "client.h"

using namespace std;

static Client* client = NULL;

void
usage()
{
	fprintf(stderr, "usage: tortilla [-h?] [-p port] [-u upload] [file.torrent ...]\n\n");
	fprintf(stderr, "    -h, -?          this help\n");
	fprintf(stderr, "    -u upload       upload limit, in kb/sec\n");
	fprintf(stderr, "    -p port         incoming tcp port to use\n");
	exit(EXIT_FAILURE);
}

static void
handle_resize(int)
{
	client->handleResize();
	signal(SIGWINCH, handle_resize);
}

static void
handle_sigint(int s)
{
	client->terminate();
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

	client = new Client(port);
	client->setUploadRate(upload * 1024);

	/* XXX */
	signal(SIGWINCH, handle_resize);
	signal(SIGINT, handle_sigint);

	/*
	 * Add the torrents one by one.
	 */
	for (int i = 0; i < argc; i++) {
		try {
			client->addTorrent(argv[i]);
		} catch (exception e) {
			/* XXX handle me */
		}
	}

	client->run();
	delete client;

	return 0;
}

/* vim:set ts=2 sw=2: */
