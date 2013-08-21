/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_nntplogin2_H
#define libmail_nntplogin2_H

#include "libmail_config.h"
#include <fstream>

#include "nntplogin.H"

LIBMAIL_START

//
// The formal news server login
//

class mail::nntp::LoginTask : public mail::nntp::LoggedInTask {

	void (mail::nntp::LoginTask::*response_func)(const char *);

	std::ofstream newNewsrc;
	std::string newNewsrcFilename;

public:
	LoginTask(callback *callbackArg, nntp &myserverArg);
	~LoginTask();

	void loggedIn();
	void processLine(const char *message);
	void fail(std::string);

private:
	void processListStatus(const char *);
	void processSubscription(const char *);
};

LIBMAIL_END

#endif
