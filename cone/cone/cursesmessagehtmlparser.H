/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef cursesmessagehtmlparser_H
#define cursesmessagehtmlparser_H

#include "cursesmessage.H"
#include "htmlparser.H"

#include <iostream>

//////////////////////////////////////////////////////////////////////
//
// Try to make sense out of HTML goo.

class CursesMessage::HtmlParser : public ::htmlParser,
		     public CursesMessage::reformatter {

	//
	// HtmlParser generates two kinds of output.
	//

	// A) Attributed output.  Text with embedded attribute characters
	// (bold, underlined, etc), that's sent to messagePtr
	//

	CursesMessage *messagePtr;

	//
	// B) Non-attributed output.  Plain text.  When formatting a reply
	// based on HTML content, the plain output is sent to a stream.

	std::ostream *streamPtr;

	//
	// All the heavy lifting is done in the superclass.  This class
	// cleans up some minor details.


	bool reply;	// If true, extra level of quoting is prepended.

	void parse(std::string text);
	bool finish();

	bool conversionError();

	bool conversionErrorFlag;
public:
	void init();

	// Constructor for displaying of attributed text.

	HtmlParser( CursesMessage *messagePtrArg,
		    const std::string &content_chsetArg,
		    const std::string &my_chsetArg,
		    Demoronize &demoronizer,
		    // Alternative unicode entity representation, used for
		    // demoronization

		    size_t displayWidthArg);

	// Constructor for formatting a plain text reply.
	HtmlParser( std::ostream &ostreamPtrArg,
		    bool replyArg,
		    const std::string &content_chsetArg,
		    const std::string &my_chsetArg,
		    Demoronize &demoronizer,
		    size_t displayWidthArg);
	~HtmlParser();

private:
	void parsedLine(std::string line, bool wrapped);
};

#endif
