/*
** Copyright 2003-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "config.h"
#include "myserverlogincallback.H"
#include "myservercallback.H"
#include "myserverpromptinfo.H"
#include "curses/mycurses.H"
#include "gettext.H"

using namespace std;

myServerLoginCallback::myServerLoginCallback()
{
	reset();
}

myServerLoginCallback::~myServerLoginCallback()
{
}

void myServerLoginCallback::reset()
{
	isPasswordPrompt=false;
	myCallback=NULL;
}

void myServerLoginCallback::loginPrompt(callbackType cbType, 
					string prompt)
{
	if (myCallback)
		myCallback->interrupted=true;

	isPasswordPrompt= cbType == PASSWORD;
}

//
// Default callback prompt
//

void myServerLoginCallback::promptPassword(string serverName,
					   string &password)
{
	string promptStr=
		Gettext( isPasswordPrompt
			 ?_("Password for %1% (CTRL-C - cancel): ")
			 :_("Login for %1% (CTRL-C - cancel): ")
			 ) << serverName;

	myServer::promptInfo response=myServer::promptInfo(promptStr);

	if (isPasswordPrompt)
		response.password();

	response=myServer::prompt(response);

	if (response.aborted())
	{
		myCallback->interrupted=false;
		callbackCancel();
		return;
	}

	if (isPasswordPrompt)
		password=response;
	if (myCallback)
		myCallback->interrupted=false;

	callback(response);
}
