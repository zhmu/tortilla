#include <iostream>
#include <ncurses.h>
#include <fstream>
#include <sstream>
#include "tortilla/exceptions.h"
#include "tortilla/overseer.h"
#include "interface.h"
#include "overview.h"
#include "info.h"

using namespace std;

Interface::Interface(Overseer* o)
{
	overseer = o; adding = false;

	initscr(); start_color();
	raw(); cbreak(); keypad(stdscr, TRUE);
	noecho(); curs_set(0); refresh();

	overviewWindow = newwin(0, COLS / 2, 0, 0);
	infoWindow = newwin(0, COLS / 2, 0, COLS / 2);

	overview = new Overview(overviewWindow, this);
	info = new Info(infoWindow, this);
}

Interface::~Interface()
{
	delwin(overviewWindow);
	delwin(infoWindow);
	delete info;
	delete overview;
	endwin();
}

void
Interface::run()
{

	while (!overseer->isTerminating()) {
		overview->draw();
		info->draw(overview->getSelectedTorrent());
		update();
		wrefresh(overviewWindow);
		refresh();

		/*
		 * Wait for 1 second, or less if a keystroke is hit
		 */
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(STDIN_FILENO, &fds);
		struct timeval tv;
		tv.tv_sec = 1; tv.tv_usec = 0;
		if (select (STDIN_FILENO + 1, &fds, NULL, NULL, &tv) < 0)
			break;
		if (FD_ISSET(STDIN_FILENO, &fds))
			handleInput();
	}
}

string
Interface::formatNumber(uint64_t n) {
	char tmp[64 /* XXX */ ];

#define FORMAT_NUMBER(i, range,spec) \
	if ((i) >= (range)) { \
		snprintf(tmp, sizeof(tmp), "%.2f %s", \
		 ((float)(i) / (float)(range)), spec); \
		return string(tmp); \
	}

	FORMAT_NUMBER(n, 1024 * 1024 * 1024, "GB");
	FORMAT_NUMBER(n, 1024 * 1024,        "MB");
	FORMAT_NUMBER(n, 1024,               "KB");

#undef FORMAT_NUMBER

	snprintf(tmp, sizeof(tmp), "%llu bytes", (unsigned long long)n);
	return string(tmp);
}

string
Interface::formatHex(const uint8_t* hex, unsigned int len)
{
	string result = "";

	for (unsigned int i = 0; i < len; i++)
		for (int j = 1; j >= 0; j--)
			result += "0123456789abcdef"[(hex[i] >> j * 4) & 0xf];
	return result;
}

void
Interface::handleInput()
{
	int ch = getch();

	if (adding) {
		handleAddInput(ch);
		return;
	}

	switch(ch) {
		case 0x11: /* control-q */
			overseer->terminate();
			break;
		case KEY_NPAGE:
			info->scrollDown();
			break;
		case KEY_PPAGE:
			info->scrollUp();
			break;
		case KEY_DOWN:
			overview->downTorrent();
			break;
		case KEY_UP:
			overview->upTorrent();
			break;
		case KEY_IC: /* insert */
			addString = "";
			adding = true;
			break;
		case KEY_DC: /* delete */
			Torrent* t = overview->getSelectedTorrent();
			if (t != NULL)
				overseer->removeTorrent(t);
			break;
	}
}

void
Interface::handleAddInput(int ch)
{
	switch(ch) {
		case 0x0a: /* return */
			try {
				addTorrent(addString);
			} catch (exception e) {
				statusMessage = string("Failed to add torrent: ") + e.what();
			}
			adding = false;
			return;
		case KEY_IC: /* insert */
			adding = false;
			return;
		case KEY_BACKSPACE:
			if (addString.size() > 0)
				addString = addString.substr(0, addString.size() - 1);
			return;
	}

	if (!isprint(ch))
		return;
	addString += ch;
}

void
Interface::update()
{
	unsigned int y = getmaxy(overviewWindow);

	if (statusMessage != "") {
		mvwprintw(overviewWindow, y - 1, 0, "%s", statusMessage.c_str());
		y--;
	}
	if (!adding)
		return;
	mvwprintw(overviewWindow, y - 1, 0, "filename> %s_", addString.c_str());
	adding = true;
}

void
Interface::addTorrent(std::string fname)
{
	ifstream is;
	is.open(fname.c_str(), ios::binary);
	Metadata* md = new Metadata(is);
	overseer->addTorrent(new Torrent(overseer, md));
	delete md;
}

/* vim:set ts=2 sw=2: */
