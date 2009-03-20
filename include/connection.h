#include <sys/types.h>
#include <sys/socket.h>
#include <string>

#ifndef __CONNECTION_H__
#define __CONNECTION_H__

/*! \brief Implements a TCP connection
 */
class Connection {
public:
	/*! \brief Constructs a new TCP connection
	 *  \param host Hostname or IP address to connect to
	 *  \param port Port number to connect to
	 */
	Connection(std::string host, uint16_t port);

	/*! \brief Constructs a new incoming TCP connection
	 *  \param port Port number to listen on
	 */
	Connection(uint16_t port);

	//! \brief Destructs the TCP connection
	~Connection();

	/*! \brief Writes data to the other side
	 *  \param buf Buffer to write
	 *  \param len Number of bytes to transfer
	 */
	void write(const void* buf, size_t len);

	/*! \brief Accepts a new connection on a listening socket
	 *  \returns A new connection object
	 */
	Connection* acceptConnection();

	//! \brief Retrieve the file descriptor associated with this connection
	inline int getFD() { return fd; }

protected:
	/*! \brief Constructs a TCP connection based on an accepted socket
	 *  \param s File descriptor to use
	 *  \param sa Socket address structure
	 *  \param slen Socket address structure length
	 */
	Connection(int s, struct sockaddr* soa, socklen_t slen);

private:
	//! \brief File descriptor used for the connection
	int fd;
};

#endif /* __CONNECTION_H__ */
