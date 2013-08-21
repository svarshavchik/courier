/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_smapstore_H
#define libmail_smapstore_H

#include "libmail_config.h"
#include	"mail.H"
#include	"imap.H"
#include	"smap.H"

#include	"smapmsgrange.H"

#include <vector>

LIBMAIL_START

class smapSTORE : public smapHandler {

	const char *getName();

	smapUidSet uidSet;
	smapMsgRange msgRange;

	std::string props;

public:
	smapSTORE(size_t messageNumArg,
		  std::string propsArg,
		  mail::imap &imapAccount,
		  mail::callback &call);
	smapSTORE(const std::vector<size_t> &messages,
		  std::string propsArg,
		  mail::imap &imapAccount,
		  mail::callback &call);
	~smapSTORE();

	void installed(imap &);

	bool ok(std::string);

	bool go();
};

LIBMAIL_END

#endif
