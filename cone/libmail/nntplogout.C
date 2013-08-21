/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "nntplogout.H"

mail::nntp::LogoutTask::LogoutTask(callback *callbackArg, nntp &myserverArg,
				   bool inactivityTimeoutArg)
	: Task(callbackArg, myserverArg),
	  inactivityTimeout(inactivityTimeoutArg),
	  goodDisconnect(false)
{
}

mail::nntp::LogoutTask::~LogoutTask()
{
}

void mail::nntp::LogoutTask::installedTask()
{
	if (myserver->getfd() < 0)
	{
		success("Ok.");
		return;
	}

	myserver->socketWrite("QUIT\r\n");
}

void mail::nntp::LogoutTask::done()
{
	if (inactivityTimeout)
	{
		Task::done();
		return;
	}

	callback::disconnect *d=myserver->disconnectCallback;

	myserver->disconnectCallback=NULL;

	Task::done();
	if (d)
		d->disconnected("");
}

void mail::nntp::LogoutTask::serverResponse(const char *msg)
{
	goodDisconnect=true;

	if (!myserver->socketEndEncryption())
	{
		myserver->socketDisconnect();
		success("Ok");
	}
}

void mail::nntp::LogoutTask::disconnected(const char *reason)
{
	if (goodDisconnect)
		reason="";

	if (inactivityTimeout)
	{
		success("Ok.");
		return;
	}

	callback::disconnect *d=myserver->disconnectCallback;

	myserver->disconnectCallback=NULL;

	success("Ok.");
	if (d)
		d->disconnected(reason);
}

void mail::nntp::LogoutTask::emptyQueue()
{
}
