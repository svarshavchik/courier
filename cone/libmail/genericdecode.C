/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "genericdecode.H"

using namespace std;

mail::generic::Decode::Decode(mail::callback::message &callback,
			      string transferEncoding)
	: mail::autodecoder(transferEncoding),
	  originalCallback(callback)
{
}

mail::generic::Decode::~Decode()
{
}

void mail::generic::Decode::messageTextCallback(size_t n, string text)
{
	messageNum=n; // Save the orig msg number (should never change)
	decode(text);
}

void mail::generic::Decode::reportProgress(size_t bc, size_t bt,
					   size_t mc, size_t mt)
{
	originalCallback.reportProgress(bc, bt, mc, mt);
}

void mail::generic::Decode::decoded(string text)
{
	originalCallback.messageTextCallback(messageNum, text);
}

void mail::generic::Decode::fail(string message)
{
	originalCallback.fail(message);
	delete this;
}

void mail::generic::Decode::success(string message)
{
	originalCallback.success(message);
	delete this;
}

