#include <algorithm>
#include <string.h>
#include "overview.h"

using namespace std;

Overview::Overview(Interface* iface)
{
	window = NULL; interface = iface;
	curSelection = 0; firstTorrentIndex = 0;
}

void
Overview::draw()
{
	vector<Torrent*> torrents = interface->getOverseer()->getTorrents();
	sort(torrents.begin(), torrents.end(), Torrent::compareTorrentNames);

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

	for (unsigned int i = firstTorrentIndex; i < torrents.size(); i++) {
		Torrent* t = torrents[i];
		uint32_t rx, tx;
		t->getRateCounters(&rx, &tx);

		unsigned int numHashing = t->getNumPiecesHashing();

		mvwprintw(window, y,     4, "(%.02f%%) %s",
		 ((float)(t->getTotalSize() - t->getBytesLeft()) / (float)t->getTotalSize()) * 100.0f,
		t->getName().c_str());

		if (t->isTerminating()) {
			mvwprintw(window, y + 1, 4, "[terminating]");
		} else if (numHashing > 0) {
			mvwprintw(window, y + 1, 4, "Hashing, %.02f%% completed",
			 100.0f - ((float)numHashing / (float)t->getNumPieces()) * 100.0f);
		} else {
			mvwprintw(window, y + 1, 4, "RX/TX rate: %s / %s",
				 Interface::formatNumber(rx).c_str(), Interface::formatNumber(tx).c_str());
			mvwprintw(window, y + 2, 4, "Total: %s up, %s down",
				 Interface::formatNumber(t->getBytesUploaded()).c_str(),
				 Interface::formatNumber(t->getBytesDownloaded()).c_str());
		}
		mvwprintw(window, y    , 2, "%c", (curSelection == i) ? '*' : ' ');
		
		y += 4;
	}

	wrefresh(window);
}

Torrent*
Overview::getSelectedTorrent()
{
	vector<Torrent*> torrents = interface->getOverseer()->getTorrents();
	sort(torrents.begin(), torrents.end(), Torrent::compareTorrentNames);
	if (curSelection >= torrents.size())
		return NULL;
	return torrents[curSelection];
}

void
Overview::downTorrent()
{
	int num = interface->getOverseer()->getTorrents().size();
	if (num > 0)
		curSelection = (curSelection + 1) % num;
	else
		curSelection = 0;
}

void
Overview::upTorrent()
{
	if (curSelection == 0)
		curSelection = interface->getOverseer()->getTorrents().size() - 1;
	else
		curSelection--;
}

void
Overview::selectTorrent(Torrent* t)
{
	vector<Torrent*> torrents = interface->getOverseer()->getTorrents();
	sort(torrents.begin(), torrents.end(), Torrent::compareTorrentNames);

	unsigned int torrentIndex = 0;
	for (vector<Torrent*>::iterator it = torrents.begin();
	     it != torrents.end(); it++, torrentIndex++) {
		Torrent* torrent = *it;
		if (torrent != t)
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
