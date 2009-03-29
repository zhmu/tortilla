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

int
main(int argc, char** argv)
{
	srand(time(NULL));
	tracer = new Tracer();

	/* XXX */
	signal(SIGPIPE, SIG_IGN);

	if (argc != 2) {
		fprintf(stderr, "usage: tortilla file.torrent\n");
		return EXIT_FAILURE;
	}

	ifstream is;
	is.open(argv[1], ios::binary);
	Metadata md(is);

	/* XXX handle it if the connection burns */
	overseer = new Overseer(1024 + rand() % 10000);
	interface = new Interface(overseer);
	overseer->addTorrent(new Torrent(overseer, &md));

	signal(SIGINT, sigint);
	overseer->waitHashingComplete();
	overseer->start();

	interface->run();
	overseer->stop();

	delete interface;
	delete overseer;
	delete tracer;
	return 0;
}

/* vim:set ts=2 sw=2: */
