#include <ncurses.h>
#include <string>
#include "interface.h"

#ifndef __INFO_H__
#define __INFO_H__

#define PANEL_PIECES	0
#define PANEL_PEERS	1
#define PANEL_LOG	2
#define PANEL_FILES	3
#define PANEL_MAX	(PANEL_FILES+1)

class Info {
public:
	Info(Interface* iface);
	void setWindow(WINDOW* w) { window = w; }

	void draw(Tortilla::Torrent* t);

	//! \brief Scroll half a page up
	void scrollUp();

	//! \brief Scroll half a page down
	void scrollDown();

	inline unsigned int getCurrentPanel() { return panel; }
	inline void setCurrentPanel(int p) { panel = p; }

protected:
	//! \brief Formatted print inside the window
	void printxyf(unsigned int x, unsigned int y, const char* format, ...);

	//! \brief Number of lines written	
	unsigned int num_lines;

	void drawPieces(Tortilla::Torrent* t, unsigned int& y);
	void drawPeers(Tortilla::Torrent* t, unsigned int& y);
	void drawLog(Tortilla::Torrent* t, unsigned int& y);
	void drawFiles(Tortilla::Torrent* t, unsigned int& y);

private:
	WINDOW* window;
	Interface* interface;

	unsigned int y_offset;

	unsigned int panel;
};

#endif /* __INTERFACE_H__ */
