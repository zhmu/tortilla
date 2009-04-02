#include <string.h>
#include "info.h"

using namespace std;

Info::Info(WINDOW* w, Interface* iface)
{
	window = w; interface = iface;
}

void
Info::draw(Torrent* t)
{
	werase(window);
	mvwprintw(window, 0, 0, "Info hash:      %s",
	 Interface::formatHex(t->getInfoHash(), TORRENT_HASH_LEN).c_str());
	mvwprintw(window, 1, 0, "Pieces:         %u / %u (%u hashing)",
	 t->getNumPiecesComplete(), t->getNumPieces(), t->getNumPiecesHashing());
	mvwprintw(window, 2, 0, "Data left:      %llu / %llu bytes",
	 (unsigned long)t->getBytesLeft(), (unsigned long)t->getTotalSize());
	mvwprintw(window, 3, 0, "Peers:          %u", t->getNumPeers());

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
		mvwprintw(window, y, 0, line);
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
		mvwprintw(window, y, 0, line);
		y++;
		snprintf(line, sizeof(line), "flags:");
		if (pi.isSnubbed()) strcat(line, " snubbed");
		if (pi.isPeerInterested()) strcat(line, " peer_int");
		if (pi.isPeerChoked()) strcat(line, " peer_chk");
		if (pi.areInterested()) strcat(line, " are_int");
		if (pi.areChoking()) strcat(line, " are_chk");
		mvwprintw(window, y, 0, line);
		y++;
	}
	wrefresh(window);
}

/* vim:set ts=2 sw=2: */
