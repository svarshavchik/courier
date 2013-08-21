/*
** Copyright 2003-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "cursesmessageflowedtext.H"
#include "htmlentity.h"
#include "htmlparser.H"
#include "colors.H"

#include <algorithm>

extern Demoronize *currentDemoronizer;

extern struct CustomColor color_md_quote1;
extern struct CustomColor color_md_quote2;
extern struct CustomColor color_md_quote3;
extern struct CustomColor color_md_quote4;

static struct CustomColor *quote_colors[]=
	{&color_md_quote1,
	 &color_md_quote2,
	 &color_md_quote3,
	 &color_md_quote4};

CursesMessage::FlowedTextParser
::FlowedTextParser(CursesMessage *messagePtrArg,
		   const std::string &content_chsetArg,
		   const std::string &my_chsetArg,
		   size_t displayWidthArg,
		   bool flowed,
		   bool delsp)
	: messagePtr(messagePtrArg),
	  content_chset(content_chsetArg),
	  my_chset(my_chsetArg),
	  displayWidth(displayWidthArg),
	  conversionErrorFlag(false),
	  currentQuoteLevel(0)
{
	if (!begin(content_chset, flowed, delsp))
		conversionErrorFlag=true;
}

CursesMessage::FlowedTextParser::~FlowedTextParser()
{
	messagePtr=NULL;
	wrapper=unicodewordwrapper<CursesMessage::FlowedTextParser>();
	// Something might still flush out from wrapper, so nuke it.
}

bool CursesMessage::FlowedTextParser::conversionError()
{
	return conversionErrorFlag;
}

void CursesMessage::FlowedTextParser::parse(std::string text)
{
	mail::textplainparser::operator<<(text);
}

void CursesMessage::FlowedTextParser::line_begin(size_t quoteLevel)
{
	if (currentQuoteLevel != quoteLevel)
		flushlines();

	currentQuoteLevel=quoteLevel;

	size_t quoteOffset=0;

	if (quoteLevel)
		quoteOffset=1+quoteLevel;

	wrapper=unicodewordwrapper<CursesMessage::FlowedTextParser>
		(norewrapper, *this, quoteOffset + 20 > displayWidth
		 ? 20:displayWidth-quoteLevel, false);
	emitted_line=false;
}

void CursesMessage::FlowedTextParser::line_contents(const unicode_char *ptr,
						    size_t cnt)
{
	while (cnt)
	{
		wrapper=*ptr;
		++ptr;
		--cnt;
	}
}

void CursesMessage::FlowedTextParser::line_end()
{
	wrapper='\n';
	wrapper.eof();

	if (!emitted_line)
		operator=(std::vector<unicode_char>());
}

void CursesMessage::FlowedTextParser::operator=(const std::vector<unicode_char>
						&line)
{
	bool flag;

	emitted_line=true;
	std::string lineStr=(*currentDemoronizer)(line, flag);

	if (flag)
		conversionErrorFlag=true;

	wrapped_lines.push_back(lineStr);

	if (lineStr.size() == 0)
		flushlines();
}

bool CursesMessage::FlowedTextParser::finish()
{
	bool flag;

	end(flag);

	flushlines();

	if (flag)
		conversionErrorFlag=true;
	return true;
}

void CursesMessage::FlowedTextParser::flushlines()
{
	std::string prefix;

	textAttributes attr;

	if (currentQuoteLevel)
	{
		for (size_t i=0; i<currentQuoteLevel; ++i)
		{
			attr.fgcolor=
				quote_colors[ (i % (sizeof(quote_colors)/
						    sizeof(quote_colors[0])))]
				->fcolor;


			prefix += (std::string)attr;

			prefix += ">";
		}

		prefix += " ";
	}

	if (messagePtr)
	{
		messagePtr->reformatFindUrls(wrapped_lines, attr);

		for (std::vector<std::string>::iterator b(wrapped_lines.begin()),
			     e(wrapped_lines.end()); b != e; ++b)
			messagePtr->reformatAddLine(prefix + *b,
						    CursesMessage::LineIndex::ATTRIBUTES);
	}

	wrapped_lines.clear();
}
