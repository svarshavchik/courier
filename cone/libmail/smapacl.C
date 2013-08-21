/*
** Copyright 2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "smap.H"
#include "smapacl.H"
#include <errno.h>
#include <stdio.h>

using namespace std;

////////////////////////////////////////////////////////////////////////
//
// ACL

const char *mail::smapACL::getName()
{
	return "ACL";
}

mail::smapACL::smapACL(string folderNameArg,
		       string &rightsArg,
		       mail::callback &getCallbackArg)
	: folderName(folderNameArg),
	  rights(rightsArg)
{
	defaultCB= &getCallbackArg;
}

mail::smapACL::~smapACL()
{
}

void mail::smapACL::installed(imap &imapAccount)
{
	vector<string> folderPathWords;
	vector<string>::iterator b, e;

	string pstr="";

	path2words(folderName, folderPathWords);

	b=folderPathWords.begin();
	e=folderPathWords.end();

	while (b != e)
	{
		pstr += " ";
		pstr += imapAccount.quoteSMAP( *b );
		b++;
	}


	imapAccount.imapcmd("", "ACL" + pstr + "\n");
}

bool mail::smapACL::processLine(imap &imapAccount,
				vector<const char *> &words)
{
	if (words.size() >= 3 && strcmp(words[0], "*") == 0 &&
	    strcasecmp(words[1], "ACL") == 0)
	{
		rights=words[2];
		return true;
	}

	return smapHandler::processLine(imapAccount, words);
}

////////////////////////////////////////////////////////////////////////
//
// GETACL

const char *mail::smapGETACL::getName()
{
	return "GETACL";
}

mail::smapGETACL::smapGETACL(string folderNameArg,
			     list<pair<string, string> > &rightsArg,
			     mail::callback &getCallbackArg)
	: folderName(folderNameArg),
	  rights(rightsArg)
{
	defaultCB= &getCallbackArg;
}

mail::smapGETACL::~smapGETACL()
{
}

void mail::smapGETACL::installed(imap &imapAccount)
{
	vector<string> folderPathWords;
	vector<string>::iterator b, e;

	string pstr="";

	path2words(folderName, folderPathWords);

	b=folderPathWords.begin();
	e=folderPathWords.end();

	while (b != e)
	{
		pstr += " ";
		pstr += imapAccount.quoteSMAP( *b );
		b++;
	}


	imapAccount.imapcmd("", "GETACL" + pstr + "\n");
}

bool mail::smapGETACL::processLine(imap &imapAccount,
				   vector<const char *> &words)
{
	if (words.size() >= 2 && strcmp(words[0], "*") == 0 &&
	    strcasecmp(words[1], "GETACL") == 0)
	{
		vector<const char *>::iterator
			b=words.begin()+2, e=words.end();

		while (b != e)
		{
			string identifier= *b++;

			if (b != e)
			{
				string rightsW= *b++;

				rights.push_back(make_pair(identifier,
							   rightsW));
			}
		}
		return true;
	}

	return smapHandler::processLine(imapAccount, words);
}

////////////////////////////////////////////////////////////////////////
//
// SETACL

const char *mail::smapSETACL::getName()
{
	return "SETACL";
}

mail::smapSETACL::smapSETACL(string folderNameArg,
			     string identifierArg,
			     string rightsArg,
			     bool delIdentifierArg,
			     string &errorIdentifierArg,
			     vector<string> &errorRightsArg,
			     mail::callback &callbackArg)
	: folderName(folderNameArg),
	  identifier(identifierArg),
	  rights(rightsArg),
	  delIdentifier(delIdentifierArg),
	  errorIdentifier(errorIdentifierArg),
	  errorRights(errorRightsArg)
{
	defaultCB= &callbackArg;
}

mail::smapSETACL::~smapSETACL()
{
}

void mail::smapSETACL::installed(imap &imapAccount)
{
	vector<string> folderPathWords;
	vector<string>::iterator b, e;

	string pstr="";

	path2words(folderName, folderPathWords);

	b=folderPathWords.begin();
	e=folderPathWords.end();

	while (b != e)
	{
		pstr += " ";
		pstr += imapAccount.quoteSMAP( *b );
		b++;
	}


	imapAccount.imapcmd("",
			    (delIdentifier ?
			     "DELETEACL" + pstr + " \"\" "
			     + imapAccount.quoteSMAP(identifier):
			     "SETACL" + pstr + " \"\" "
			     + imapAccount.quoteSMAP(identifier)
			     + " "
			     + imapAccount.quoteSMAP(rights)) + "\n");
}

bool mail::smapSETACL::processLine(imap &imapAccount,
				   vector<const char *> &words)
{
	if (words.size() >= 2 && strcmp(words[0], "*") == 0 &&
	    strcasecmp(words[1], "GETACL") == 0)
		return true; // Don't care

	return smapHandler::processLine(imapAccount, words);
}
