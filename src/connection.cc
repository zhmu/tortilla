#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <netdb.h>
#include <iostream>
#include "exceptions.h"
#include "connection.h"
#include "tracer.h"

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
	struct addrinfo* rp; fd = -1;
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (fd < 0)
			continue;

		/*
		 * Set the socket to nonblocking; this is used to be able to timeout
		 * connections.
		 */
		int fl = fcntl(fd, F_GETFL, NULL);
		fl |= O_NONBLOCK;
		fcntl(fd, F_SETFL, fl);

		if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0)
			break;
		if (errno == EINPROGRESS) {
			/*
		 	 * Use select(2) to wait at most 5 seconds for the connection to be
			 * made.
			 */
			fd_set fds;
			struct timeval tv;
			FD_ZERO(&fds);
			FD_SET(fd, &fds);
			tv.tv_sec = 5; tv.tv_usec = 0;
			if (select(fd + 1, NULL, &fds, NULL, &tv) > 0) {
				/* Something happened; but is it for good? */
				unsigned int err;
				socklen_t errlen = sizeof(err);
				if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (void*)&err, &errlen) == 0 && err == 0)
					/* Socket was acceptable! */
					break;
			}
		}

		close(fd); fd = -1;
	}
	/* We don't need the address information anymore; just the pointer is enough */
	freeaddrinfo(result);

	if (fd < 0)
		throw ConnectionException("unable to connect to " + host + " port " + portstr);

	/* Socket is fine; get rid of the nonblocking mode before we hand it back */
	int fl = fcntl(fd, F_GETFL, NULL);
	fl &= ~O_NONBLOCK;
	fcntl(fd, F_SETFL, fl);
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
	TRACE(NETWORK, "listening: port=%u", port);
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

size_t
Connection::read(void* buf, size_t len, bool block)
{
	if (!block)
		return ::read(fd, buf, len);

	/* Keep reading until we fill the buffer */
	char* ptr = (char*)buf;
	size_t got = 0;
	while (got < len) {
		ssize_t l = ::read(fd, ptr, len - got);
		if (l <= 0)
			return -1;
		ptr += l; got += l;
	}
	return got;
}

/* vim:set ts=2 sw=2: */
