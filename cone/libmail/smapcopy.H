/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_smapcopy_H
#define libmail_smapcopy_H

#include "libmail_config.h"
#include	"mail.H"
#include	"imap.H"
#include	"smap.H"
#include	"smapmsgrange.H"

LIBMAIL_START

class smapCOPY : public smapHandler {

	const char *getName() override;

	smapUidSet uidSet;
	smapMsgRange msgRange;

	std::string path;

	const char *cmd;

public:
	smapCOPY(const std::vector<size_t> &messages,
		 mail::folder *copyTo,
		 mail::imap &imapAccount,
		 mail::callback &callback,
		 const char *cmdArg);

	~smapCOPY();

	void installed(imap &) override;
	bool ok(std::string) override;

	bool go();
	bool processLine(imap &imapAccount, std::vector<const char *> &words) override;
};

LIBMAIL_END

#endif
