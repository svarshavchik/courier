/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "smap.H"
#include "smapstatus.H"
#include <sstream>

using namespace std;

////////////////////////////////////////////////////////////////////////
//
// STATUS

const char *mail::smapSTATUS::getName()
{
	return "STATUS";
}

mail::smapSTATUS::smapSTATUS(string pathArg,
			     mail::callback::folderInfo &infoCallbackArg,
			     mail::callback &callbackArg)
	: path(pathArg), infoCallback(infoCallbackArg)
{
	defaultCB= &callbackArg;
}

mail::smapSTATUS::~smapSTATUS()
{
}

void mail::smapSTATUS::installed(imap &imapAccount)
{
	vector<string> words;

	path2words(path, words);

	vector<string>::iterator b=words.begin(), e=words.end();

	string pstr="";

	while (b != e)
	{
		pstr += " ";
		pstr += imapAccount.quoteSMAP( *b );
		b++;
	}


	imapAccount.imapcmd("", (infoCallback.fastInfo
				 ? "STATUS CHEAP":"STATUS FULL")
			    + pstr + "\n");
}

bool mail::smapSTATUS::processLine(imap &imapAccount,
				 vector<const char *> &words)
{
	if (words.size() >= 2 && strcmp(words[0], "*") == 0 &&
	    strcasecmp(words[1], "STATUS") == 0)
	{
		vector<const char *>::iterator b=words.begin() + 2;

		while (b != words.end())
		{
			const char *c= *b++;

			if (strncasecmp(c, "EXISTS=", 7) == 0)
			{
				string s=c+7;
				istringstream i(s);

				i >> infoCallback.messageCount;
			}
			else if (strncasecmp(c, "UNSEEN=", 7) == 0)
			{
				string s=c+7;
				istringstream i(s);

				i >> infoCallback.unreadCount;
			}
		}
		return true;
	}
	return smapHandler::processLine(imapAccount, words);
}
