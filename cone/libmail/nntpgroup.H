/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_nntpgroup_H
#define libmail_nntpgroup_H

#include "libmail_config.h"

#include "nntp.H"
#include "nntplogin.H"

#include <string>

LIBMAIL_START

//
// Open a specific group


class mail::nntp::GroupTask : public mail::nntp::LoggedInTask {

	void (mail::nntp::GroupTask::*response_func)(const char *);

protected:
	std::string groupName;

public:

	GroupTask(callback *callbackArg, nntp &myserverArg,
		  std::string groupNameArg);
	~GroupTask();

	// Implements:

	void loggedIn();
	void processLine(const char *message);

	// Subclasses must defined:

	virtual void selectedGroup(msgnum_t estimatedCount,
				   msgnum_t loArticleCount,
				   msgnum_t hiArticleCount)=0;
	virtual void processGroup(const char *)=0;

private:
	void processGroupStatus(const char *);
	void processOtherStatus(const char *);

	void selectedCurrentGroup();
};

LIBMAIL_END

#endif
