/*
** Copyright 2003, S. Varshavchik.
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

	const char *getName() override;

	bool isOpen;

	size_t expectedFetchCount;
	size_t fetchCount;

	bool noopSent;

	void fetchedIndexInfo() override;
public:
	smapNEWMAIL(mail::callback *callbackArg,
		    bool isOpenArg);

	void installed(imap &) override;
	bool ok(std::string) override;
	bool fail(std::string message) override;

	~smapNEWMAIL();
};

LIBMAIL_END

#endif
