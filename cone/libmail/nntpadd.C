/*
** Copyright 2003-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "nntpadd.H"
#include "nntppost.H"
#include <errno.h>
#include <cstring>

using namespace std;

mail::nntp::add::add(mail::nntp *myServerArg, mail::callback *callbackArg)
	: addMessage(myServerArg), myServer(myServerArg),
	  origCallback(callbackArg), tfile(tmpfile()), byteCount(0)
{
}

mail::nntp::add::~add()
{
	if (tfile)
		fclose(tfile);
}

void mail::nntp::add::saveMessageContents(std::string s)
{
	byteCount += s.size();

	if (tfile)
		if (fwrite(s.c_str(), s.size(), 1, tfile) != 1)
			; // Ignore gcc warning
}

void mail::nntp::add::go()
{
	if (!tfile || fflush(tfile) < 0 || ferror(tfile) < 0)
	{
		fail(strerror(errno));
		return;
	}

	if (!checkServer())
		return;

	PostTask *p=new PostTask(origCallback, *myServer, tfile);

	if (!p)
	{
		fail(strerror(errno));
		return;
	}

	try {
		myServer->installTask(p);
		tfile=NULL;
		origCallback=NULL;
	} catch (...) {
		tfile=p->msg;
		p->msg=NULL;
		delete p;
		throw;
	}
	delete this;
}

void mail::nntp::add::fail(string errmsg)
{
	origCallback->fail(errmsg);
	delete this;
}
