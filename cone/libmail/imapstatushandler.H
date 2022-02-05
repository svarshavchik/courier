/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_imapstatushandler_H
#define libmail_imapstatushandler_H

#include "libmail_config.h"
#include "imaphandler.H"
#include "imapfolders.H"

////////////////////////////////////////////////////////////////////////
// The STATUS command

LIBMAIL_START

class imapStatusHandler : public imapCommandHandler {

	mail::callback::folderInfo &callback1;
	mail::callback &callback2;

public:
	std::string hier;

	imapStatusHandler(mail::callback::folderInfo &myCallback,
				mail::callback &myCallback2,
				std::string myHier);
	~imapStatusHandler();
	const char *getName() override;
	void timedOut(const char *errmsg) override;

	void installed(imap &imapAccount) override;
private:

	bool untaggedMessage(imap &imapAccount, std::string name) override;
	bool taggedMessage(imap &imapAccount, std::string name, std::string message,
			   bool okfail, std::string errmsg) override;
};

LIBMAIL_END

#endif
