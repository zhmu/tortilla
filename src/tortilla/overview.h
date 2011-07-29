#include <list>
#include <ncurses.h>
#include "interface.h"

#ifndef __OVERVIEW_H__
#define  __OVERVIEW_H__

class Client;

class Overview {
public:
	Overview(Client* c);
	void setWindow(WINDOW* w) { window = w; }

	void draw();

	TorrentInfo* getSelectedTorrent();

	void upTorrent();
	void downTorrent();
	void selectTorrent(TorrentInfo* t);

private:
	WINDOW* window;
	Client* client;

	unsigned int curSelection, firstTorrentIndex;
};

#endif /* __OVERVIEW_H__ */
