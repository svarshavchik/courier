/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_nntplogout_H
#define libmail_nntplogout_H

#include "libmail_config.h"

#include "nntp.H"

LIBMAIL_START

//
// Formally log out.
//

class mail::nntp::LogoutTask : public mail::nntp::Task {

	bool inactivityTimeout;
	bool goodDisconnect;
public:
	LogoutTask(callback *callbackArg, nntp &myserverArg,
		   bool inactivityTimeoutArg);
	~LogoutTask();

	void done();
	void installedTask();
	void serverResponse(const char *);

	void disconnected(const char *reason);

	void emptyQueue(); // Override it - do not set inactivity timeout

};

LIBMAIL_END

#endif
