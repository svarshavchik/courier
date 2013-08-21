/*
** Copyright 2003-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef myserverlogincallback_H
#define myserverlogincallback_H

#include "config.h"

#include "libmail/logininfo.H"
#include "myserver.H"

//
// Implement Libmail login callbacks -- prompting for uid and passwd.

class myServerLoginCallback : public mail::loginCallback {
public:
	myServerLoginCallback();
	~myServerLoginCallback();

	myServer::Callback *myCallback;

	bool isPasswordPrompt;

	void reset();

	void loginPrompt(callbackType cbType, std::string prompt);

	void promptPassword(std::string serverName, std::string &password);
};

#endif
