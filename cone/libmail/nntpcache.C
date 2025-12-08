/*
** Copyright 2003-2008, S. Varshavchik.
**
** See COPYING for distribution information.
*/
#include "nntpcache.H"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <cstring>

mail::nntp::CacheTask::CacheTask(
	mail::callback *callbackArg,
	nntp &myserverArg,
	std::string groupNameArg,
	size_t msgNumArg,
	std::string uidArg,
	std::shared_ptr<rfc822::fdstreambuf> *cacheFdArg,
	std::shared_ptr<rfc2045::entity> *cacheStructArg)
	: FetchTaskBase(callbackArg, myserverArg, groupNameArg,
			msgNumArg, uidArg, mail::readBoth),
	  cacheFd(cacheFdArg),
	  cacheStruct(cacheStructArg)
{
}

mail::nntp::CacheTask::~CacheTask()=default;

void mail::nntp::CacheTask::loggedIn()
{
	this->tmpfile=std::make_shared<rfc822::fdstreambuf>(
		rfc822::fdstreambuf::tmpfile()
	);

	if (!this->tmpfile->error())
	{
		fcntl(this->tmpfile->fileno(), F_SETFD, FD_CLOEXEC);

		FetchTaskBase::loggedIn();
		return;
	}

	fail(strerror(errno));
}

void mail::nntp::CacheTask::fetchedText(std::string txt)
{
	auto p=txt.data();
	auto n=txt.size();

	while (n)
	{
		auto done=tmpfile->sputn(p, n);
		if (done <= 0)
			break;

		p += done;
		n -= done;
	}
	rfc2045p.parse(txt.begin(), txt.end());
}

void mail::nntp::CacheTask::success(std::string msg)
{
	myserver->cleartmp();

	myserver->cachedUid=uid;
	myserver->genericTmpFp=std::move(tmpfile);
	myserver->genericTmpRfcp=std::make_shared<rfc2045::entity>(
		rfc2045p.parsed_entity()
	);

	if (cacheFd)
		*cacheFd=myserver->genericTmpFp;

	if (cacheStruct)
		*cacheStruct=myserver->genericTmpRfcp;

	FetchTaskBase::success(msg);
}
