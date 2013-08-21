/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_genericdecode_H
#define libmail_genericdecode_H

#include "libmail_config.h"
#include "mail.H"
#include "generic.H"
#include "autodecoder.H"

#include "base64.H"
#include "qp.H"

///////////////////////////////////////////////////////////////////////////
//
// A helper object for the generic readMessageContentDecoded method
// implementation.  The mail::autodecoder() superclass handles the actual
// decoding task.  The decoded() method forwards the decoded data to the
// original application callback object.
//
// This helper object also conveniently subclasses mail::callback::message,
// nicely forwarding the retrieved MIME content to mail::autodecoder.
// It is dynamically allocated, and after its success() or fail() method is
// invoked, the message is obediently passed along to the original
// callback function, and afterwards this object destroys itself.

class mail::generic::Decode : public mail::autodecoder,
		     public mail::callback::message {

	mail::callback::message &originalCallback;

public:
	Decode(mail::callback::message &callback, std::string transferEncoding);
	~Decode();

private:
	size_t messageNum;

	void decoded(std::string buffer);
	void messageTextCallback(size_t n, std::string text);
	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);

	void fail(std::string message);
	void success(std::string message);
};

#endif
