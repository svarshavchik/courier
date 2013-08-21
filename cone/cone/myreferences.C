/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "config.h"
#include "myreferences.H"
#include "myserver.H"
#include "mymessage.H"
#include "cursesmessage.H"
#include "myservercallback.H"
#include "gettext.H"
#include "curses/cursesstatusbar.H"
#include "rfc822/rfc822.h"
#include "rfc2045/rfc2045.h"
#include "libmail/rfc2047decode.H"
#include "unicode/unicode.h"
#include "libmail/envelope.H"

using namespace std;

myMessageIds::myMessageIds()
{
}

myMessageIds::~myMessageIds()
{
	if (msgids.size() > 0)
		throw "Internal error - dangling message ID pool references.";
}

messageId::messageId() : ids(0)
{
}

messageId::messageId(myMessageIds &idRef, std::string s)
	: ids(&idRef), ref(ids->msgids.find(s))
{
	if (ref == ids->msgids.end())
		ref=ids->msgids.insert(make_pair(s, (size_t)0)).first;

	++ref->second;
}

messageId::~messageId()
{
	if (ids && --ref->second == 0)
		ids->msgids.erase(ref);
}

messageId::messageId(const messageId &a) : ids(a.ids), ref(a.ref)
{
	if (ids)
		++ref->second;
}

messageId &messageId::operator=(const messageId &a)
{
	if (a.ids)
		++a.ref->second;

	if (ids && --ref->second == 0)
		ids->msgids.erase(ref);

	ids=a.ids;
	ref=a.ref;

	return *this;
}

//////////////////////////////////////////////////////////////////////////////
//
// Watching stuff

unsigned Watch::defaultWatchDays=14;
unsigned Watch::defaultWatchDepth=5;

WatchInfo::WatchInfo(time_t t, size_t s) : expires(t), depth(s)
{
}

WatchInfo::~WatchInfo()
{
}

Watch::Watch()
{
}

Watch::~Watch()
{
}

Watch &Watch::operator()(messageId id, time_t t, size_t s)
{
	if (*id.c_str() == 0)
		return *this; // Don't watch a blank msgid!

	map<messageId, WatchInfo>::iterator p=watchList.find(id);

	if (p == watchList.end())
		p=watchList.insert(make_pair(id, WatchInfo(t, s))).first;

	p->second=WatchInfo(t, s);

	return *this;
}

void Watch::unWatch(messageId id)
{
	map<messageId, WatchInfo>::iterator p=watchList.find(id);

	if (p != watchList.end())
		p->second.depth=0;
}


bool Watch::watching(messageId id, time_t &expires, size_t &depth) const
{
	map<messageId, WatchInfo>::const_iterator p=watchList.find(id);

	if (p != watchList.end())
	{
		expires=p->second.expires;
		depth=p->second.depth;
		return true;
	}

	return false;
}
