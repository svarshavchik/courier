/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef libmail_smapidle_H
#define libmail_smapidle_H

#include "libmail_config.h"

#include "mail.H"
#include "smap.H"

#include <time.h>

LIBMAIL_START

class smapIdleHandler : public smapHandler {

	bool idleOnOff;

	bool idling;
	bool shouldTerminate;
	bool terminating;

	bool waiting;
	struct timeval waitingUntil;

	void terminateIdle(imap &);

	const char *getName();
	void timedOut(const char *errmsg);


	bool getTimeout(imap &, int &ioTimeout);

public:
	smapIdleHandler(bool idleOnOffArg, mail::callback *callbackArg);

	~smapIdleHandler();

	void installed(imap &);

	virtual bool ok(std::string);

	void anotherHandlerInstalled(imap &imapAccount);
};


LIBMAIL_END
#endif
