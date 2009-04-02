#include <ncurses.h>
#include <string>
#include "tortilla/overseer.h"

#ifndef __INTERFACE_H__
#define __INTERFACE_H__

class Interface {
public:
	Interface(Overseer* o);
	~Interface();

	void run();

	Overseer* getOverseer() { return overseer; }

	static std::string formatNumber(uint64_t n);
	static std::string formatHex(const uint8_t* hex, unsigned int len);

private:
	Overseer* overseer;

	WINDOW* overviewWindow;
	WINDOW* infoWindow;
};

#endif /* __INTERFACE_H__ */