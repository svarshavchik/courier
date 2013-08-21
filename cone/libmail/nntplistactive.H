/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_nntplistactive_H
#define libmail_nntplistactive_H

#include "libmail_config.h"
#include <fstream>

#include "nntp.H"
#include "nntplogin.H"

#include <set>
#include <string>

LIBMAIL_START

// Issue a LIST ACTIVE, update newsrc accordingly

class mail::nntp::ListActiveTask : public mail::nntp::LoggedInTask {

	void (mail::nntp::ListActiveTask::*response_func)(const char *);

	std::string serverDate;

	std::set<std::string> newsgroupList;
	size_t count;

	callback::folderList *folderListCallback;

public:
	enum cmdType {
		LIST,
		NEWGROUPS
	};
private:
	cmdType cmd;
public:

	ListActiveTask(callback *callbackArg, nntp &myserverArg);
	// A regular, garden-variety LIST+resync newsrc
	~ListActiveTask();

	ListActiveTask(callback *callbackArg, nntp &myserverArg,
		       std::string sinceDate,
		       callback::folderList *);

	// Do a NEWGROUPS

	void loggedIn();
	void processLine(const char *message);

private:
	void processDateStatus(const char *);
	void processListStatus(const char *);
	void processSubscription(const char *);
};

LIBMAIL_END

#endif
