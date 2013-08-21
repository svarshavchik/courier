/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "smap.H"
#include "smapdelete.H"

using namespace std;

///////////////////////////////////////////////////////////////////////
//
// DELETE

const char *mail::smapDELETE::getName()
{
	return "DELETE";
}

mail::smapDELETE::smapDELETE(std::string pathArg,
			     bool deleteDirectoryArg,
			     mail::callback &callbackArg)
	: path(pathArg),
	  deleteDirectory(deleteDirectoryArg)
{
	defaultCB= &callbackArg;
}

mail::smapDELETE::~smapDELETE()
{
}

void mail::smapDELETE::installed(imap &imapAccount)
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

	imapAccount.imapcmd("", (deleteDirectory ? "RMDIR":"DELETE")
			    + pstr + "\n");
}
