/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_mboxmagictag_h
#define libmail_mboxmagictag_h

#include "libmail_config.h"
#include "mail.H"
#include "maildir/maildirkeywords.h"

#include <string>

LIBMAIL_START

///////////////////////////////////////////////////////////////////////////
//
// Each message in the mboxAccount file has a magic header prepended to it, as
// the first line after the From_ line.  This header line is not seen by
// the application.  The header line contains the message's UID, and flags.

class mboxMagicTag {

	std::string tag;

	mail::keywords::Message keywords;

public:
	mboxMagicTag(std::string header, mail::keywords::Hashtable &h);
	// Initialize from the first header line.

	mboxMagicTag(std::string uid, mail::messageInfo flags,
		     mail::keywords::Message keywordsArg);
	// Manual initialization

	mboxMagicTag();
	// Create a new UID

	~mboxMagicTag();

	bool good() const { return tag.size() > 0; }

	mail::messageInfo getMessageInfo() const;

	std::string toString() const;
	// Convert to a header line, sans the newline.


	mail::keywords::Message &getKeywords() { return keywords; }
	void setKeywords(const mail::keywords::Message &m) { keywords=m; }

};

LIBMAIL_END

#endif
