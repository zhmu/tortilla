#include <string.h>
#include "info.h"

using namespace std;

Info::Info(WINDOW* w, Interface* iface)
{
	window = w; interface = iface; y_offset = 0;
}

void
Info::draw(Torrent* t)
{
	werase(window);
	if (t == NULL) {
		printxyf(0, 0, "<no torrent selected>");
		wrefresh(window);
		return;
	}
	printxyf(0, 0, "Info hash:      %s",
	 Interface::formatHex(t->getInfoHash(), TORRENT_HASH_LEN).c_str());
	printxyf(0, 1, "Pieces:         %u / %u (%u hashing)",
	 t->getNumPiecesComplete(), t->getNumPieces(), t->getNumPiecesHashing());
	printxyf(0, 2, "Data left:      %llu / %llu bytes",
	 (unsigned long)t->getBytesLeft(), (unsigned long)t->getTotalSize());
	printxyf(0, 3, "Peers:          %u", t->getNumPeers());

	vector<PieceInfo> pieces = t->getPieceDetails();
	unsigned int y = 4;
	for (unsigned int i = 0; i < pieces.size(); i += 40) {
		char line[1024 /* XXX */];
		snprintf(line, sizeof(line), "%5u: ", i);

		for (unsigned int j = 0; j < 40; j++) {
			if (i + j >= pieces.size())
				break;
			char tmp[2];
			PieceInfo& pi = pieces[i + j];
			if (pi.getHave())
				tmp[0] = pi.isHashing() ? '?' : '#';
			else if (pi.isRequested())
				tmp[0] = 'R';
			else
				tmp[0] = '.';
			tmp[1] = '\0';
			strncat(line, tmp, sizeof(line));
		}
		strncat(line, " ", sizeof(line));
		printxyf(0, y, line);
		y++;
	}

	vector<PeerInfo> peers = t->getPeerDetails();
	for (unsigned int i = 0; i < peers.size(); i++) {
		PeerInfo& pi = peers[i];

		char line[1024 /* XXX */];
		snprintf(line, sizeof(line), "%s, %s, rx/tx: %s / %s",
		 pi.getEndpoint().c_str(), 
		 pi.isIncoming() ? "in" : "out",
		 Interface::formatNumber(pi.getRxRate()).c_str(),
		 Interface::formatNumber(pi.getTxRate()).c_str());
		printxyf(0, y, line);
		y++;
		snprintf(line, sizeof(line), "flags:");
		if (pi.isSnubbed()) strcat(line, " snubbed");
		if (pi.isPeerInterested()) strcat(line, " peer_int");
		if (pi.isPeerChoked()) strcat(line, " peer_chk");
		if (pi.areInterested()) strcat(line, " are_int");
		if (pi.areChoking()) strcat(line, " are_chk");
		printxyf(0, y, line);
		y++;
	}
	wrefresh(window);
}

void
Info::printxyf(unsigned int x, unsigned int y, const char* format, ...)
{
	va_list vl;

	if (y < y_offset)
		return;

	if (wmove(window, y - y_offset, x) == ERR)
		return;

	va_start(vl, format);
	vwprintw(window, format, vl);
	va_end(vl);
}

void
Info::scrollUp()
{
	unsigned int lines = getmaxy(window);

	if (y_offset - lines / 2 < 0)
		y_offset = 0;
	else
		y_offset -= lines / 2;
}

void
Info::scrollDown()
{
	unsigned int lines = getmaxy(window);

	y_offset += lines / 2;
}

/* vim:set ts=2 sw=2: */
