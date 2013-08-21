/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "nntplogin2.H"
#include <cstring>

#include <errno.h>

using namespace std;

//
// Initial login
//

mail::nntp::LoginTask::LoginTask(callback *callbackArg, nntp &myserverArg)
	: LoggedInTask(callbackArg, myserverArg)
{
}

mail::nntp::LoginTask::~LoginTask()
{
}

void mail::nntp::LoginTask::loggedIn()
{
	ifstream i(myserver->newsrcFilename.c_str());

	if (i.is_open())
	{
		processListStatus("500 Dummy.");
		return;
	}

	newNewsrcFilename=myserver->newsrcFilename + ".tmp";

	newNewsrc.open(newNewsrcFilename.c_str());

	if (!newNewsrc.is_open())
	{
		fail(strerror(errno));
		return;
	}

	response_func= &mail::nntp::LoginTask::processListStatus;
	myserver->socketWrite("LIST SUBSCRIPTIONS\r\n");
}

void mail::nntp::LoginTask::processLine(const char *message)
{
	(this->*response_func)(message);
}

void mail::nntp::LoginTask::processListStatus(const char *message)
{
	if (message[0] != '2')
	{
		newNewsrc.close();
		if (newNewsrcFilename.size() > 0)
			unlink(newNewsrcFilename.c_str());

		if (atoi(message) == 480 ||
		    atoi(message) == 381)
			fail(message);
		else
			success("Connected to server.");
		return;
	}

	response_func= &mail::nntp::LoginTask::processSubscription;
}

void mail::nntp::LoginTask::processSubscription(const char *message)
{
	if (strcmp(message, "."))
	{
		newNewsrc << message << ":" << endl;
		return;
	}

	newNewsrc.close();
	if (newNewsrc.bad() || newNewsrc.fail() ||
	    rename(newNewsrcFilename.c_str(),
		   myserver->newsrcFilename.c_str()) < 0)
	{
		string msg=strerror(errno);
		fail(msg);
		return;
	}

	success("Ok.");
}

void mail::nntp::LoginTask::fail(string msg)
{
	// Since we never officially logged in, we can't officially disconnect.

	myserver->disconnectCallback=NULL;

	LoggedInTask::fail(msg);
}
