#include <string.h>
#include "overview.h"

using namespace std;

Overview::Overview(WINDOW* w, Interface* iface)
{
	window = w; interface = iface;
	curSelection = 0;
}

void
Overview::draw()
{
	vector<Torrent*> torrents = interface->getOverseer()->getTorrents();

	unsigned int y = 1;
	unsigned int num = 0;
	werase(window);
	for (vector<Torrent*>::iterator it = torrents.begin();
	    it != torrents.end(); it++) {
		Torrent* t = *it;
		uint32_t rx, tx;
		t->getRateCounters(&rx, &tx);

		mvwprintw(window, y,     4, "(%.02f%%) %s",
		 ((float)(t->getTotalSize() - t->getBytesLeft()) / (float)t->getTotalSize()) * 100.0f,
		 t->getName().c_str());
		mvwprintw(window, y + 1, 4, "RX/TX rate: %s / %s",
			 Interface::formatNumber(rx).c_str(), Interface::formatNumber(tx).c_str());
		mvwprintw(window, y + 2, 4, "Total: %s up, %s down",
			 Interface::formatNumber(t->getBytesUploaded()).c_str(),
			 Interface::formatNumber(t->getBytesDownloaded()).c_str());
		mvwprintw(window, y    , 2, "%c", (curSelection == num) ? '*' : ' ');
		
		y += 4; num++;
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

/* vim:set ts=2 sw=2: */
