/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_smapsearch_H
#define libmail_smapsearch_H

#include "libmail_config.h"
#include	"mail.H"
#include	"imap.H"
#include	"smap.H"
#include	"smapmsgrange.H"
#include	"search.H"

#include	<vector>

LIBMAIL_START

class smapSEARCH : public smapHandler {

	const char *getName() override;

	searchParams parameters;
	searchCallback *callback;

	std::vector<size_t> found;

public:
	smapSEARCH(const searchParams &parametersArg,
		   searchCallback &callbackArg,
		   mail::imap &imapAccount);

	~smapSEARCH();

	void installed(imap &) override;
	bool ok(std::string) override;
	bool fail(std::string message) override;

	bool processLine(imap &imapAccount, std::vector<const char *> &words) override;
};

LIBMAIL_END

#endif
