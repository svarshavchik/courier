/*
** Copyright 2003-2008, S. Varshavchik.
**
** See COPYING for distribution information.
*/
#include "tmpaccount.H"
#include <errno.h>
#include <cstring>

using namespace std;

mail::tmpaccount::add::add(tmpaccount *accountArg, mail::callback &cbArg)
	: addMessage(accountArg),
	  newFile{
		  std::make_shared<rfc822::fdstreambuf>(
			  rfc822::fdstreambuf::tmpfile()
		  )
	  }, account(accountArg),
	  cb(cbArg)
{
}

mail::tmpaccount::add::~add()=default;

void mail::tmpaccount::add::saveMessageContents(string s)
{
	if (newFile)
	{
		char *p=s.data();
		size_t n=s.size();

		while (n)
		{
			auto done=newFile->sputn(p, n);

			if (done <= 0) break;

			p += done;
			n -= done;
		}
	}

	rfc2045p.parse(s.begin(), s.end());
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

	if (!newFile || newFile->pubsync() < 0 || newFile->error())
	{
		fail(strerror(errno));
		return;
	}

	if (account->f)
	{
		account->f.reset();
		vector< pair<size_t, size_t> > dummy;

		dummy.push_back( make_pair( (size_t)0, (size_t)0));
		if (account->folder_callback)
			account->folder_callback->messagesRemoved(dummy);
	}

	account->rfc2045p=std::make_shared<rfc2045::entity>(
		rfc2045p.parsed_entity()
	);

	account->f=newFile;
	newFile.reset();

	if (account->folder_callback)
		account->folder_callback->newMessages();

	cb.success("Ok.");
	delete this;
}
