#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <iostream>
#include "exceptions.h"
#include "connection.h"

using namespace std;

Connection::Connection(string host, uint16_t port)
{
	char portstr[8 /* 8 is enough to hold to a 16 bit unsigned int value */];
	struct addrinfo hints;
	struct addrinfo* result;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	snprintf(portstr, sizeof(portstr), "%u", port);
	int i = getaddrinfo(host.c_str(), portstr, &hints, &result);
	if (i != 0)
		throw ConnectionException("getaddrinfo(): " + string(gai_strerror(i)));

	/* We got a list of addresses - try them one by one */
	struct addrinfo* rp;
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (fd < 0)
			continue;

		if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0)
			break;

		close(fd);
	}
	/* We don't need the address information anymore; just the pointer is enough */
	freeaddrinfo(result);

	if (rp == NULL)
		throw ConnectionException("unable to connect to " + host + " port " + portstr);
}

Connection::Connection(uint16_t port)
{
	struct sockaddr_in sin;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		throw ConnectionException("unable create socket");

	unsigned int i = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i)) < 0)
		throw ConnectionException("can't set reuseaddr socket options");

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	if (bind(fd, (struct sockaddr*)&sin, sizeof(sin)) < 0)
		throw ConnectionException("bind(): " + string(strerror(errno)));

	if (listen(fd, 5) < 0)
		throw ConnectionException("listen(): " + string(strerror(errno)));
}

Connection::Connection(int s, struct sockaddr* soa, socklen_t slen)
{
	fd = s;
}

void
Connection::write(const void* buf, size_t len)
{
	if ((size_t)::write(fd, buf, len) != len)
		cerr << "warning: short write" << endl;
}

Connection*
Connection::acceptConnection()
{
	struct sockaddr soa;
	socklen_t slen = sizeof(soa);

	int s = accept(fd, &soa, &slen);
	if (s < 0) {
		cerr << "accept(): " << string(strerror(errno)) << endl;
		return NULL;
	}
	return new Connection(s, &soa, slen);
}

Connection::~Connection()
{
	close(fd);
}

/* vim:set ts=2 sw=2: */
