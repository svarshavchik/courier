/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "cursesmessagehtmlparser.H"
#include "gettext.H"

CursesMessage::HtmlParser
::HtmlParser( CursesMessage *messagePtrArg,
	      const std::string &content_chsetArg,
	      const std::string &my_chsetArg,
	      Demoronize &demoronizer,
	      size_t displayWidthArg)
	: ::htmlParser( content_chsetArg.size() == 0 ?
			my_chsetArg:content_chsetArg,
			my_chsetArg,
			demoronizer, displayWidthArg),
	  messagePtr(messagePtrArg),
	  streamPtr(NULL),
	  reply(false)
{
}

bool CursesMessage::HtmlParser::conversionError()
{
	return ::htmlParser::conversionError();
}

CursesMessage::HtmlParser
::HtmlParser( std::ostream &streamPtrArg,
	      bool replyArg,
	      const std::string &content_chsetArg,
	      const std::string &my_chsetArg,
	      Demoronize &demoronizer,
	      size_t displayWidthArg)
	: ::htmlParser( content_chsetArg.size() == 0 ?
			my_chsetArg:content_chsetArg,
			my_chsetArg,
			demoronizer, displayWidthArg),
	messagePtr(NULL),
	streamPtr(&streamPtrArg),
	reply(replyArg)
{
	init();
}

void CursesMessage::HtmlParser::init()
{
	parse(_("<b>&laquo; HTML content follows &raquo;</b><br>"));
}

CursesMessage::HtmlParser::~HtmlParser()
{
}

void CursesMessage::HtmlParser::parse(std::string text)
{
	::htmlParser::parse(text);
}

bool CursesMessage::HtmlParser::finish()
{
	flush(); // remaining flotsam from htmlParser
	return true;
}

void CursesMessage::HtmlParser::parsedLine(std::string line, bool wrapped)
{
	if (reply)
	{
		if (line.size() == 0 || line[0] != '>')
			line.insert(line.begin(), ' ');
		line.insert(line.begin(), '>');
	}

	// Non-wrapped lines cannot terminate with spaces

	if (line == "-- ")
		wrapped=false;
	else if (!wrapped)
	{
		std::string::iterator b(line.begin()), e(line.end());

		while (b != e)
		{
			if (e[-1] != ' ')
				break;
			--e;
		}

		line.erase(e, line.end());
	}

	if (messagePtr)
		messagePtr->reformatAddLine(line,
					    CursesMessage::LineIndex::ATTRIBUTES);

	if (streamPtr) // When replying, drop all attributes
	{
		std::vector<std::pair<textAttributes, std::string> > parsedLine;

		textAttributes::getAttributedText(line, parsedLine);

		std::vector<std::pair<textAttributes, std::string> >::iterator
			b, e;

		b=parsedLine.begin();
		e=parsedLine.end();

		while (b != e)
		{
			(*streamPtr) << b->second;
			b++;
		}

		(*streamPtr) << (wrapped ? " ":"") << std::endl;
	}
}
