#include <iostream>
#include <fstream>
#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include "metadata.h"

using namespace std;

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

	std::cout << md;

	return 0;
}
