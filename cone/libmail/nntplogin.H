/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_nntplogin_H
#define libmail_nntplogin_H

#include "libmail_config.h"

#include "nntp.H"
#include "logininfo.H"

LIBMAIL_START

//
// Superclass of all NNTP tasks that must have a logged in connection
//

class mail::nntp::LoggedInTask : public mail::nntp::Task,
	    private loginInfo::callbackTarget {

	void (mail::nntp::LoggedInTask::*responseFunc)(const char *);

	bool loggingIn;

public:
	LoggedInTask(callback *callbackArg, nntp &myserverArg);
	~LoggedInTask();

	void serverResponse(const char *message);
	void installedTask();

	virtual void loggedIn()=0;
	virtual void processLine(const char *message)=0;

	void fail(std::string);
private:
	void doFwdResponse(const char *);

	void doGreeting(const char *);

	void authinfoUser();
	void authinfoPwd();

	void authCompleted();

	void doAuthUser(const char *);
	void doAuthPwd(const char *);
	void doModeReader1(const char *);
	void doModeReader2(const char *);

	void gotPassword(std::string);

	void loginInfoCallback(std::string);
	void loginInfoCallbackCancel();


	void doTryAgain(const char *);

	void loginCompleted();
	void loginCompleted2();
};

LIBMAIL_END
#endif
