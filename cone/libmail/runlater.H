/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_runlater_H
#define libmail_runlater_H

#include "libmail_config.h"
#include "objectmonitor.H"

#include <queue>

/*
** Local mail drivers are immediate - they implement the request and invoke
** the callback function without delay.  Some requests iterate over an entire
** message set (such as mail::copyMessages).  Each callback function
** iterates to the next message.  With large message sets the stack could
** grow quite big.  The solution is the mail::runLater interface.  After
** calling RunLater(), the caller MUST immediately terminate.  The stack
** will unwind back to mail::account::process(), which then invokes the
** checkRunLater() method.
*/

LIBMAIL_START

class runLater;

class runLater : public mail::obj {
public:
	runLater();
	~runLater();

	static std::queue<mail::ptr<runLater> > runningLater;

	void RunLater();

	virtual void RunningLater()=0;

	static void checkRunLater(int &timeout);
};

LIBMAIL_END

#endif

