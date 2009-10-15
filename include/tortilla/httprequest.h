#include <map>
#include <string>
#include "connection.h"

#ifndef __TORTILLA_HTTPREQUEST_H__
#define __TORTILLA_HTTPREQUEST_H__

class TrackerTalker;

class HTTPRequest {
friend class Receiver;
public:
	/*! \brief Initialize a HTTP request
	 *  \param tt Tracker communicator the request belongs to
	 *  \param url URL to use
	 *  \param params Map of key => value arguments
	 */
	HTTPRequest(TrackerTalker* tt, std::string url, std::map<std::string, std::string> parms);

	//! \brief Tear down the HTTP request
	~HTTPRequest();

	//! \brief Escapes a string in a RFC 2396-approved way
	static std::string escape(std::string s);

	//! \brief Awaken!
	void process();

	//! \brief Do we need to be removed?
	bool mustTerminate() { return terminate; }

protected:
	//! \brief Retrieve the file descriptor used
	int getFD();

	//! \brief Need to awake the request for a read?
	bool isWaitingForRead() { return waitingForRead; }

	//! \brief Need to awake the request for a write?
	bool isWaitingForWrite() { return waitingForWrite; }

private:
	//! \brief Tracker communicator we belong to
	TrackerTalker* talker;

	//! \brief Connection used
	Connection* connection;

	//! \brief Do we intend to wait for a read?
	bool waitingForRead;

	//! \brief Do we intend to wait for a write?
	bool waitingForWrite;

	//! \brief Server name we are connecting to
	std::string server;

	//! \brief Complete request to send
	std::string requestString;

	//! \brief Buffer of received data
	std::string buffer;

	//! \brief Do we need to be terminated?
	bool terminate;
};

#endif /* __TORTILLA_HTTPREQUEST_H__ */
