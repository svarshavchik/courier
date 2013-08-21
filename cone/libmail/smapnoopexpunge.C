/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "smap.H"
#include "smapnoopexpunge.H"
#include "imapfolder.H"
#include <sstream>

using namespace std;

const char *mail::smapNoopExpunge::getName()
{
	return cmd;
}

mail::smapNoopExpunge::smapNoopExpunge(const char *cmdArg,
				       mail::callback &callbackArg,
				       mail::imap &imapAccount)
	: cmd(cmdArg), uidSet( imapAccount, vector<size_t>()),
	  expungeSet(false), existsOrExpungeFlag(false)
{
	defaultCB= &callbackArg;
}

mail::smapNoopExpunge::smapNoopExpunge(const vector<size_t> &messageList,
				       mail::callback &callbackArg,
				       mail::imap &imapAccount)
	: cmd("EXPUNGE"), uidSet( imapAccount, messageList),
	  expungeSet(true), existsOrExpungeFlag(false)
{
	defaultCB= &callbackArg;
}

void mail::smapNoopExpunge::existsOrExpungeSeen()
{
	existsOrExpungeFlag=true;
}

mail::smapNoopExpunge::smapNoopExpunge(const char *cmdArg,
				       mail::imap &imapAccount)
	: cmd(cmdArg), uidSet(imapAccount, vector<size_t>()),
	  expungeSet(false), existsOrExpungeFlag(false)
{
	defaultCB=NULL;
}

mail::smapNoopExpunge::~smapNoopExpunge()
{
}

void mail::smapNoopExpunge::installed(imap &imapAccount)
{
	string imapCommand=cmd;

	if (expungeSet)
	{
		ostringstream o;

		if (!uidSet.getNextRange(imapAccount, o))
		{
			mail::callback *p=defaultCB;

			defaultCB=NULL;

			imapAccount.uninstallHandler(this);

			if (p)
				p->success("OK");
			return;
		}

		imapCommand += o.str();
	}
	imapAccount.imapcmd("", imapCommand + "\n");
}

bool mail::smapNoopExpunge::ok(std::string okmsg)
{
	if (expungeSet)
	{
		ostringstream o; // More?

		o << "EXPUNGE";

		if (uidSet.getNextRange(*myimap, o))
		{
			o << "\n";

			myimap->imapcmd("", o.str());
			doDestroy=false;
			return true;
		}
	}

	// If a NOOP/EXPUNGE found new messages, we don't want to invoke the
	// callback function right away, because we must wait for
	// smapNEWMAIL to finish populating the message index.
	// Simply do this by requeueing another NOOP request at the end of
	// the task queue.
	//
	if (existsOrExpungeFlag && defaultCB)
	{
		myimap->installForegroundTask(new smapNoopExpunge("NOOP",
								  *defaultCB,
								  *myimap)
					      );
		defaultCB=NULL;
	}

	return smapHandler::ok(okmsg);
}

bool mail::smapNoopExpunge::fail(std::string okmsg)
{

	if (existsOrExpungeFlag)
		myimap->installForegroundTask(new smapNoopExpunge("NOOP",
								  *myimap));

	return smapHandler::fail(okmsg);
}
