#include <map>
#include <string>

#ifndef __TORTILLA_HTTP_H__
#define __TORTILLA_HTTP_H__

class HTTP {
public:
	/*! \brief Retrieve a HTTP URL using a GET request
	 *  \param url URL to retrieve
	 *  \param parms A map of key => value pairs as GET parameters
	 *
	 *  Note that this function is blocking and will not return until it is finished.
	 *
	 */
	static std::string get(std::string url, std::map<std::string, std::string> parms);
};

#endif /* __TORTILLA_HTTP_H__ */
