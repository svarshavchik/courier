/*
** Copyright 2003-2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "smap.H"
#include "misc.H"
#include "smapcreate.H"
#include <errno.h>
#include <stdio.h>

using namespace std;

////////////////////////////////////////////////////////////////////////
//
// CREATE

const char *mail::smapCREATE::getName()
{
	return "CREATE";
}

mail::smapCREATE::smapCREATE(string pathArg,
			     string name,
			     bool createDirectoryArg,
			     mail::callback::folderList &listCallbackArg,
			     mail::callback &callbackArg)
	
	: path(pathArg),
	  createDirectory(createDirectoryArg),
	  renameFolder(false),
	  listCallback(&listCallbackArg)
{
	defaultCB= &callbackArg;
	if (path.size() > 0)
		path += "/";

	name=toutf8(name);

	vector<const char *> w;

	w.push_back(name.c_str());

	path += words2path(w);
}

mail::smapCREATE::smapCREATE(string oldPathArg,
			     string pathArg,
			     string name,
			     mail::callback::folderList &listCallbackArg,
			     mail::callback &callbackArg)
	: oldPath(oldPathArg),
	  path(pathArg),
	  createDirectory(false),
	  renameFolder(true),
	  listCallback(&listCallbackArg)
{
	defaultCB= &callbackArg;
	if (path.size() > 0)
		path += "/";

	name=toutf8(name);

	vector<const char *> w;

	w.push_back(name.c_str());

	path += words2path(w);
}

mail::smapCREATE::smapCREATE(string pathArg,
			     bool createDirectoryArg,
			     mail::callback &callbackArg)
	
	: path(pathArg),
	  createDirectory(createDirectoryArg),
	  renameFolder(false),
	  listCallback(NULL)
{
	defaultCB= &callbackArg;
}

mail::smapCREATE::~smapCREATE()
{
}

void mail::smapCREATE::installed(imap &imapAccount)
{
	vector<string> oldPathWords;
	vector<string> words;

	path2words(oldPath, oldPathWords);

	path2words(path, words);

	vector<string>::iterator b, e;

	string pstr="";

	if (renameFolder)
	{
		b=oldPathWords.begin();
		e=oldPathWords.end();

		while (b != e)
		{
			pstr += " ";
			pstr += imapAccount.quoteSMAP( *b );
			b++;
		}

		pstr += " \"\"";
	}

	b=words.begin();
	e=words.end();

	while (b != e)
	{
		pstr += " ";
		pstr += imapAccount.quoteSMAP( *b );
		b++;
	}

	imapAccount.imapcmd("", (renameFolder ? "RENAME":
				 createDirectory ? "MKDIR":"CREATE")
			    + pstr + "\n");
}

bool mail::smapCREATE::ok(std::string okMsg)
{
	if (listCallback)
	{
		size_t n=path.rfind('/');

		string name= n == std::string::npos ? path:path.substr(n+1);

		vector<string> words;

		path2words(name, words);

		mail::imapFolder f( *myimap, path, "", fromutf8(words[0]), 0);

		f.hasMessages(!createDirectory);
		f.hasSubFolders(createDirectory);

		vector<const mail::folder *> a;

		a.push_back(&f);
		listCallback->success(a);
	}
	return smapHandler::ok(okMsg);
}
