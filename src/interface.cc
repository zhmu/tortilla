#include <ncurses.h>
#include "interface.h"
#include "overview.h"
#include "info.h"

using namespace std;

Interface::Interface(Overseer* o)
{
	overseer = o;

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
		case KEY_DC: /* delete */
			Torrent* t = overview->getSelectedTorrent();
			if (t != NULL)
				overseer->removeTorrent(t);
			break;
	}
}

/* vim:set ts=2 sw=2: */
