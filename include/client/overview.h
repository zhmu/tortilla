#include <ncurses.h>
#include "interface.h"

#ifndef __OVERVIEW_H__
#define  __OVERVIEW_H__

class Overview {
public:
	Overview(WINDOW* w, Interface* iface);

	void draw();

	Torrent* getSelectedTorrent();

	void upTorrent();
	void downTorrent();

private:
	WINDOW* window;
	Interface* interface;

	unsigned int curSelection, firstTorrentIndex;
};

#endif /* __OVERVIEW_H__ */
