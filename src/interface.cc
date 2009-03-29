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
}

Interface::~Interface()
{
	delwin(overviewWindow);
	delwin(infoWindow);
	endwin();
}

void
Interface::run()
{
	Overview o(overviewWindow, this);
	Info i(infoWindow, this);

	while (!overseer->isTerminating()) {
		o.draw();
		i.draw(o.getSelectedTorrent());
		refresh();

		/*
		 * Wait for 1 second.
		 */
		struct timeval tv;
		tv.tv_sec = 1; tv.tv_usec = 0;
		select (0, NULL, NULL, NULL, &tv);
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

/* vim:set ts=2 sw=2: */
