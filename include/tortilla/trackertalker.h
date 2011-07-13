#include <boost/thread/mutex.hpp>
#include <string>
#include <vector>
#include "metadata.h"
#include "torrent.h"

#ifndef __TORTILLA_TRACKERTALKER__
#define __TORTILLA_TRACKERTALKER__

namespace Tortilla {

class HTTPRequest;

/*! \brief Contains a list of trackers in a given tier
 */
class AnnounceTier {
public:
	/*! \brief Constructs a new announce tier object based on a tier list
	 *  \param tt Talker we belong to
	 *  \param ml Metalist to use for the trackers
	 */
	AnnounceTier(TrackerTalker* tt, const MetaList* ml);

	/*! \brief Constructs a new announce tier object based on a single tracker URL
	 *  \param tt Talker we belong to
	 *  \param url URL to use
	 */
	AnnounceTier(TrackerTalker* tt, std::string url);

	//! \brief Resets the tracker order
	void resetCurrentTracker();

	/*! \brief Retrieves the next tracker from the tier
	 *  \returns Tracker, or "" if the tracker does not exist
	 */
	std::string getNextTracker();

	/*! \brief Demotes the current tracker
	 *
	 *  This should be called if the current tracker cannot be reached.
	 */
	void demoteCurrentTracker();

	/*! \brief Promotes the current tracker
	 *
	 *  This should be called if the current tracker was successfully
	 *  reached.
	 */
	void promoteCurrentTracker();

protected:
	//! \brief Contains the list of trackers in this tier
	std::vector<std::string> trackers;

	//! \brief Tracker talker we belon to
	TrackerTalker* talker;

	//! \brief Retrieves the tracer used for logging
	Tracer* getTracer();

	//! \brief Current tracker being used
	unsigned int currentTracker;
};

/*! \brief Handles communicating with a tracker
 */
class TrackerTalker {
public:
	/*! \brief Constructs a new tracker communications aid
	 *  \param t Torrent we belong to
	 *  \param dictionary Dictionary to use
	 */
	TrackerTalker(Torrent* t, MetaDictionary* dictionary);

	//! \brief Destructs the tracker communications aid
	virtual ~TrackerTalker();

	//! \brief Retrieve the torrent we are bound to
	inline Torrent* getTorrent() { return torrent; }

	/*! \brief Issues a request to the tracker
	 *  \param req Request to make
	 */
	void request(std::map<std::string, std::string> req);

	/*! \brief Called if the tracker request completed
	 *  \param result Result message
	 *  \param error If set, request failed
	 */
	void callbackTrackerRequest(std::string result, bool error);

protected:
	/*! \brief Attempts to perform the tracker request
	 *  \returns true if the event was successfully queued, false otherwise
	 */
	bool tryRequest();

	//! \brief List of tracker tiers
	std::vector<AnnounceTier*> tiers;

	//! \brief Torrent we are bound to
	Torrent* torrent;

	//! \brief Retrieves the tracer used for logging
	inline Tracer* getTracer() { return torrent->getTracer(); }

	//! \brief Request that needs to be done
	std::map<std::string, std::string> currentRequest;

	//! \brief Current tier we are handling
	unsigned int currentTier;

	//! \brief HTTP request we are using
	HTTPRequest* httpRequest;

	//! \brief Mutex protecting the data
	boost::mutex mtx_data;
};

}

#endif /* __TORTILLA_TRACKERTALKER__ */
