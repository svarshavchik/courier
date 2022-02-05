/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_nntpadd_H
#define libmail_nntpadd_H

#include "libmail_config.h"

#include "nntp.H"
#include "addmessage.H"

#include <stdio.h>

LIBMAIL_START

// Posting messages to an nntp server

class mail::nntp::add : public addMessage {

	nntp *myServer;
	mail::callback *origCallback;

	FILE *tfile;

	unsigned long byteCount;

public:
	add(mail::nntp *myServerArg, mail::callback *callbackArg);
	~add();

	void saveMessageContents(std::string) override;
	void go() override;
	void fail(std::string errmsg) override;
};

LIBMAIL_END
#endif
