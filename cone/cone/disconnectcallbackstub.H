/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef disconnectcallbackstub_H
#define disconnectcallbackstub_H

#include "config.h"

#include "libmail/mail.H"

//
// When we need to open a libmail account temporarily, use this account
// callback object.
//

class disconnectCallbackStub : public mail::callback::disconnect {
public:
	disconnectCallbackStub();
	~disconnectCallbackStub();

	void disconnected(const char *errmsg);
	void servererror(const char *errmsg);
};

#endif
