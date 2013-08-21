/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "nntplogin.H"

using namespace std;

mail::nntp::LoggedInTask::LoggedInTask(mail::callback *callbackArg,
				       mail::nntp &myserverArg)
	: Task(callbackArg, myserverArg), loggingIn(true)
{
}

mail::nntp::LoggedInTask::~LoggedInTask()
{
}

void mail::nntp::LoggedInTask::installedTask()
{
	if (myserver->getfd() >= 0) // Already logged in
	{
		loginCompleted2();
		return;
	}

	myserver->serverGroup="";
	responseFunc= &mail::nntp::LoggedInTask::doGreeting;
	myserver->nntpLoginInfo=myserver->savedLoginInfo;


	if (myserver->nntpLoginInfo.uid.size() > 0 &&
	    myserver->nntpLoginInfo.pwd.size() == 0 &&
	    myserver->nntpLoginInfo.loginCallbackFunc)
	{
		currentCallback=myserver->nntpLoginInfo.loginCallbackFunc;
		currentCallback->target=this;
		defaultTimeout=300;
		resetTimeout();
		myserver->nntpLoginInfo.loginCallbackFunc->getPassword();
		return;
	}
	gotPassword(myserver->nntpLoginInfo.pwd);
}

void mail::nntp::LoggedInTask::gotPassword(std::string pwd)
{
	defaultTimeout=myserver->timeoutSetting;
	resetTimeout();

	myserver->nntpLoginInfo.pwd=
		myserver->savedLoginInfo.pwd=pwd;

	myserver->savedLoginInfo.loginCallbackFunc=NULL;

	if (myserver->nntpLoginInfo.pwd.find('\n') !=
	    std::string::npos ||
	    myserver->nntpLoginInfo.pwd.find('\r') !=
	    std::string::npos ||
	    myserver->nntpLoginInfo.uid.find('\n') !=
	    std::string::npos ||
	    myserver->nntpLoginInfo.uid.find('\r') !=
	    std::string::npos)
	{
		fail("Invalid characters in userid or password");
		return;
	}

        string errmsg=myserver->
		socketConnect(myserver->nntpLoginInfo, "nntp", "nntps");

        if (errmsg.size() > 0)
        {
		fail(errmsg.c_str());
                return;
        }
}

// Make sure the socket gets closed, if we fail.

void mail::nntp::LoggedInTask::fail(std::string msg)
{
	if (loggingIn)
		myserver->socketDisconnect();
	Task::fail(msg);
}

void mail::nntp::LoggedInTask::doFwdResponse(const char *message)
{
	processLine(message);
}

void mail::nntp::LoggedInTask::serverResponse(const char *message)
{
	(this->*responseFunc)(message);
}

void mail::nntp::LoggedInTask::doGreeting(const char *message)
{
	if (*message != '2')
	{
		myserver->socketDisconnect();
		fail(message);
		return;
	}

	responseFunc= &mail::nntp::LoggedInTask::doModeReader1;

	myserver->socketWrite("MODE READER\r\n");
}

void mail::nntp::LoggedInTask::doModeReader1(const char *message)
{
	if (myserver->nntpLoginInfo.uid.size() > 0 &&
	    myserver->nntpLoginInfo.pwd.size() > 0)
	{
		authinfoUser();
		return;
	}

	loginCompleted();
}

// Received uid/pwd from the app.
void mail::nntp::LoggedInTask::loginInfoCallback(std::string cb)
{
	gotPassword(cb);
}

// App doesn't want to supply login/pwd

void mail::nntp::LoggedInTask::loginInfoCallbackCancel()
{
	fail("LOGIN cancelled.");
}

void mail::nntp::LoggedInTask::authinfoUser()
{
	defaultTimeout=myserver->timeoutSetting;
	resetTimeout();

	responseFunc= &mail::nntp::LoggedInTask::doAuthUser;
	myserver->socketWrite("AUTHINFO USER " +
			      myserver->nntpLoginInfo.uid + "\r\n");
}

void mail::nntp::LoggedInTask::doAuthUser(const char *msg)
{
	if (atoi(msg) == 381)
	{
		authinfoPwd();
		return;
	}

	if (msg[0] != '2')
	{
		fail(msg);
		return;
	}
	authCompleted();
}

void mail::nntp::LoggedInTask::authinfoPwd()
{
	defaultTimeout=myserver->timeoutSetting;
	resetTimeout();

	responseFunc= &mail::nntp::LoggedInTask::doAuthPwd;
	myserver->socketWrite("AUTHINFO PASS " +
			      myserver->nntpLoginInfo.pwd + "\r\n");
}


void mail::nntp::LoggedInTask::doAuthPwd(const char *resp)
{
	if (resp[0] != '2')
	{
		fail(resp);
		return;
	}
	authCompleted();
}

void mail::nntp::LoggedInTask::authCompleted()
{
	responseFunc= &mail::nntp::LoggedInTask::doModeReader2;

	myserver->socketWrite("MODE READER\r\n");
}

void mail::nntp::LoggedInTask::doModeReader2(const char *message)
{
	loginCompleted();
}

// Some additional cleanup before calling superclass's loggedIn()

void mail::nntp::LoggedInTask::loginCompleted()
{
	myserver->serverGroup="";
	loginCompleted2();
}

void mail::nntp::LoggedInTask::loginCompleted2()
{
	responseFunc= &mail::nntp::LoggedInTask::doFwdResponse;
	myserver->savedLoginInfo.loginCallbackFunc=NULL;
	loggingIn=false;
	loggedIn();
}
