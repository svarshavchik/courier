/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "imap.H"
#include "imaphandler.H"
#include "imapidle.H"

using namespace std;

mail::imapIdleHandler::imapIdleHandler(bool idleOnOffArg,
				       mail::callback *callbackArg)
	: idleOnOff(idleOnOffArg), idling(false),
	  shouldTerminate(false), terminating(false),
	  callback(callbackArg)
{
}

const char *mail::imapIdleHandler::getName()
{
	return "IDLE";
}

bool mail::imapIdleHandler::getTimeout(imap &imapAccount,
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

			imapAccount.imapcmd("IDLE", "IDLE\r\n");
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

	ioTimeout = 15 * 60 * 1000;
	return false;
}

void mail::imapIdleHandler::timedOut(const char *errmsg)
{
	mail::callback *c=callback;

	callback=NULL;

	if (c)
		callbackTimedOut(*c, errmsg);
}

mail::imapIdleHandler::~imapIdleHandler()
{
	mail::callback *c=callback;

	callback=NULL;

	if (c)
		c->success("OK");
}

bool mail::imapIdleHandler::untaggedMessage(imap &imapAccount,
					    std::string name)
{
	return false;
}

void mail::imapIdleHandler::installed(imap &imapAccount)
{
	if (!idleOnOff || !imapAccount.wantIdleMode
	    || shouldTerminate || !imapAccount.task_queue.empty())
	{
		imapAccount.uninstallHandler(this);
		return;
	}

	waiting=true;

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
}


bool mail::imapIdleHandler::taggedMessage(imap &imapAccount, std::string name,
					  std::string message,
					  bool okfail, std::string errmsg)
{
	mail::callback *c=callback;

	callback=NULL;

	imapAccount.uninstallHandler(this);

	if (imapAccount.wantIdleMode)
	{
		// Reinstall, when we're done

		imapAccount.installForegroundTask(new imapIdleHandler(true,
								      NULL));
		c=NULL;
	}

	if (c)
	{
		if (okfail)
			c->success(errmsg);
		else
			c->fail(errmsg);
	}
	return true;
}
		
bool mail::imapIdleHandler::continuationRequest(imap &imapAccount,
					    std::string request)
{
	idling=true;
	if (shouldTerminate)
		terminateIdle(imapAccount);

	mail::callback *c=callback;

	callback=NULL;

	if (c)
		c->success("Monitoring folder for changes...");

	return true;
}

void mail::imapIdleHandler::anotherHandlerInstalled(imap &imapAccount)
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

void mail::imapIdleHandler::terminateIdle(imap &imapAccount)
{
	if (terminating)
		return;
	terminating=true;

	mail::callback *c=callback;

	callback=NULL;

	if (c)
		c->success("OK");

	imapAccount.imapcmd("", "DONE\r\n");
}

