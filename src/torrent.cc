#include <iostream>
#include <sstream>
#include <stdint.h>
#include "exceptions.h"
#include "http.h"
#include "sha1.h"
#include "torrent.h"

using namespace std;

Torrent::Torrent(Metadata* md)
{
	metadata = md;
	downloaded = 0; uploaded = 0; left = 0;

	/* XXX these two variables should be passed from upper hand some day */
	port = 0; peerID = "123456789abcdef01234";

	/*
	 * Ensure the 'info' and 'announce' metadata fields exist; we can't do much
	 * without them.
	 */
	MetaDictionary* dictionary = md->getDictionary();
	MetaDictionary* info = dynamic_cast<MetaDictionary*>((*dictionary)["info"]);
	if (info == NULL)
		throw TorrentException("metadata doesn't contain an info dictionary");
	if (dynamic_cast<MetaString*>((*dictionary)["announce"]) == NULL)
		throw TorrentException("metadata doesn't contain an announce URL");

	/* Construct the SHA1 hash of the 'info' dictionary  */
	stringbuf sb;
	ostream os(&sb); istream is(&sb);
	HashSHA1 sha1(is);
	os << *info;
 	infoHash = sha1.getHash();

	Metadata* m = contactTracker("started");
	m->getDictionary()->dump();
}

Metadata*
Torrent::contactTracker(std::string event)
{
	map<string, string> m;

	/* Construct the tracker request, and off it goes */
	m["info_hash"] = infoHash;
	m["peer_id"] = peerID;
	if (event != "")
		m["event"] = event;
	m["downloaded"] = convertInteger(downloaded);
	m["uploaded"] = convertInteger(uploaded);
	m["left"] = convertInteger(left);
	m["port"] = convertInteger(port);
	string result = HTTP::get((dynamic_cast<MetaString*>((*metadata->getDictionary())["announce"]))->getString(), m);

	/* Parse the result as metadata (which it should be) */
	stringbuf sb(result);
	istream is(&sb);
	return new Metadata(is);
}

Torrent::~Torrent()
{
}

std::string
Torrent::convertInteger(uint64_t i)
{
	ostringstream o;
	o << i;
	return o.str();
}

/* vim:set ts=2 sw=2: */
