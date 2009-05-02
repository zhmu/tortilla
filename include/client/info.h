#include <ncurses.h>
#include <string>
#include "interface.h"

#ifndef __INFO_H__
#define __INFO_H__

class Info {
public:
	Info(WINDOW* w, Interface* iface);

	void draw(Torrent* t);

	//! \brief Scroll half a page up
	void scrollUp();

	//! \brief Scroll half a page down
	void scrollDown();

protected:
	//! \brief Formatted print inside the window
	void printxyf(unsigned int x, unsigned int y, const char* format, ...);

	//! \brief Number of lines written	
	unsigned int num_lines;

private:
	WINDOW* window;
	Interface* interface;

	unsigned int y_offset;
};

#endif /* __INTERFACE_H__ */
