#include <ncurses.h>
#include "interface.h"

#ifndef __OVERVIEW_H__
#define  __OVERVIEW_H__

class Overview {
public:
	Overview(Interface* iface);
	void setWindow(WINDOW* w) { window = w; }

	void draw();

	Torrent* getSelectedTorrent();

	void upTorrent();
	void downTorrent();
	void selectTorrent(Torrent* t);

private:
	WINDOW* window;
	Interface* interface;

	unsigned int curSelection, firstTorrentIndex;
};

#endif /* __OVERVIEW_H__ */
