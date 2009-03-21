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
#include "torrent.h"

using namespace std;

Overseer* overseer = NULL;

void
sigint(int s)
{
	overseer->terminate();
}

int
main(int argc, char** argv)
{
	srand(time(NULL));

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
	overseer->run();

	delete overseer;
	return 0;
}

/* vim:set ts=2 sw=2: */
