#include <ncurses.h>
#include <string>
#include "interface.h"

#ifndef __INFO_H__
#define __INFO_H__

class Info {
public:
	Info(WINDOW* w, Interface* iface);

	void draw(Torrent* t);

private:
	WINDOW* window;
	Interface* interface;
};

#endif /* __INTERFACE_H__ */
