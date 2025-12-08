/*
** Copyright 2003, S. Varshavchik.
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

	std::shared_ptr<rfc822::fdstreambuf> *cacheFd;
	std::shared_ptr<rfc2045::entity> *cacheStruct;


	std::shared_ptr<rfc822::fdstreambuf> tmpfile;
	rfc2045::entity_parser<false> rfc2045p;

public:
	CacheTask(mail::callback *callbackArg, nntp &myserverArg,
		  std::string groupNameArg,
		  size_t msgNumArg,
		  std::string uidArg,
		  std::shared_ptr<rfc822::fdstreambuf> *cacheFdArg,
		  std::shared_ptr<rfc2045::entity> *cacheStructArg);

	~CacheTask();

	void success(std::string msg) override;
	void loggedIn() override;
	void fetchedText(std::string) override;
};

LIBMAIL_END

#endif
