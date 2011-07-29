#include <algorithm>
#include <string.h>
#include "client.h"
#include "overview.h"
#include "tortilla/torrent.h"
#include "torrentinfo.h"

using namespace std;

Overview::Overview(Client* c)
{
	window = NULL; client = c;
	curSelection = 0; firstTorrentIndex = 0;
}

void
Overview::draw()
{
	TorrentInfoVector& torrents = client->getTorrents();
	//sort(torrents.begin(), torrents.end(), Tortilla::Torrent::compareTorrentNames);

	unsigned int y = 1;
	werase(window);

	/*
	 * First of all, ensure the current selected torrent fits on the screen; this
	 * works by updating firstTorrentIndex to ensure the torrent we are looking
	 * for is in the center, if it's not visible already.
	 */
	unsigned int rows, cols;
	getmaxyx(window, rows, cols);
	unsigned int torrentsPerScreen = rows / 4;
	if (curSelection >= firstTorrentIndex + torrentsPerScreen || curSelection < firstTorrentIndex)
		firstTorrentIndex = (curSelection > (torrentsPerScreen / 2)) ? 
			curSelection - (torrentsPerScreen / 2) : 0;

	TorrentInfoVector::iterator it = torrents.begin() + firstTorrentIndex;
	unsigned int i = firstTorrentIndex;
	while (it != torrents.end()) {
		TorrentInfo* ti = *it;
		uint32_t rx, tx;
		ti->getTorrent()->getRateCounters(&rx, &tx);

		unsigned int numHashing = ti->getNumPiecesHashing();

		mvwprintw(window, y,     4, "(%.02f%%) %s",
		 ((float)(ti->getTotalSize() - ti->getNumBytesLeft()) / (float)ti->getTotalSize()) * 100.0f,
		ti->getName().c_str());

		if (ti->getTorrent()->isTerminating()) {
			mvwprintw(window, y + 1, 4, "[terminating]");
		} else if (numHashing > 0) {
			mvwprintw(window, y + 1, 4, "Hashing, %.02f%% completed",
			 100.0f - ((float)numHashing / (float)ti->getNumPieces()) * 100.0f);
		} else {
			mvwprintw(window, y + 1, 4, "RX/TX rate: %s / %s",
				 Interface::formatNumber(rx).c_str(), Interface::formatNumber(tx).c_str());
			mvwprintw(window, y + 2, 4, "Total: %s up, %s down",
				 Interface::formatNumber(ti->getBytesUploaded()).c_str(),
				 Interface::formatNumber(ti->getBytesDownloaded()).c_str());
		}
		mvwprintw(window, y    , 2, "%c", (curSelection == i) ? '*' : ' ');
		
		y += 4; it++; i++;
	}

	wrefresh(window);
}

TorrentInfo*
Overview::getSelectedTorrent()
{
	TorrentInfoVector& torrents = client->getTorrents();
	//sort(torrents.begin(), torrents.end(), Tortilla::Torrent::compareTorrentNames);
	if (curSelection >= torrents.size())
		return NULL;
	return torrents[curSelection];
}

void
Overview::downTorrent()
{
	int num = client->getTorrents().size();
	if (num > 0)
		curSelection = (curSelection + 1) % num;
	else
		curSelection = 0;
}

void
Overview::upTorrent()
{
	int num = client->getTorrents().size();
	if (num == 0)
		return;
	if (curSelection == 0)
		curSelection = num - 1;
	else
		curSelection--;
}

void
Overview::selectTorrent(TorrentInfo* t)
{
	TorrentInfoVector& torrents = client->getTorrents();
	//sort(torrents.begin(), torrents.end(), Tortilla::Torrent::compareTorrentNames);

	unsigned int torrentIndex = 0;
	for (TorrentInfoVector::iterator it = torrents.begin();
	     it != torrents.end(); it++, torrentIndex++) {
		TorrentInfo* ti = *it;
		if (ti != t)
			continue;

		/* Ensure the selected torrent fits */
		unsigned int rows, cols;
		getmaxyx(window, rows, cols);
		unsigned int torrentsPerScreen = rows / 4;

		if (curSelection < torrentIndex || curSelection > (torrentIndex + torrentsPerScreen))
			firstTorrentIndex = torrentIndex - (torrentsPerScreen / 2);

		curSelection = torrentIndex;
	}

	/* If we got here, torrent was not found; do nothing */
}

/* vim:set ts=2 sw=2: */
