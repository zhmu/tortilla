#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>
#include <string>
#include <stdint.h>

#ifndef __TORTILLA_CONNECTION_H__
#define __TORTILLA_CONNECTION_H__

namespace Tortilla {

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
	 *  \return Number of bytes sent
	 */
	ssize_t write(const void* buf, size_t len);

	/*! \brief Reads data from the other side
	 *  \param buf Buffer to read
	 *  \param len Number of bytes to read
	 *  \param block If true, block until all len bytes are read
	 *  \returns Number of bytes read, always len if block is true
	 */
	ssize_t read(void* buf, size_t len, bool block = false);

	/*! \brief Accepts a new connection on a listening socket
	 *  \returns A new connection object
	 */
	Connection* acceptConnection();

	//! \brief Retrieve the file descriptor associated with this connection
	inline int getFD() const { return fd; }

	//! \brief Retrieve a human-readable endpoint description
	inline const std::string& getEndpoint() const { return endpoint; }

	//! \brief Are we connecting?
	bool areConnecting() const { return connecting; }

	//! \brief Used to signal connection is done!
	void connectionDone() { connecting = false; }

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

	//! \brief Are we currently waiting for a connect(2) to finish?
	bool connecting;

	//! \brief Human-readable endpoint name
	std::string endpoint;
};

}

#endif /* __TORTILLA_CONNECTION_H__ */
