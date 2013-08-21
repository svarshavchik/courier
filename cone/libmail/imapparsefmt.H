/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_imapparsefmt_H
#define libmail_imapparsefmt_H

#include "libmail_config.h"
#include "mail.H"
#include "imaphandler.H"
#include <vector>

LIBMAIL_START

class imap;
class imapHandlerStructured;

/////////////////////////////////////////////////////////////////////
//
//  A helper class for parsing complicated IMAP replies.
//
//  Intended to be used by a imapHandlerStructured subclass.
//
//  Have the subclass construct this object, then repeatedly call
//  process(), as it receives tokens from imapHandlerStructured.
//
//  Navigate the built tree when process() returns true, looking at children
//  and value members.

class imapparsefmt {

	imapparsefmt *current;

public:
	bool nil;			// NIL value
	std::string value;			// string value
	std::vector<imapparsefmt *> children;	// Children
	imapparsefmt *parent;

	imapparsefmt();
	~imapparsefmt();

	void begin()
	{
		*this=imapparsefmt();
	}

	imapparsefmt &operator=(const imapparsefmt &);
	imapparsefmt(const imapparsefmt &);

	bool process(imap &, imapHandlerStructured::Token &,
		     bool &parse_error);

private:
	imapparsefmt(imapparsefmt *parent);

	void destroy();
};

LIBMAIL_END

#endif
