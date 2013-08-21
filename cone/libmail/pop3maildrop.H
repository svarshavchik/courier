/*
** Copyright 2003-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_pop3maildrop_H
#define libmail_pop3maildrop_H

#include "libmail_config.h"
#include "mail.H"
#include "maildir.H"

////////////////////////////////////////////////////////////////////////
//
// A POP3 maildrop is subclassed from mail::maildir, so that it looks like
// a maildir.  It maintains a private mail::pop3 object that is used to
// feed grab mail from a pop3 maildrop, and copy it to the local maildir.
//

LIBMAIL_START

class pop3maildrop : private callback::disconnect,
	// Swallow disconnect events from maildir

		     public mail::maildir {

	class pop3acct;  // Custom POP3 subclass

	pop3acct *myPop3Acct;

	class checkNewMailHelper;
public:

	friend class CheckNewMailHelper;

	pop3maildrop(mail::callback::disconnect &disconnect_callback,
		     mail::loginCallback *loginCallbackFunc,
		     mail::callback &callback,
		     std::string pathArg,
		     std::string pop3url,
		     std::string pop3pass,
		     std::vector<std::string> &certificates);
	~pop3maildrop();

	// Hook into POP3:
	void logout(callback &callback);
	void checkNewMail(callback &callback);

	bool hasCapability(std::string capability);
	std::string getCapability(std::string capability);
private:
	// Inherited from callback::disconnect:

	void disconnected(const char *errmsg);
	void servererror(const char *errmsg);

};

LIBMAIL_END

#endif
