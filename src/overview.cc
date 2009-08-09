#include <string.h>
#include "overview.h"

using namespace std;

Overview::Overview(WINDOW* w, Interface* iface)
{
	window = w; interface = iface;
	curSelection = 0; firstTorrentIndex = 0;
}

void
Overview::draw()
{
	vector<Torrent*> torrents = interface->getOverseer()->getTorrents();

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

		if (numHashing > 0) {
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
	if (curSelection >= interface->getOverseer()->getTorrents().size())
		return NULL;
	return interface->getOverseer()->getTorrents()[curSelection];
}

void
Overview::downTorrent()
{
	curSelection = (curSelection + 1) % interface->getOverseer()->getTorrents().size();
}

void
Overview::upTorrent()
{
	if (curSelection == 0)
		curSelection = interface->getOverseer()->getTorrents().size() - 1;
	else
		curSelection--;
}

/* vim:set ts=2 sw=2: */
