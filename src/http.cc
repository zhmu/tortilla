#include <curl/curl.h>
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
	if (curl == NULL)
		throw HTTPException("unable to create curl object");

	/*
	 * Construct the full GET request here; this works by fetching the key=value
	 * pairs from params, escaping both of them and adding them to the URL.
	 */
	string fullurl = url;
	for (map<string, string>::iterator it = params.begin();
	       it != params.end(); it++) {
		char *key, *value;
		key   = curl_easy_escape(curl, (*it).first.c_str(),  0);
		value = curl_easy_escape(curl, (*it).second.c_str(), 0);
		if (key == NULL || value == NULL) {
			/* Unlikely, yet we'd better check for it */
			if (key != NULL)   curl_free(key);
			if (value != NULL) curl_free(value);
			throw HTTPException("unable to escape parameters");
		}

		/*
		 * Now add ?key=value or &key=value (the first only for the
		 * very first parameter
		 */
		fullurl += (fullurl == url) ? "?" : "&";
		fullurl += key + string("=") + value;
		curl_free(key); curl_free(value);
	}

	string s;
	curl_easy_setopt(curl, CURLOPT_URL, fullurl.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write_string);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);

	CURLcode cc = curl_easy_perform(curl);
	if (cc != 0)
		throw HTTPException("unable to talk to tracker");
	return s;
}

/* vim:set ts=2 sw=2: */
