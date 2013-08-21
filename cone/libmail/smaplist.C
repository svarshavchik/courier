/*
** Copyright 2003-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "smap.H"
#include "misc.H"
#include "smaplist.H"
#include <sstream>
#include <errno.h>
#include <cstring>

using namespace std;

////////////////////////////////////////////////////////////////////////
//
// LIST

const char *mail::smapLIST::getName()
{
	return "LIST";
}

mail::smapLIST::smapLIST(string pathArg,
			 mail::callback::folderList &listCallbackArg,
			 mail::callback &callbackArg)
	: path(pathArg), listCallback(listCallbackArg)
{
	defaultCB= &callbackArg;
}

mail::smapLIST::~smapLIST()
{
	vector<mail::folder *>::iterator b=subfolders.begin(),
		e=subfolders.end();

	while (b != e)
	{
		mail::folder *f= *b;

		*b=NULL;
		b++;
		if (f)
			delete f;
	}
}

void mail::smapLIST::installed(imap &imapAccount)
{
	string pstr="";

	if (path.size() > 0)
	{
		vector<string> words;

		path2words(path, words);

		vector<string>::iterator b=words.begin(), e=words.end();

		pstr="";

		while (b != e)
		{
			pstr += " ";
			pstr += imapAccount.quoteSMAP( *b );
			b++;
		}
	}

	imapAccount.imapcmd("", "LIST" + pstr + "\n");
}

bool mail::smapLIST::processLine(imap &imapAccount,
				 vector<const char *> &words)
{
	if (words.size() >= 4 && strcmp(words[0], "*") == 0 &&
	    strcasecmp(words[1], "LIST") == 0)
	{
		vector<const char *> dummy;

		dummy.push_back(words[2]);

		string p=path;
		if (p.size() > 0)
			p += "/";
		p += words2path(dummy);

		imapFolder newFolder(imapAccount, p, "",
				     fromutf8(words[3]), 0);

		vector<const char *>::iterator b=words.begin() + 4;

		bool hasFOLDER=false;
		bool hasDIRECTORY=false;

		while (b != words.end())
		{
			const char *c= *b++;

			if (strcasecmp(c, "FOLDER") == 0)
			{
				hasFOLDER=true;
			}
			else if (strcasecmp(c, "DIRECTORY") == 0)
			{
				hasDIRECTORY=true;
			}
		}

		newFolder.hasMessages(hasFOLDER);
		newFolder.hasSubFolders(hasDIRECTORY);

		imapFolder *f=new imapFolder(newFolder);

		if (!f)
			LIBMAIL_THROW(strerror(errno));

		try {
			subfolders.push_back(f);
		} catch (...) {
			delete f;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}
		return true;
	}

	return smapHandler::processLine(imapAccount, words);
}

bool mail::smapLIST::ok(std::string okMsg)
{
	vector<const mail::folder *> cpy;

	cpy.insert(cpy.end(), subfolders.begin(), subfolders.end());

	listCallback.success(cpy);
	return smapHandler::ok(okMsg);
}

mail::smapLISToneFolder
::smapLISToneFolder(std::string pathArg,
		    mail::callback::folderList &listCallbackArg,
		    mail::callback &callbackArg)
	: smapLIST(pathArg, listCallbackArg, callbackArg)
{
	vector<string> words;

	path2words(path, words);

	nameComponent=words.end()[-1];
	size_t n=path.rfind('/');

	if (n == std::string::npos)
		path="";
	else
		path=path.substr(0, n);
}

mail::smapLISToneFolder::~smapLISToneFolder()
{
}

bool mail::smapLISToneFolder::processLine(imap &imapAccount,
					  std::vector<const char *> &words)
{
	if (words.size() >= 4 && strcmp(words[0], "*") == 0 &&
	    strcasecmp(words[1], "LIST") == 0)
	{
		if (nameComponent != words[2])
			return true;
	}

	if (!smapLIST::processLine(imapAccount, words))
		return false;

	if (subfolders.size() == 2) // Could be two entries, one a folder,
		// then a second entry as a folder directory
	{
		mail::folder *a=subfolders[0];
		mail::folder *b=subfolders[1];

		if (b->hasMessages())
			a->hasMessages(true);

		if (b->hasSubFolders())
			a->hasSubFolders(true);

		subfolders.erase(subfolders.begin() + 1);
		delete b;
	}
	return true;
}

bool mail::smapLISToneFolder::ok(std::string msg)
{
	// If the folder wasn't found, create a fake entry

	if (subfolders.size() == 0)
	{
		vector<const char *> dummy;
		dummy.push_back("*");
		dummy.push_back("LIST");
		dummy.push_back(nameComponent.c_str());
		dummy.push_back("");
		processLine(*myimap, dummy);
	}

	return smapLIST::ok(msg);
}

