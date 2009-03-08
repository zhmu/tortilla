#include <sys/types.h>
#include <sys/socket.h>
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
		throw ConnectionException("unable to connect to " + host);
}

void
Connection::write(const void* buf, size_t len)
{
	if (::write(fd, buf, len) != len)
		cerr << "warning: short write" << endl;
}

Connection::~Connection()
{
	close(fd);
}

/* vim:set ts=2 sw=2: */
