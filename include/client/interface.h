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

private:
	void handleInput();
	void handleAddInput(int ch);

	Overseer* overseer;
	Overview* overview;
	Info* info;

	WINDOW* overviewWindow;
	WINDOW* infoWindow;

	//! \brief Are we currently adding a torrent?
	bool adding;

	std::string addString;
	std::string statusMessage;
};

#endif /* __INTERFACE_H__ */
