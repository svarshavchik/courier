/*
** Copyright 2003-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "nntpcache.H"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <cstring>

mail::nntp::CacheTask::CacheTask(mail::callback *callbackArg,
				 nntp &myserverArg,
				 std::string groupNameArg,
				 size_t msgNumArg,
				 std::string uidArg,

				 int *cacheFdArg,
				 struct rfc2045 **cacheStructArg)
	: FetchTaskBase(callbackArg, myserverArg, groupNameArg,
			msgNumArg, uidArg, mail::readBoth),
	  cacheFd(cacheFdArg),
	  cacheStruct(cacheStructArg),
	  tmpfile(NULL),
	  rfc2045p(NULL)
{
}

mail::nntp::CacheTask::~CacheTask()
{
	if (tmpfile)
		fclose(tmpfile);

	if (rfc2045p)
		rfc2045_free(rfc2045p);
}

void mail::nntp::CacheTask::loggedIn()
{
	if ((tmpfile= ::tmpfile()) == NULL ||
	    (rfc2045p=rfc2045_alloc()) == NULL)
	{
		fail(strerror(errno));
		return;
	}

	FetchTaskBase::loggedIn();
}

void mail::nntp::CacheTask::fetchedText(std::string txt)
{
	if (fwrite(txt.c_str(), txt.size(), 1, tmpfile) != 1)
		; // Ignore gcc warning
	rfc2045_parse(rfc2045p, txt.c_str(), txt.size());
}

void mail::nntp::CacheTask::success(std::string msg)
{
	rfc2045_parse_partial(rfc2045p);

	if (fflush(tmpfile) < 0 || ferror(tmpfile))
	{
		fail(strerror(errno));
		return;
	}

	fcntl(fileno(tmpfile), F_SETFD, FD_CLOEXEC);

	myserver->cleartmp();

	myserver->cachedUid=uid;
	myserver->genericTmpFp=tmpfile;
	tmpfile=NULL;

	myserver->genericTmpRfcp=rfc2045p;
	rfc2045p=NULL;

	if (cacheFd)
		*cacheFd=fileno(myserver->genericTmpFp);

	if (cacheStruct)
		*cacheStruct=myserver->genericTmpRfcp;

	FetchTaskBase::success(msg);
}
