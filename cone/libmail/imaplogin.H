/*
** Copyright 2002, S. Varshavchik.
**
** See COPYING for distribution information.
*/
#ifndef libmail_imaplogin_H
#define libmail_imaplogin_H

#include	"config.h"
#include	"logininfo.H"

#include	"imap.H"

LIBMAIL_START

class loginInfo;

/////////////////////////////////////////////////////////////////////////
//
// Initial handler that waits for a greeting message, and decides what to
// do from there.

class imapGreetingHandler : public imapHandler {

private:
	mail::loginInfo myLoginInfo;

	const char *getName() override;
	void timedOut(const char *errmsg) override;
	int capability_sent;
	int completed;

	bool preauthenticated;
public:
	imapGreetingHandler(mail::loginInfo loginInfo);
	~imapGreetingHandler();

	void installed(imap &) override;

	int process(imap &imapAccount, std::string &buffer) override;

	void error(imap &imapAccount, std::string errmsg);
};

LIBMAIL_END

#endif
