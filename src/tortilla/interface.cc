#include <sys/types.h>
#include <sys/ioctl.h>
#include <algorithm>
#include <dirent.h>
#include <iostream>
#include <ncurses.h>
#include <fstream>
#include <sstream>
#include <string.h>
#include "tortilla/exceptions.h"
#include "tortilla/info.h"
#include "tortilla/torrent.h"
#include "client.h"
#include "interface.h"
#include "info.h"
#include "overview.h"
#include "torrentinfo.h"

using namespace std;

Interface::Interface(Client* c)
{
	client = c; adding = false; searching = false; statusTime = 0;

	initscr(); start_color();
	raw(); cbreak(); keypad(stdscr, TRUE);
	noecho(); curs_set(0); refresh();

	overview = new Overview(c);
	info = new Info(this);

	overviewWindow = NULL; infoWindow = NULL; statusLine = NULL;
	updateWindows();

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

	while (!client->isTerminating()) {
		redraw();

		/*
		 * Wait for 1 second, or less if a keystroke is hit
		 */
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(STDIN_FILENO, &fds);
		struct timeval tv;
		tv.tv_sec = 1; tv.tv_usec = 0;
		if (select (STDIN_FILENO + 1, &fds, NULL, NULL, &tv) < 0)
			continue;
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
	if (searching) {
		handleSearchInput(ch);
		return;
	}

	switch(ch) {
		case 0x11: /* control-q */
			client->terminate();
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
			tabMatches.clear();
			break;
		case '/':
			searchString = "";
			searching = true;
			break;
		case '1':
		case '2':
		case '3':
		case '4':
			info->setCurrentPanel(ch - '1');
			break;
		case '=':
			alterUploadRate(1024);
			return;
		case '-':
			alterUploadRate(-1024);
			return;
		case '+':
			alterUploadRate(10 * 1024);
			return;
		case '_':
			alterUploadRate(10 * -1024);
			return;
		case 0x09: /* TAB */
			info->setCurrentPanel((info->getCurrentPanel() + 1) % PANEL_MAX);
			break;
		case KEY_DC: /* delete */
			if (overview->getSelectedTorrent() != NULL)
				client->removeTorrent(overview->getSelectedTorrent());
			break;
		case 'D':
			if (overview->getSelectedTorrent() != NULL) {
				FILE* f = fopen("dump.xml", "w");
				if (f != NULL) {
					overview->getSelectedTorrent()->getTorrent()->debugDump(f);
					fclose(f);
				}
			}
			break;
	}
}

void
Interface::handleAddInput(int ch)
{
	switch(ch) {
		case 0x0a: /* return */
			if (addString.size() != 0) {
				try {
					addTorrent(addString);
					setStatusMessage("Torrent successfully added");
				} catch (exception e) {
					setStatusMessage(string("Failed to add torrent: ") + e.what());
				}
			}
			adding = false;
			return;
		case KEY_IC: /* insert */
		case 0x1b: /* escape */
			adding = false;
			return;
		case KEY_BACKSPACE:
			if (addString.size() > 0)
				addString = addString.substr(0, addString.size() - 1);
			return;
		case 0x09: /* tab */
			handleCompletion();
			return;
		case 0x15: /* ctrl-u */
			addString = "";
			return;
		case 0x17: /* ctrl-w */
			string::size_type pos = addString.find_last_of(" ");
			if (pos != string::npos)
				addString = addString.substr(0, pos);
			else
				addString = "";
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

	/* Reset the status message if it has timed out */
	if (statusMessage != "" && statusTime + INTERFACE_STATUS_TIMEOUT < time(NULL))
		statusMessage = "";

	/* If there is no status message, show overview */
	if (statusMessage == "") {
		TorrentInfoVector torrents = client->getTorrents();
		uint32_t totalRx = 0, totalTx = 0;
		uint64_t totalUp = 0, totalDown = 0;
		for (TorrentInfoVector ::const_iterator it = torrents.begin();
		     it != torrents.end(); it++) {
			uint32_t rx, tx;
			TorrentInfo* t = *it;
			t->getTorrent()->getRateCounters(&rx, &tx);
			totalRx += rx; totalTx += tx;
			totalUp += t->getTorrent()->getBytesUploaded();
			totalDown += t->getTorrent()->getBytesDownloaded();
		}
		string status;
		status  = "RX/TX: rate " + Interface::formatNumber(totalRx) + " / " + Interface::formatNumber(totalTx);
		status += ", total " + Interface::formatNumber(totalDown) + " / " + Interface::formatNumber(totalUp);

		werase(statusLine);
		mvwprintw(statusLine, 0, 0, "%s", status.c_str());
		wrefresh(statusLine);
	}

	if (searching) {
		mvwprintw(overviewWindow, y - 1, 0, "search> %s_", searchString.c_str());
		return;
	}

	if (!adding)
		return;
	mvwprintw(overviewWindow, y - 1, 0, "filename> %s_", addString.c_str());
	y--;
	for (vector<string>::iterator it = tabMatches.begin();
	     it != tabMatches.end(); it++) {
		mvwprintw(overviewWindow, y - 1, 0, "%s", (*it).c_str());
		y--;
	}
}

void
Interface::setStatusMessage(std::string msg)
{
	statusMessage = msg;
	werase(statusLine);
	mvwprintw(statusLine, 0, 0, "%s", statusMessage.c_str());
	wrefresh(statusLine);
	statusTime = time(NULL);
}

void
Interface::addTorrent(std::string fname)
{
	client->addTorrent(fname);
}

void
Interface::handleCompletion()
{
	string path;
	string fname;

	/* First of all, dissect the input in path/fname parts */
	string::size_type pos = addString.find_last_of("/");
	if (pos != string::npos) {
		path = addString.substr(0, pos);
		fname = addString.substr(pos + 1);
	} else {
		path = ".";
		fname = addString;
	}

	/* Try to open this directory, and make a list of all matches */
	DIR* dir = opendir(path.c_str());
	if (dir == NULL)
		return;
	vector<string> matches;
	while (true) {
		struct dirent* de = readdir(dir);
		if (de == NULL)
			break;
		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
			continue;
		if (strncmp(de->d_name, fname.c_str(), fname.size()) != 0)
			continue;
		matches.push_back(de->d_name);
	}
	closedir(dir);

	/* Don't bother continueing if nothing matches */
	tabMatches = matches;
	if (matches.size() == 0)
		return;

	/*
	 * OK, we now need to determine the largest common prefix shared by the
	 * matches; we do this by taking the first match, and simply compare each
	 * charachter of each match one-by-one until we either run out of matches or
	 * when the determined common prefix is the empty string.
	 */
	std::string common_prefix = matches.front();
	matches.erase(matches.begin(), matches.begin() + 1);
	for (vector<string>::iterator it = matches.begin();
	     it != matches.end(); it++) {
		string s = *it;
		unsigned int len;
		for (len = 0; len < std::min(s.size(), common_prefix.size()); len++) {
			if (s[len] != common_prefix[len])
				break;
		}
		common_prefix = common_prefix.substr(0, len);
		if (common_prefix.length() == 0)
			return;
	}

	addString = common_prefix;
}

void
Interface::alterUploadRate(int delta)
{
	unsigned int rate = client->getUploadRate();
	if (delta < 0 && rate < (unsigned int)-delta)
		rate = 0;
	else
		rate += delta;
	client->setUploadRate(rate);

	char tmp[64];
	if (rate == 0) {
		strcpy(tmp, "unlimited");
	} else {
		snprintf(tmp, sizeof(tmp), "%u", rate / 1024);
	}
	std::string msg = "Upload rate changed to ";
	msg += tmp;
	msg += " KB/sec";
	setStatusMessage(msg);
}

void
Interface::handleResize()
{
	struct winsize size;
	if (ioctl(0, TIOCGWINSZ, &size) != 0)
		return;

	resizeterm(size.ws_row, size.ws_col);

	updateWindows();
}

void
Interface::updateWindows()
{
	if (overviewWindow != NULL) delwin(overviewWindow);
	if (infoWindow != NULL) delwin(infoWindow);
	if (statusLine != NULL) delwin(statusLine);
		
	overviewWindow = newwin(LINES - 1, COLS / 2, 0, 0);
	infoWindow = newwin(LINES - 1, COLS / 2, 0, COLS / 2);
	statusLine = newwin(0, 0, LINES - 1, 0);

	overview->setWindow(overviewWindow);
	info->setWindow(infoWindow);
}

void
Interface::redraw()
{
	overview->draw();
	info->draw(overview->getSelectedTorrent());
	update();
	wrefresh(overviewWindow);
	refresh();
}

void
Interface::handleSearchInput(int ch)
{
	/*
	 * XXX this should be somehow merged with handleAddInput as most of the input
	 * stuff is the same
	 */
	switch(ch) {
		case 0x0a: /* return */
			if (searchString.size() != 0) {
				/* First of all, lowercase our search string so we can do case insensitive matching */
				string s(searchString);
				transform(s.begin(), s.end(), s.begin(), ::tolower);
			
				/*
			 	 * Wade through all torrents and see if we have a match!
			 	 */
				TorrentInfoVector& torrents = client->getTorrents();
				for (TorrentInfoVector ::iterator it = torrents.begin();
				     it != torrents.end(); it++) {
					TorrentInfo* ti = *it;
					string torrentName(ti->getTorrent()->getName());
					if (torrentName.size() < s.size())
						continue;
					transform(torrentName.begin(), torrentName.end(), torrentName.begin(), ::tolower);

					if (torrentName.compare(0, s.size(), s) != 0)
						continue;

					/* Got it! */
					overview->selectTorrent(ti);
					break;
				}
			}
			searching = false;
			return;
		case 0x1b: /* escape */
			searching = false;
			return;
		case KEY_BACKSPACE:
			if (searchString.size() > 0)
				searchString = searchString.substr(0, searchString.size() - 1);
			return;
		case 0x15: /* ctrl-u */
			searchString = "";
			return;
		case 0x17: /* ctrl-w */
			string::size_type pos = searchString.find_last_of(" ");
			if (pos != string::npos)
				searchString = searchString.substr(0, pos);
			else
				searchString = "";
			return;
	}

	if (!isprint(ch))
		return;
	searchString += ch;
}

/* vim:set ts=2 sw=2: */
