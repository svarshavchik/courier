/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_smapaddmessage_H
#define libmail_smapaddmessage_H

#include "libmail_config.h"
#include	"mail.H"
#include	"smap.H"

#include	"addmessage.H"
#include	<stdio.h>

#include	<vector>

LIBMAIL_START

class smapAddMessage : public addMessage {

	mail::imap &imapAccount;

	mail::callback *callback;

	FILE *tfile;

public:
	smapAddMessage(mail::imap &imapAccount,
		       mail::callback &callbackArg);
	~smapAddMessage();

	std::vector<std::string> cmds;

	void saveMessageContents(std::string);
	void go();
	void fail(std::string);
};

LIBMAIL_END

#endif
