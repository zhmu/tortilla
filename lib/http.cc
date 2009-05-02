#include <curl/curl.h>
#include <string.h>
#include "exceptions.h"
#include "http.h"

using namespace std;

size_t
http_write_string(void* buffer, size_t size, size_t nmemb, void* userp)
{
	string* s = (string*)userp;

	s->append((const char*)buffer, size * nmemb);
	return size * nmemb;
}

string
HTTP::get(string url, map<string, string> params)
{
	if (curl_global_init(CURL_GLOBAL_ALL))
		throw HTTPException("unable to init curl library");

	CURL* curl = curl_easy_init();
	if (curl == NULL) {
		curl_global_cleanup();
		throw HTTPException("unable to create curl object");
	}

	/*
	 * Construct the full GET request here; this works by fetching the key=value
	 * pairs from params, escaping both of them and adding them to the URL.
	 */
	string fullurl = url;
	for (map<string, string>::iterator it = params.begin();
	       it != params.end(); it++) {
		char *value;
		/*
		 * Note: do not escape the key; this causes issues with several braindead	
		 * trackers :-/
		 */
		value = curl_easy_escape(curl, (*it).second.c_str(), 0);
		if (value == NULL) {
			/* Unlikely, yet we'd better check for it */
			curl_easy_cleanup(curl);
			curl_global_cleanup();
			throw HTTPException("unable to escape parameters");
		}

		/*
		 * Now add ?key=value or &key=value (the first only for the
		 * very first parameter
		 */
		fullurl += (fullurl == url) ? "?" : "&";
		fullurl += (*it).first + string("=") + value;
		curl_free(value);
	}

	string s;
	curl_easy_setopt(curl, CURLOPT_URL, fullurl.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write_string);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);

	/* XXX we don't do IPv6 yet */
	curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

	CURLcode cc = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	curl_global_cleanup();
	if (cc != 0)
		throw HTTPException("unable to talk to tracker");
	return s;
}

/* vim:set ts=2 sw=2: */
