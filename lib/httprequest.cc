#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "exceptions.h"
#include "httprequest.h"
#include "trackertalker.h"

using namespace std;

HTTPRequest::HTTPRequest(TrackerTalker* tt, string url, map<string, string> params)
{
	talker = tt; terminate = false;

	/*
	 * First of all, the URL must begin with 'http://'; nothing else is
	 * supported.
	 */
	string protocol = "http://";
	if (url.find(protocol) != 0)
		throw HTTPException("non-HTTP-trackers are not yet supported");
	url.erase(0, protocol.length());

	/*
	 * Next, grab the server name (this is everything before the /).
	 */
	server = "";
	string::size_type npos = url.find("/");
	if (npos != string::npos) {
		server = url.substr(0, npos);
		url = url.erase(0, npos);
	}

	/* If the server name has a : in it, treat it as port */
	int port = 80; /* XXX should use getservbyname(3) ? */
	npos = server.find(":");
	if (npos != string::npos) {
		port = atoi(server.substr(npos + 1).c_str());
		server = server.erase(npos);
	}

	/*
	 * Construct the full arguments ; this works by fetching the key=value pairs
	 * from params, escaping both of them and adding them to the URL.
	 */
	string args = "";
	for (map<string, string>::iterator it = params.begin();
	       it != params.end(); it++) {
		/*
		 * Note: do not escape the key; this causes issues with several braindead	
		 * trackers :-/
		 */
		if (args != "")
			args += "&";
		args += (*it).first + string("=") + HTTPRequest::escape((*it).second);
	}
	requestString = url + "?" + args;

	/* Create a connection to the server */
	try {
		connection = new Connection(server, port);
	} catch (ConnectionException e) {
		throw HTTPException(string("can't set up connection: ") + e.what());
	}

	/* That's all for now - we need to wait until the connection is made */
	waitingForWrite = true; waitingForRead = false;
}

HTTPRequest::~HTTPRequest()
{
	delete connection;
}

string
HTTPRequest::escape(string s)
{
	/*
	 * Escape anything not in A-Za-z0-9-_.!~*'() ; this is
	 * identical to what URI::Escape does, and it's mandated
	 * by RFC 2396 (and the subsequent RFC2732 update)
	 */
	const string valid = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz01234567890-_.!~*'()";
	string result = "";
	for (string::iterator it = s.begin();
	     it != s.end(); it++) {
		if (valid.find(*it) != string::npos) {
			result += *it;
			continue;
		}

		char tmp[16];
		snprintf(tmp, sizeof(tmp), "%%%x%x",
		 (unsigned char)((*it) >> 4) & 0xf, (unsigned char)(*it) & 0xf);
		result += tmp;
	}
	return result;
}


void
HTTPRequest::process()
{
	/*
	 * If we were waiting for a write, this means we have to write the request.
	 */
	if (waitingForWrite) {
		waitingForWrite = false; waitingForRead = true;

		string s;
		s  = "GET " + requestString + " HTTP/1.0\r\n";
		s += "Host: " + server + "\r\n";
		s += "\r\n";
		connection->write(s.c_str(), s.size());
		return;
	}

	/* We got data! */
	char buf[65535 /* XXX */];
	ssize_t len = connection->read(buf, sizeof(buf));
	if (len < 0) {
		/* Can't connect */
		talker->callbackTrackerRequest("", true);

		/* We have done our duty */
		terminate = true;
		return;
	}
	if (len == 0) {
		/*
		 * Socket was closed; this means we have the complete message. Remove the
		 * HTTP headers and isolate the content.
		 */
		string::size_type npos = buffer.find("\r\n\r\n");
		if (npos != string::npos) {
			buffer.erase(0, npos + 4);
		}

		/* Inform the torrent */
		talker->callbackTrackerRequest(buffer, buffer.length() == 0);

		/* We have our data; leave */
		terminate = true;
		return;
	}
	string s(buf, len);
	buffer += s;
}

int
HTTPRequest::getFD() {
	return connection->getFD();
}

/* vim:set ts=2 sw=2: */
