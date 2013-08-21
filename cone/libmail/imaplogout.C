/*
** Copyright 2002-2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "imap.H"
#include "misc.H"
#include "imapfolder.H"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>

#include <iostream>

using namespace std;

LIBMAIL_START

/////////////////////////////////////////////////////////////////////////
//
// The LOGOUT command

class imapLogoutHandler : public imapHandler {

	bool eatEverything;

private:
	mail::callback &callback;
	bool orderlyShutdown;

	void dologout(imap &imapAccount, mail::callback &callback);

	const char *getName();
	void timedOut(const char *errmsg);
	void installed(imap &imapAccount);

public:
	imapLogoutHandler(mail::callback &myCallback);
	~imapLogoutHandler();
	int process(imap &imapAccount, string &buffer);
} ;
LIBMAIL_END

//////////////////////////////////////////////////////////////////////////////
//
// Logging out.

void mail::imap::logout(mail::callback &callback)
{
	if (!socketConnected())
        {
                callback.success("Already logged out.");
                return;
        }

	orderlyShutdown=true;

	if (currentFolder)
		currentFolder->closeInProgress=true;

	installForegroundTask(new mail::imapLogoutHandler(callback));
}

int mail::imapLogoutHandler::process(mail::imap &imapAccount, string &buffer)
{
	if (eatEverything)
		return buffer.size();

	size_t p=buffer.find('\n');

	if (p == std::string::npos)
	{
		if (buffer.size() > 16000)
			return buffer.size() - 16000;
		// SANITY CHECK - DON'T LET HOSTILE SERVER DOS us
		return (0);
	}

	string buffer_cpy=buffer;

	buffer_cpy.erase(p);
	++p;

	string::iterator b=buffer_cpy.begin();
	string::iterator e=buffer_cpy.end();
	string word=imapAccount.get_cmdreply(&b, &e);

	upper(word);

	if (word == (imapAccount.smap ? "+OK":"LOGOUT"))
		dologout(imapAccount, callback);

	return p;
}

const char *mail::imapLogoutHandler::getName()
{
	return "LOGOUT";
}

void mail::imapLogoutHandler::timedOut(const char *errmsg)
{
	callback.success(errmsg ? errmsg:"Timed out.");
}

void mail::imapLogoutHandler::installed(mail::imap &imapAccount)
{
	if (imapAccount.smap)
		imapAccount.imapcmd("", "LOGOUT\n");
	else
		imapAccount.imapcmd("LOGOUT", "LOGOUT\r\n");
}

mail::imapLogoutHandler::imapLogoutHandler(mail::callback &myCallback)
		: eatEverything(false),
		  callback(myCallback), orderlyShutdown(false)
{
}

mail::imapLogoutHandler::~imapLogoutHandler()
{
	if (!orderlyShutdown)
		callback.success("Logged out.");
}

#include <fstream>

void mail::imapLogoutHandler::dologout(mail::imap &imapAccount,
				       mail::callback &c)
{
	if (!imapAccount.socketEndEncryption())
	{
		orderlyShutdown=true;
		imapAccount.uninstallHandler(this);
		c.success("Logged out.");
	}
	else
	{
		eatEverything=true;
	}
}
