#include <ncurses.h>
#include <string>
#include "tortilla/overseer.h"

#ifndef __INTERFACE_H__
#define __INTERFACE_H__

class Overview;
class Info;

class Interface {
public:
	Interface(Overseer* o);
	~Interface();

	void run();

	Overseer* getOverseer() { return overseer; }

	static std::string formatNumber(uint64_t n);
	static std::string formatHex(const uint8_t* hex, unsigned int len);

	void addTorrent(std::string fname);

protected:
	void update();
	void alterUploadRate(int delta);

private:
	void handleInput();
	void handleAddInput(int ch);
	void handleCompletion();

	Overseer* overseer;
	Overview* overview;
	Info* info;

	WINDOW* overviewWindow;
	WINDOW* infoWindow;

	//! \brief Are we currently adding a torrent?
	bool adding;

	//! \brief Which files match currently?
	std::vector<std::string> tabMatches;

	std::string addString;
	std::string statusMessage;
};

#endif /* __INTERFACE_H__ */
