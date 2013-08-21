/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef libmail_imapidle_H
#define libmail_imapidle_H

#include "libmail_config.h"

#include "mail.H"
#include "imap.H"

#include <time.h>

LIBMAIL_START

class imapIdleHandler : public imapCommandHandler {

	bool idleOnOff;

	bool idling;
	bool shouldTerminate;
	bool terminating;

	bool waiting;
	struct timeval waitingUntil;

	mail::callback *callback;

	void terminateIdle(imap &);

	const char *getName();
	void timedOut(const char *errmsg);


	bool getTimeout(imap &, int &);

public:
	imapIdleHandler(bool idleOnOffArg, mail::callback *callbackArg);
	~imapIdleHandler();

	void installed(imap &);

	bool untaggedMessage(imap &imapAccount, std::string name);

	bool taggedMessage(imap &imapAccount, std::string name,
			   std::string message,
			   bool okfail, std::string errmsg);

	bool continuationRequest(imap &imapAccount,
				 std::string request);

	void anotherHandlerInstalled(imap &imapAccount);
};


LIBMAIL_END
#endif
