#include <iostream>
#include <fstream>
#include <sstream>
#include <err.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "tortilla/callbacks.h"
#include "tortilla/exceptions.h"
#include "tortilla/metadata.h"
#include "tortilla/overseer.h"
#include "tortilla/tracer.h"
#include "tortilla/torrent.h"

using namespace std;

class yoctoCallbacks : public Callbacks {
public:
	void gotTrackerReply(Torrent* t, int newPeers, std::string message);
	void completedPiece(Torrent* t, int piece);
	void completedTorrent(Torrent* t);
	void removingTorrent(Torrent* t);
	void addedPeer(Torrent* t, Peer* p);
	void removingPeer(Torrent* t, Peer* p);
};

Overseer* overseer = NULL;
Tracer* tracer = NULL;
Callbacks* callbacks = NULL;

void
yoctoCallbacks::gotTrackerReply(Torrent* t, int newPeers, std::string message)
{
	printf(">>>> Got a tracker %s reply, %i new peers, message '%s'\n",
	 newPeers >= 0 ? "success" : "FAILURE",
	 newPeers, message.c_str());
}

void
yoctoCallbacks::completedPiece(Torrent* t, int piece)
{
	printf(">>>> Completed piece %u\n", piece);
}

void
yoctoCallbacks::completedTorrent(Torrent* t)
{
	printf(">>>> Completed torrent\n");
}

void
yoctoCallbacks::removingTorrent(Torrent* t)
{
	printf(">>>> Torrent removed from torrent list\n");
}

void
yoctoCallbacks::addedPeer(Torrent* t, Peer* p)
{
	printf(">>>> Peer %s added to torrent\n", p->getID().c_str());
}

void
yoctoCallbacks::removingPeer(Torrent* t, Peer* p)
{
	printf(">>>> Peer %s removed from torrent\n", p->getID().c_str());
}

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

			unsigned int numHashing = t->getNumPiecesHashing();
			if (numHashing > 0) {
				printf("*** %s (%.2f%%) - hashing: %.2f%% done\n",
			 	 t->getName().c_str(),
			 ((float)(t->getTotalSize() - t->getBytesLeft()) / (float)t->getTotalSize()) * 100.0f,
			 100.0f - ((float)numHashing / (float)t->getNumPieces()) * 100.0f);
				break;
			}
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
	//callbacks = new yoctoCallbacks();
	tracer = new Tracer();
	overseer = new Overseer(port, tracer, callbacks);
	overseer->setUploadRate(upload * 1024);

	ifstream is;
	is.open(argv[0], ios::binary);
	Metadata* md = new Metadata(is);
	overseer->addTorrent(new Torrent(overseer, md, ""));
	delete md;

	signal(SIGINT, sigint);

	run();

	printf(">> Cleaning up...\n");

	delete overseer;
	delete tracer;
	return 0;
}

/* vim:set ts=2 sw=2: */
