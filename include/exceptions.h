#include <exception>
#include <string>

#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__

//! \brief Base exception class
class TortillaException : public std::exception {
public:
	/*! \brief Construct an exception
	 *  \param errstr Message describing the exception
	 */
	TortillaException(const std::string errstr) {
		message = errstr;
	}

	//! \brief Destructs the exception
	virtual ~TortillaException() throw() { }

	//! \brief Retrieve the exception message
	virtual const char* what() const throw() { return message.c_str(); }

private:
	//! \brief Exception message
	std::string message;
};

//! \brief Exception used for bad metadata
class MetadataException : public TortillaException {
public:
	MetadataException(const std::string errstr) : TortillaException(errstr) { }
};

//! \brief Exception used for bad HTTP
class HTTPException : public TortillaException {
public:
	HTTPException(const std::string errstr) : TortillaException(errstr) { }
};

//! \brief Exception used for torrent problems
class TorrentException : public TortillaException {
public:
	TorrentException(const std::string errstr) : TortillaException(errstr) { }
};

#endif /* __EXCEPTION_H__ */
