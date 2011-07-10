#include <boost/thread/locks.hpp>
#include <algorithm>
#include "trackertalker.h"
#include "macros.h"
#include "metafield.h"
#include "httprequest.h"
#include "exceptions.h"
#include "tracer.h"

using namespace std;
using namespace boost;

#define TRACER (getTracer())

AnnounceTier::AnnounceTier(TrackerTalker* tt, const MetaList* ml)
{
	talker = tt;
	for (list<MetaField*>::const_iterator it = ml->getList().begin();
		   it != ml->getList().end(); it++) {
		MetaString* ms = dynamic_cast<MetaString*>(*it);
		if (ms == NULL)
			throw TrackerException("announce tear list doesn't contain a string");
		string trackerURL = ms->getString();
		trackers.push_back(trackerURL);
		TRACE(TRACKER, "added tracker: %s", trackerURL.c_str());
	}
	random_shuffle(trackers.begin(), trackers.end());
}

AnnounceTier::AnnounceTier(TrackerTalker* tt, std::string url)
{
	talker = tt;
	trackers.push_back(url);
}

void
AnnounceTier::resetCurrentTracker()
{
	currentTracker = 0;
}

string
AnnounceTier::getNextTracker()
{
	if (currentTracker >= trackers.size())
		return "";
	return trackers[currentTracker++];
}

void
AnnounceTier::demoteCurrentTracker()
{
	TRACE(TRACKER, "TODO: demote");
}

void
AnnounceTier::promoteCurrentTracker()
{
	TRACE(TRACKER, "TODO: promote");
}

Tracer*
AnnounceTier::getTracer() {
	return talker->getTorrent()->getTracer();
}

TrackerTalker::TrackerTalker(Torrent* t, MetaDictionary* dictionary)
{
	torrent = t; httpRequest = NULL;

	const MetaList* mlList = dynamic_cast<const MetaList*>((*dictionary)["announce-list"]);
	if (mlList == NULL) {
		/* New-style announce list unavailable; revert to old single URL */
		const MetaString* msAnnounce = dynamic_cast<const MetaString*>((*dictionary)["announce"]);
		if (msAnnounce == NULL)
			throw TrackerException("metadata doesn't contain an announce URL or list");

		string url = msAnnounce->getString();
		TRACE(TRACKER, "added old-style tracker: %s", url.c_str());
		tiers.push_back(new AnnounceTier(this, url));
		return;
	}

	/*
	 * An announce-list is made up of lists of sublists. Each sublist is an
	 * announcement tier, and contains a list of strings which must be shuffeled
	 * and stored. Each of the tiers must be tried in-order.
 	 */
	for (list<MetaField*>::const_iterator it = mlList->getList().begin();
		   it != mlList->getList().end(); it++) {
			MetaList* tierList = dynamic_cast<MetaList*>(*it);
			if (tierList == NULL)
				throw TorrentException("announce list doesn't contain a list element");

			tiers.push_back(new AnnounceTier(this, tierList));
	}
}

TrackerTalker::~TrackerTalker()
{
	for (vector<AnnounceTier*>::iterator it = tiers.begin();
	     it != tiers.end(); it++) {
		delete (*it);
	}
}

void
TrackerTalker::request(map<string, string> req)
{
	/* Always start at the first tier with this request */
	currentTier = 0; currentRequest = req;
	for (vector<AnnounceTier*>::iterator it = tiers.begin();
	     it != tiers.end(); it++) {
		(*it)->resetCurrentTracker();
	}

	/* Give it a spin */
	if (!tryRequest()) {
		/* Wow, this failed extremely quickly - talk to the torrent */
		torrent->callbackTrackerReply("", true);
	}
}

bool
TrackerTalker::tryRequest()
{
	unique_lock<mutex> lock(mtx_data);
	while (true) {
		/* Find the next announcer to use */
		if (currentTier >= tiers.size())
			break;
		AnnounceTier* at = tiers[currentTier];
		string url = at->getNextTracker();
		if (url == "") {
			currentTier++;
			continue;
		}

		/* OK, we got an URL - try to chat with it */
		lock.unlock(); /* drop data lock while talking to tracker */
		try {
			TRACE(TRACKER, "attempting to contact tracker '%s'", url.c_str());
			httpRequest = new HTTPRequest(this, url, currentRequest);
			torrent->addRequest(httpRequest);
			return true;
		} catch (HTTPException e) {
			TRACE(TRACKER, "unable to communicate with tracker '%s': %s", url.c_str(), e.what());
		}

		/* Fallthrough to the next tracker - reacquire lock */
		lock.lock();
	}
	return false;
}

void 
TrackerTalker::callbackTrackerRequest(std::string result, bool error)
{
	TRACE(TRACKER, "tracker callback: error=%u", error ? 1 : 0);

	/* Get rid of the request ASAP - the receiver will clean it up */
	{
		unique_lock<mutex> lock(mtx_data);
		httpRequest = NULL;
	}

	AnnounceTier* at = tiers[currentTier];

	if (error) {
		at->demoteCurrentTracker();
		if (!tryRequest()) {
			/* Give up... */
			torrent->callbackTrackerReply(result, true);
		}
		return;
	}

	at->promoteCurrentTracker();
	torrent->callbackTrackerReply(result, false);
}

/* vim:set ts=2 sw=2: */
