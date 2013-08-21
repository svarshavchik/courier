/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "smapcopy.H"

#include <vector>
#include <sstream>

using namespace std;

const char *mail::smapCOPY::getName()
{
	return "COPY";
}

mail::smapCOPY::smapCOPY(const std::vector<size_t> &messages,
			 mail::folder *copyTo,
			 mail::imap &imapAccount,
			 mail::callback &callback,
			 const char *cmdArg)
	: uidSet(imapAccount, messages), path(copyTo->getPath()),
	  cmd(cmdArg)
{
	defaultCB= &callback;
}

mail::smapCOPY::~smapCOPY()
{
}

void mail::smapCOPY::installed(imap &imapAccount)
{
	msgRange.init(imapAccount, uidSet);
	uidSet.clear();
	if (go())
		return;

	ok("OK");
	imapAccount.uninstallHandler(this);
}

bool mail::smapCOPY::ok(std::string msg)
{
	if (go())
	{
		doDestroy=false;
		return true;
	}

	return smapHandler::ok(msg);
}

bool mail::smapCOPY::go()
{
	ostringstream msgList;

	msgList << cmd;

	if (!myimap->currentFolder ||
	    myimap->currentFolder->closeInProgress ||
	    !(msgRange >> msgList))
		return false;

	vector<string> words;
	path2words(path, words);

	msgList << " \"\"";

        vector<string>::iterator fb=words.begin(), fe=words.end();

        while (fb != fe)
        {
                msgList << " " << myimap->quoteSMAP( *fb );
                fb++;
        }

	msgList << "\n";

	myimap->imapcmd("", msgList.str());

	return true;
}

bool mail::smapCOPY::processLine(imap &imapAccount,
				 vector<const char *> &words)
{
        if (words.size() >= 2 && strcmp(words[0], "*") == 0 &&
            strcasecmp(words[1], "COPY") == 0)
		return true;

	return smapHandler::processLine(imapAccount, words);
}
