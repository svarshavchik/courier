/*
** Copyright 2003, S. Varshavchik.
**
** See COPYING for distribution information.
*/
#ifndef libmail_smapnoopexpunge_H
#define libmail_smapnoopexpunge_H

#include "libmail_config.h"
#include	"mail.H"
#include	"smap.H"

#include	"smapmsgrange.H"

LIBMAIL_START

class smapNoopExpunge : public smapHandler {

	const char *cmd;

	smapUidSet uidSet;
	bool expungeSet;

	const char *getName() override;

	bool existsOrExpungeFlag;

	void existsOrExpungeSeen() override;

public:
	smapNoopExpunge(const char *cmd,
			mail::callback &callbackArg,
			mail::imap &imapAccount);
	smapNoopExpunge(const char *cmd,
			mail::imap &imapAccount);
	smapNoopExpunge(const std::vector<size_t> &messageList,
			mail::callback &callbackArg,
			mail::imap &imapAccount);

	void installed(imap &) override;
	bool ok(std::string) override;
	bool fail(std::string message) override;

	~smapNoopExpunge();
};

LIBMAIL_END

#endif
