/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "smapstore.H"
#include <sstream>

using namespace std;

const char *mail::smapSTORE::getName()
{
	return "STORE";
}

mail::smapSTORE::smapSTORE(size_t messageNumArg,
			   string propsArg,
			   imap &imapAccount,
			   mail::callback &callback)
	: uidSet(imapAccount, vector<size_t>(&messageNumArg,
					     &messageNumArg+1)),
	  props(propsArg)
{
	defaultCB= &callback;
}

mail::smapSTORE::smapSTORE(const vector<size_t> &messages,
			   string propsArg,
			   imap &imapAccount,
			   mail::callback &callback)
	: uidSet(imapAccount, messages), props(propsArg)
{
	defaultCB= &callback;
}

mail::smapSTORE::~smapSTORE()
{
}

void mail::smapSTORE::installed(imap &imapAccount)
{
	msgRange.init(imapAccount, uidSet);

	uidSet.clear();

	if (go())
		return;

	ok("OK");
	imapAccount.uninstallHandler(this);
}

bool mail::smapSTORE::ok(std::string msg)
{
	if (go())
	{
		doDestroy=false;
		return true;
	}
	return smapHandler::ok(msg);
}

bool mail::smapSTORE::go()
{
	std::ostringstream o;

	o << "STORE";

	if (!myimap->currentFolder ||
	    myimap->currentFolder->closeInProgress ||
	    !(msgRange >> o))
		return false;

	o << " " << props << "\n";

	myimap->imapcmd("", o.str());
	return true;
}
