/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "smap.H"
#include "imapfolder.H"
#include "smapopen.H"
#include "smapnewmail.H"
#include <string.h>
#include <sstream>

using namespace std;

const char *mail::smapNEWMAIL::getName()
{
	return "NEWMAIL";
}

mail::smapNEWMAIL::smapNEWMAIL(mail::callback *callbackArg,
			       bool isOpenArg)
	: isOpen(isOpenArg), expectedFetchCount(0), fetchCount(0),
	  noopSent(false)
{
	defaultCB= callbackArg;
}

mail::smapNEWMAIL::~smapNEWMAIL()
{
}

void mail::smapNEWMAIL::installed(imap &imapAccount)
{
	imapFOLDERinfo *p=myimap->currentFolder;

	if (p)
	{
		size_t n=p->index.size();

		if (p->exists < n)
		{
			ostringstream o;

			o << "FETCH " << p->exists+1 << "-" << n
			  << " FLAGS UID\n" << flush;

			expectedFetchCount=(n - p->exists)*2;

			imapAccount.imapcmd("", o.str());
			return;
		}
	}

	smapHandler::ok("OK");
	imapAccount.uninstallHandler(this);
}

//
// Report on our progress
//

void mail::smapNEWMAIL::fetchedIndexInfo()
{
	++fetchCount;

	if (fetchCount <= expectedFetchCount && defaultCB)
		defaultCB->reportProgress(0, 0,
					  fetchCount/2,
					  expectedFetchCount/2);
}

bool mail::smapNEWMAIL::ok(std::string str)
{
	if (!noopSent)
	{
		imapFOLDERinfo *p=myimap->currentFolder;

		while (p && p->exists < p->index.size() &&
		       p->index[p->exists].uid.size() > 0)
			++p->exists;

		noopSent=true;
		doDestroy=false;
		myimap->imapcmd("", "NOOP\n"); // Get a snapshot
		return true;
	}

	if (myimap->currentFolder)
	{
		if (isOpen)
			myimap->currentFolder->opened();
		else
			myimap->currentFolder->folderCallback.newMessages();
	}
	return smapHandler::ok(str);
}

bool mail::smapNEWMAIL::fail(std::string str)
{
	if (myimap->currentFolder)
	{
		if (isOpen)
			myimap->currentFolder->opened();
		else
			myimap->currentFolder->folderCallback.newMessages();
	}

	return smapHandler::fail(str);
}
