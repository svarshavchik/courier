/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_envelope_h
#define libmail_envelope_h

#include <vector>
#include <string>

#include "namespace.H"

//
// The message envelope information (sender/recipients, other useful stuff)

LIBMAIL_START

class address;

class envelope {

public:
	envelope();
	virtual ~envelope();
	time_t date;
	std::string subject;
	std::vector<mail::address> from, sender, replyto, to, cc, bcc;
	std::string inreplyto, messageid;

	std::vector<std::string> references; // Synthesized
};

// An envelope with two more fields: message arrival date, and message size

class xenvelope : public envelope {
public:
	xenvelope();
	~xenvelope();

	time_t arrivalDate;
	unsigned long messageSize;

	xenvelope &operator=(const envelope &e);
};

LIBMAIL_END

#endif
