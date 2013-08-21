/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "logininfo.H"

///////////////////////////////////////////////////////////////////////////
//
// mail::imapLoginInfo TLS helper functions

mail::loginInfo::loginInfo()
	: loginCallbackFunc(NULL),
	  use_ssl(false), callbackPtr(0), tlsInfo(0)
{
}

mail::loginInfo::~loginInfo()
{
}

///////////////////////////////////////////////////////////////////////////
//
// The callback object

mail::loginCallback::loginCallback() : target(NULL)
{
}

mail::loginCallback::~loginCallback()
{
}

void mail::loginCallback::callback(std::string v)
{
	mail::loginInfo::callbackTarget *t=target;

	target=NULL;

	if (t)
		t->loginInfoCallback(v);
}

void mail::loginCallback::callbackCancel()
{
	mail::loginInfo::callbackTarget *t=target;

	target=NULL;

	if (t)
		t->loginInfoCallbackCancel();
}

void mail::loginCallback::getUserid()
{
	loginPrompt(USERID, "Userid: ");
}

void mail::loginCallback::getPassword()
{
	loginPrompt(PASSWORD, "Password: ");
}

mail::loginInfo::callbackTarget::callbackTarget()
	: currentCallback(NULL)
{
}

mail::loginInfo::callbackTarget::~callbackTarget()
{
	if (currentCallback)
		currentCallback->target=NULL;
}


