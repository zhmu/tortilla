#include <string.h>
#include "tortilla/torrent.h"
#include "torrentinfo.h"
#include "info.h"

using namespace std;

Info::Info(Interface* iface)
{
	window = NULL; interface = iface; y_offset = 0; num_lines = 0; panel = PANEL_PIECES;
}

void
Info::draw(TorrentInfo* ti)
{
	werase(window);
	if (ti == NULL) {
		printxyf(0, 0, "<no torrent selected>");
		wrefresh(window);
		return;
	}

	printxyf(0, 0, "Info hash:      %s",
	 Interface::formatHex(ti->getInfoHash(), TORRENT_HASH_LEN).c_str());
	printxyf(0, 1, "Pieces:         %u / %u (%u hashing)",
	 ti->getNumPiecesCompleted(), ti->getNumPieces(), ti->getNumPiecesHashing());
	printxyf(0, 2, "Data left:      %s / %s",
	 Interface::formatNumber(ti->getNumBytesLeft()).c_str(),
	 Interface::formatNumber(ti->getTotalSize()).c_str());
	printxyf(0, 3, "Peers:          %u active / %u pending",
	 ti->getNumPeers(), ti->getNumPendingPeers());

	unsigned int y = 4;
	switch(panel) {
		case PANEL_PIECES:
			drawPieces(ti, y);
			break;
		case PANEL_PEERS:
			drawPeers(ti, y);
			break;
		case PANEL_LOG:
			drawLog(ti, y);
			break;
		case PANEL_FILES:
			drawFiles(ti, y);
			break;
	}
	num_lines = y;
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

	if (y_offset < lines / 2)
		y_offset = 0;
	else
		y_offset -= lines / 2;
}

void
Info::scrollDown()
{
	unsigned int lines = getmaxy(window);

	if (y_offset + lines / 2 < num_lines)
		y_offset += lines / 2;
}

void
Info::drawPieces(TorrentInfo* ti, unsigned int& y)
{
	vector<Tortilla::PieceInfo> pieces = ti->getTorrent()->getPieceDetails();
	for (unsigned int i = 0; i < pieces.size(); i += 40) {
		char line[1024 /* XXX */];
		snprintf(line, sizeof(line), "%5u: ", i);

		for (unsigned int j = 0; j < 40; j++) {
			if (i + j >= pieces.size())
				break;
			char tmp[2];
			Tortilla::PieceInfo& pi = pieces[i + j];
			if (pi.getHave())
				tmp[0] = pi.isHashing() ? '?' : '#';
			else if (pi.isQueued())
				tmp[0] = 'Q';
			else
				tmp[0] = '.';
			tmp[1] = '\0';
			strncat(line, tmp, sizeof(line));
		}
		strncat(line, " ", sizeof(line));
		printxyf(0, y, line);
		y++;
	}
}

void
Info::drawPeers(TorrentInfo* ti, unsigned int& y)
{
	vector<Tortilla::PeerInfo> peers = ti->getTorrent()->getPeerDetails();
	for (unsigned int i = 0; i < peers.size(); i++) {
		Tortilla::PeerInfo& pi = peers[i];

		char line[1024 /* XXX */];
		snprintf(line, sizeof(line), "[%3u%%] %s (%s), rx/tx: %s / %s",
		 (int)((pi.getNumPieces() / (float)ti->getTorrent()->getNumPieces()) * 100.0f),
		 pi.getEndpoint().c_str(), 
		 pi.isIncoming() ? " in" : "out",
		 Interface::formatNumber(pi.getRxRate()).c_str(),
		 Interface::formatNumber(pi.getTxRate()).c_str());
		printxyf(0, y, line);
		y++;
		if (pi.isSnubbed()) printxyf(7, y, "snubbed");
		if (pi.isPeerInterested()) {
			wattron(window, A_BOLD);
			printxyf(15, y, "peer_int");
			wattroff(window, A_BOLD);
		}
		if (pi.isPeerChoked()) printxyf(24, y, "peer_chk");
		if (pi.areInterested()) {
			wattron(window, A_REVERSE);
			printxyf(33, y, "are_int");
			wattroff(window, A_REVERSE);
		}
		if (pi.areChoking()) printxyf(41, y, "are_chk");
		y++;
	}
}

void
Info::drawLog(TorrentInfo* ti, unsigned int& y)
{
	list<string> messages = ti->getTorrent()->getMessageLog();

	if (messages.size() == 0) {
		printxyf(0, y, "<message log is empty>");
		y++;
	} else {
		for (list<string>::iterator it = messages.begin();
				 it != messages.end(); it++) {
			printxyf(0, y, "%s", (*it).c_str());
			y++;
		}
	}
}

void
Info::drawFiles(TorrentInfo* ti, unsigned int& y)
{
	vector<Tortilla::PieceInfo> pieces = ti->getTorrent()->getPieceDetails();
	vector<Tortilla::FileInfo> files = ti->getTorrent()->getFileDetails();

	for (unsigned int i = 0; i < files.size(); i++) {
		Tortilla::FileInfo& fi = files[i];
	
		unsigned int completed = 0;
		for (unsigned int p = 0; p < fi.getNumPieces(); p++) {
			if ( pieces[fi.getFirstPieceNum() + p].getHave() &&
			    !pieces[fi.getFirstPieceNum() + p].isHashing())
				completed++;
		}

		printxyf(0, y, "%u%% - %s",
		 (int)(((float)completed / (float)fi.getNumPieces()) * 100.f),
		 fi.getFilename().c_str());
		y++;
	}
}

/* vim:set ts=2 sw=2: */
