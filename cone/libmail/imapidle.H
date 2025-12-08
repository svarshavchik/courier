/*
** Copyright 2003, S. Varshavchik.
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

	const char *getName() override;
	void timedOut(const char *errmsg) override;


	bool getTimeout(imap &, int &) override;

public:
	imapIdleHandler(bool idleOnOffArg, mail::callback *callbackArg);
	~imapIdleHandler();

	void installed(imap &) override;

	bool untaggedMessage(imap &imapAccount, std::string name) override;

	bool taggedMessage(imap &imapAccount, std::string name,
			   std::string message,
			   bool okfail, std::string errmsg) override;

	bool continuationRequest(imap &imapAccount,
				 std::string request) override;

	void anotherHandlerInstalled(imap &imapAccount) override;
};


LIBMAIL_END
#endif
