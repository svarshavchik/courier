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

	void done() override;
	void installedTask() override;
	void serverResponse(const char *) override;

	void disconnected(const char *reason) override;

	void emptyQueue() override; // Override it - do not set inactivity timeout

};

LIBMAIL_END

#endif
