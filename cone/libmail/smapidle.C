/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "smap.H"
#include "smapidle.H"
#include <iostream>

using namespace std;

mail::smapIdleHandler::smapIdleHandler(bool idleOnOffArg,
				       mail::callback *callbackArg)
	: idleOnOff(idleOnOffArg), idling(false),
	  shouldTerminate(false), terminating(false)
{
	defaultCB=callbackArg;
}

const char *mail::smapIdleHandler::getName()
{
	return "IDLE";
}

bool mail::smapIdleHandler::getTimeout(imap &imapAccount,
				       int &ioTimeout)
{
	if (waiting)
	{
		struct timeval tv;

		gettimeofday(&tv, NULL);

		if (tv.tv_sec > waitingUntil.tv_sec ||
		    (tv.tv_sec == waitingUntil.tv_sec &&
		     tv.tv_usec >= waitingUntil.tv_usec))
		{
			waiting=false;

			imapAccount.imapcmd("", "IDLE\n");
			ioTimeout=0;
			return false;
		}
		else
		{
			struct timeval t=waitingUntil;

			t.tv_usec -= tv.tv_usec;

			if (t.tv_usec < 0)
			{
				t.tv_usec += 1000000;
				--t.tv_sec;
			}
			t.tv_sec -= tv.tv_sec;

			ioTimeout=t.tv_sec * 1000 + t.tv_usec / 1000;

			if (ioTimeout == 0)
				ioTimeout=100;
			return false;
		}
	}

	ioTimeout= 15 * 60 * 1000;
	return false;
}

void mail::smapIdleHandler::timedOut(const char *errmsg)
{
	mail::callback *c=defaultCB;

	defaultCB=NULL;

	if (c)
		callbackTimedOut(*c, errmsg);
}

mail::smapIdleHandler::~smapIdleHandler()
{
	mail::callback *c=defaultCB;

	defaultCB=NULL;

	if (c)
		c->success("OK");
}

void mail::smapIdleHandler::installed(imap &imapAccount)
{
	if (!idleOnOff || !imapAccount.wantIdleMode
	    || shouldTerminate || !imapAccount.task_queue.empty())
	{
		imapAccount.uninstallHandler(this);
		return;
	}

	// Wait 1/10th of a second before issuing an IDLE.  When we're in
	// IDLE mode, and a new task is started, we terminate and a requeue
	// ourselves after the pending command.  We don't want to immediately
	// reenter the IDLE mode because if the first task queues up a second
	// task we would have to immediately get out of IDLE mode right away,
	// so wait 1/10th of a second to see if anything is up.

	gettimeofday(&waitingUntil, NULL);
	if ((waitingUntil.tv_usec += 100000) > 1000000)
	{
		waitingUntil.tv_usec %= 1000000;
		++waitingUntil.tv_sec;
	}
	waiting=true;
}

bool mail::smapIdleHandler::ok(std::string msg)
{
	if (!idling) // Response to the IDLE command
	{
		idling=true;
		if (shouldTerminate)
			terminateIdle(*myimap);

		mail::callback *c=defaultCB;

		defaultCB=NULL;

		if (c)
			c->success("Monitoring folder for changes...");

		doDestroy=false;
		return true;
	}

	// Response to the RESUME command

	mail::callback *c=defaultCB;

	defaultCB=NULL;

	if (myimap->wantIdleMode)
	{
		// Reinstall, when we're done

		myimap->installForegroundTask(new smapIdleHandler(true,
								  NULL));
		c=NULL;
	}

	if (c)
		c->success(msg);

	return true;
}
		
void mail::smapIdleHandler::anotherHandlerInstalled(imap &imapAccount)
{
	if (waiting)
	{
		imapAccount.uninstallHandler(this);
		return;
	}

	shouldTerminate=true;
	if (idling)
		terminateIdle(imapAccount);
}

void mail::smapIdleHandler::terminateIdle(imap &imapAccount)
{
	if (terminating)
		return;

	terminating=true;

	mail::callback *c=defaultCB;

	defaultCB=NULL;

	if (c)
		c->success("OK");

	imapAccount.imapcmd("", "RESUME\n");
}

