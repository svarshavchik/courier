/*
** Copyright 2003-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "tmpaccount.H"
#include <errno.h>
#include <cstring>

using namespace std;

mail::tmpaccount::add::add(tmpaccount *accountArg, mail::callback &cbArg)
	: addMessage(accountArg), newFile(tmpfile()),
	  rfc2045p(rfc2045_alloc()), account(accountArg),
	  cb(cbArg)
{
}

mail::tmpaccount::add::~add()
{
	if (newFile)
		fclose(newFile);
	if (rfc2045p)
		rfc2045_free(rfc2045p);
}

void mail::tmpaccount::add::saveMessageContents(string s)
{
	if (newFile)
		if (fwrite(&s[0], s.size(), 1, newFile) != 1)
			; // Ignore gcc warning
	if (rfc2045p)
		rfc2045_parse(rfc2045p, &s[0], s.size());
}

void mail::tmpaccount::add::fail(string errmsg)
{
	cb.fail(errmsg);
	delete this;
}

void mail::tmpaccount::add::go()
{
	if (!checkServer())
		return;

	if (!newFile || fflush(newFile) < 0 || ferror(newFile) || !rfc2045p)
	{
		fail(strerror(errno));
		return;
	}

	rfc2045_parse_partial(rfc2045p);

	if (account->f)
	{
		fclose(account->f);
		account->f=NULL;
		vector< pair<size_t, size_t> > dummy;

		dummy.push_back( make_pair( (size_t)0, (size_t)0));
		if (account->folder_callback)
			account->folder_callback->messagesRemoved(dummy);
	}

	if (account->rfc2045p)
		rfc2045_free(account->rfc2045p);

	account->rfc2045p=rfc2045p;
	account->f=newFile;
	rfc2045p=NULL;
	newFile=NULL;

	if (account->folder_callback)
		account->folder_callback->newMessages();

	cb.success("Ok.");
	delete this;
}
