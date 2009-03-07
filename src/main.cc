#include <iostream>
#include <fstream>
#include <sstream>
#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include "metadata.h"
#include "sha1.h"
#include "http.h"
#include "torrent.h"

using namespace std;

std::string
constructPeerID()
{
	string result = "";
	for(int i = 0; i < 20; i++) {
		result += "1";
	}
	return result;
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

	Torrent t(&md);

	return 0;
}

/* vim:set ts=2 sw=2: */
