/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef myreferences_H
#define myreferences_H

#include "config.h"

#include <string>
#include <vector>
#include <map>
#include <list>
#include <time.h>

/*
** Message threading data.  There are a lot of message Ids being copied
** around.  Store a single occurence of each message ID, and reference-count
** it.
*/

class messageId;

class myMessageIds {

	std::map<std::string, size_t> msgids;
	/* All the message IDs in the system, keyed by message IDed, and
	** valued with the reference count.
	*/

public:
	myMessageIds();
	~myMessageIds();

	friend class messageId;
};

class messageId {
	myMessageIds *ids;
	std::map<std::string, size_t>::iterator ref;
public:
	messageId();
	messageId(myMessageIds &, std::string);
	~messageId();
	messageId(const messageId &);

	messageId &operator=(const messageId &);

	bool operator==(const messageId &a) const
	{ return (ids && a.ids && ref->first == a.ref->first); }

	bool operator<(const messageId &a) const
	{ return (ids && a.ids && ref->first < a.ref->first); }

	const char *c_str() { return ids ? ref->first.c_str():""; }
	operator std::string() const { return ids ? ref->first:
		std::string(""); }
};

////////////////////////////////////////////////////////////////////////////
//
// Watching followups to the following message IDs

class WatchInfo {
public:
	time_t expires; // When this watch record expires
	size_t depth;
	// How many followups to watch.  0-this is an unWatched message ID

	WatchInfo(time_t, size_t);
	~WatchInfo();
};

class Watch {

public:
	std::map<messageId, WatchInfo> watchList;

	Watch();
	~Watch();

	static unsigned defaultWatchDays;
	static unsigned defaultWatchDepth;

	Watch &operator()(messageId, time_t, size_t);

	bool watching(messageId, time_t &, size_t &) const;

	void unWatch(messageId);
};



#endif
