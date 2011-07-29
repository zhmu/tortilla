#include <ncurses.h>
#include <string>
#include <vector>
#include <stdint.h>

#ifndef __INTERFACE_H__
#define __INTERFACE_H__

//! \brief Number of seconds after which we reset the status message
#define INTERFACE_STATUS_TIMEOUT 5

class Client;
class Overview;
class Info;

class Interface {
public:
	Interface(Client* c);
	~Interface();

	void run();

	static std::string formatNumber(uint64_t n);
	static std::string formatHex(const uint8_t* hex, unsigned int len);

	void addTorrent(std::string fname);
	void setStatusMessage(std::string msg);

	void handleResize();

protected:
	void update();
	void alterUploadRate(int delta);
	void redraw();

private:
	void handleInput();
	void handleAddInput(int ch);
	void handleSearchInput(int ch);
	void handleCompletion();

	void updateWindows();

	Client* client;
	Overview* overview;
	Info* info;

	WINDOW* overviewWindow;
	WINDOW* infoWindow;
	WINDOW* statusLine;

	//! \brief Last time we updated the status message
	time_t statusTime;

	//! \brief Are we currently adding a torrent?
	bool adding;

	//! \brief Are we searching for a torrent?
	bool searching;

	//! \brief Which files match currently?
	std::vector<std::string> tabMatches;

	std::string addString;
	std::string searchString;
	std::string statusMessage;
};

#endif /* __INTERFACE_H__ */
