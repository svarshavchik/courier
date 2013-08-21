/*
** Copyright 2002-2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "misc.H"
#include "imapstatushandler.H"
#include "imapfolders.H"
#include "smapstatus.H"
#include <stdlib.h>
#include <string.h>
#include <sstream>

using namespace std;

// Invoked from mail::imapFolder::readFolderInfo()

void mail::imap::folderStatus(string path,
			      mail::callback::folderInfo &callback1,
			      mail::callback &callback2)
{
	if (!ready(callback2))
		return;

	if (smap)
	{
		mail::smapSTATUS *s=
			new mail::smapSTATUS(path, callback1, callback2);

		installForegroundTask(s);
	}
	else
	{
		mail::imapStatusHandler *s=
			new mail::imapStatusHandler(callback1, callback2,
						    path);
		installForegroundTask(s);
	}
}

mail::imapStatusHandler
::imapStatusHandler(mail::callback::folderInfo &myCallback,
			  mail::callback &myCallback2,
			  string myHier)
	: callback1(myCallback), callback2(myCallback2), hier(myHier)
{
}

mail::imapStatusHandler::~imapStatusHandler()
{
}

const char *mail::imapStatusHandler::getName()
{
	return "STATUS";
}


void mail::imapStatusHandler::timedOut(const char *errmsg)
{
	callback2.fail(errmsg ? errmsg:"Server timed out.");
}

void mail::imapStatusHandler::installed(mail::imap &imapAccount)
{
	imapAccount.imapcmd("STATUS", "STATUS " + imapAccount.quoteSimple(hier)
		     + " (MESSAGES UNSEEN)\r\n");
}

LIBMAIL_START

class imapSTATUS : public imapHandlerStructured {

	mail::callback::folderInfo &callback1;
	string expectedHier;

	string attrName;

	void (imapSTATUS::*next_func)(imap &, Token);

public:
	imapSTATUS(string hier, mail::callback::folderInfo &callback);
	~imapSTATUS();

	void installed(imap &imapAccount);
private:
	const char *getName();
	void timedOut(const char *errmsg);

	void process(imap &imapAccount, Token t);

	void get_hier_name(imap &imapAccount, Token t);
	void get_attr_list(imap &imapAccount, Token t);
	void get_attr_name(imap &imapAccount, Token t);
	void get_attr_value(imap &imapAccount, Token t);
};

LIBMAIL_END

bool mail::imapStatusHandler::untaggedMessage(mail::imap &imapAccount, string name)
{
	if (name != "STATUS")
		return false;

	imapAccount.installBackgroundTask( new mail::imapSTATUS(hier, callback1));
	return true;
}

bool mail::imapStatusHandler::taggedMessage(mail::imap &imapAccount, string name,
					  string message,
					  bool okfail,
					  string errmsg)
{
	if (name != "STATUS")
		return false;

	if (okfail)
		callback2.success(errmsg);
	else
		callback2.fail(errmsg);
	imapAccount.uninstallHandler(this);
	return true;
}

///////////////////////////////////////////////////////////////////////////
//
// * STATUS parsing

mail::imapSTATUS::imapSTATUS(string hierArg,
				   mail::callback::folderInfo &callbackArg)
	: callback1(callbackArg), expectedHier(hierArg),
	  next_func( &mail::imapSTATUS::get_hier_name)
{
	callback1.messageCount=0;
	callback1.unreadCount=0;
}

mail::imapSTATUS::~imapSTATUS()
{
}

void mail::imapSTATUS::installed(mail::imap &imapAccount)
{
}

const char *mail::imapSTATUS::getName()
{
	return "* STATUS";
}

void mail::imapSTATUS::timedOut(const char *errmsg)
{
}

void mail::imapSTATUS::process(mail::imap &imapAccount, Token t)
{
	(this->*next_func)(imapAccount, t);
}

void mail::imapSTATUS::get_hier_name(mail::imap &imapAccount, Token t)
{
	if (t == ATOM || t == STRING)
	{
		next_func= &mail::imapSTATUS::get_attr_list;

		if (t.text != expectedHier)
			error(imapAccount);	// Sanity check
	}
	else
		error(imapAccount);
}

void mail::imapSTATUS::get_attr_list(mail::imap &imapAccount, Token t)
{
	if (t == NIL)
	{
		done();
		callback1.success();
		return;
	}

	if (t != '(')
	{
		error(imapAccount);
		return;
	}

	next_func= &mail::imapSTATUS::get_attr_name;
}

void mail::imapSTATUS::get_attr_name(mail::imap &imapAccount, Token t)
{
	if (t == ')')
	{
		done();
		callback1.success();
		return;
	}

	if (t != ATOM && t != STRING)
	{
		error(imapAccount);
		return;
	}

	attrName=t.text;
	upper(attrName);
	next_func= &mail::imapSTATUS::get_attr_value;
}

void mail::imapSTATUS::get_attr_value(mail::imap &imapAccount, Token t)
{
	if (t != ATOM && t != STRING)
	{
		error(imapAccount);
		return;
	}

	size_t num=0;

	istringstream(t.text.c_str()) >> num;

	if (attrName == "MESSAGES")
		callback1.messageCount=num;
	else if (attrName == "UNSEEN")
		callback1.unreadCount=num;

	next_func= &mail::imapSTATUS::get_attr_name;
}
