/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_smapnewmail_H
#define libmail_smapnewmail_H

#include "libmail_config.h"
#include	"mail.H"
#include	"smap.H"

LIBMAIL_START

class smapNEWMAIL : public smapHandler {

	const char *getName();

	bool isOpen;

	size_t expectedFetchCount;
	size_t fetchCount;

	bool noopSent;

	void fetchedIndexInfo();
public:
	smapNEWMAIL(mail::callback *callbackArg,
		    bool isOpenArg);

	void installed(imap &);
	bool ok(std::string);
	bool fail(std::string);

	~smapNEWMAIL();
};

LIBMAIL_END

#endif
