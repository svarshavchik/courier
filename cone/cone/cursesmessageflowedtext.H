/*
** Copyright 2003-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef cursesmessageflowedtext_H
#define cursesmessageflowedtext_H

#include "cursesmessage.H"
#include "rfc2045/rfc3676parser.h"


//////////////////////////////////////////////////////////////////////
// Helper classes for flowed text reformatting
//

class CursesMessage::FlowedTextParser : public mail::textplainparser,
		     public CursesMessage::reformatter {

	CursesMessage *messagePtr;
	std::string content_chset;
	std::string my_chset;
	size_t displayWidth;

	// Inherited from reformatter
	void parse(std::string text) override;
	bool finish() override;
	bool conversionError() override;

	bool conversionErrorFlag;

	size_t currentQuoteLevel;

	// Inherited from textplainparser

	void line_begin(size_t) override;

	void line_contents(const char32_t *, size_t) override;

	void line_end() override;

	unicoderewrapnone norewrapper;

	unicodewordwrapper<CursesMessage::FlowedTextParser> wrapper;

	std::vector<std::string> wrapped_lines;

	void flushlines();

	bool emitted_line;
public:
	FlowedTextParser( CursesMessage *messagePtrArg,
			  const std::string &content_chsetArg,
			  const std::string &my_chsetArg,
			  size_t displayWidthArg,
			  bool flowed,
			  bool delsp);
	~FlowedTextParser();


	FlowedTextParser &operator++() { return *this; }
	FlowedTextParser &operator++(int) { return *this; }
	FlowedTextParser &operator*() { return *this; }
	void operator=(const std::u32string &);
};

#endif
