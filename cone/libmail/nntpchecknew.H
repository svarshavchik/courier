/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_nntpchecknew_H
#define libmail_nntpchecknew_H

#include "libmail_config.h"

#include "nntp.H"
#include "nntplogin.H"

#include <string>
#include <vector>

LIBMAIL_START

//
// Check for new messages in a group

class mail::nntp::CheckNewTask : public mail::nntp::LoggedInTask {

	void (mail::nntp::CheckNewTask::*response_func)(const char *);

	std::string groupName;

	std::vector<msgnum_t> newMsgList;

	msgnum_t loWatermark, hiWatermark;

	msgnum_t firstNewMsg;

public:

	CheckNewTask(callback *callbackArg, nntp &myserverArg,
		  std::string groupNameArg);
	~CheckNewTask();

	void loggedIn();
	void processLine(const char *message);

private:
	void processGroupStatus(const char *);
	void processXhdrStatus(const char *);
	void processXhdrList(const char *);

	void checkPurged();

};

LIBMAIL_END

#endif
