/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_nntpcache_H
#define libmail_nntpcache_H

#include "libmail_config.h"

#include "nntp.H"
#include "nntpfetch.H"

#include <string>

#include "rfc2045/rfc2045.h"
#include <stdio.h>

LIBMAIL_START

//
// Cache message contents.
//

class mail::nntp::CacheTask : public mail::nntp::FetchTaskBase {

	int *cacheFd;
	struct rfc2045 **cacheStruct;

	FILE *tmpfile;
	struct rfc2045 *rfc2045p;

public:
	CacheTask(mail::callback *callbackArg, nntp &myserverArg,
		  std::string groupNameArg,
		  size_t msgNumArg,
		  std::string uidArg,

		  int *cacheFdArg,
		  struct rfc2045 **cacheStructArg);

	~CacheTask();

	void success(std::string msg);
	void loggedIn();
	void fetchedText(std::string);
};

LIBMAIL_END

#endif
