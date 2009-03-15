#include <iostream>
#include <fstream>
#include <sstream>
#include <err.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include "metadata.h"
#include "sha1.h"
#include "overseer.h"
#include "torrent.h"

using namespace std;

Overseer o;

std::string
constructPeerID()
{
	string result = "";
	for(int i = 0; i < 20; i++) {
		result += "1";
	}
	return result;
}

void
sigint(int s)
{
	o.terminate();
}

int
main(int argc, char** argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: tortilla file.torrent\n");
		return EXIT_FAILURE;
	}

	ifstream is;
	is.open(argv[1], ios::binary);

	Metadata md(is);
	o.addTorrent(new Torrent(&o, &md));

	signal(SIGINT, sigint);
	o.run();

	return 0;
}

/* vim:set ts=2 sw=2: */
