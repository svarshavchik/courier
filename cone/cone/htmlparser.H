/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef htmlparser_H
#define htmlparser_H

#include "config.h"

#include "htmlentity.h"

#include <string>
#include <cstring>
#include <vector>
#include <list>
#include <map>

#include <iostream>
#include "unicode/unicode.h"

/////////////////////////////////////////////////////////////////////////
//
// This class converts HTML to flowed-format plain text.


// ... with some extra jazz, namely rendering attributes.  In the converted
// plain text format there will be embedded escape sequences that specify
// attributes for the following text.  The attributes are reset at the start
// of each line; each line begins with unattributed text.
//
// The textAttributes() object can be converted to/from string, which is
// the escape sequence.
//
// The escape sequnce is \x7Fzzzz\x7F, where zzz specify the encoded
// attributes.  The converted HTML text never uses \x7Fs, so there's no
// ambiguity.

class textAttributes {  // Parsed character attributes
public:
	unsigned bold:1;
	unsigned underline:1;
	unsigned reverse:1;
	unsigned italic:1;
	unsigned bgcolor;
	unsigned fgcolor;

	std::string url; // A URL is treated as an attribute

	textAttributes();
	textAttributes(std::string);
	~textAttributes();

	bool operator==(const textAttributes &a) const;
	bool operator!=(const textAttributes &a) const;
	operator std::string() const;

	// Convert text with embedded attribute escape codes to an array
	// of attribute/text pairs.

	static void getAttributedText(std::string,
				      std::vector< std::pair<textAttributes,
				      std::string> > &);
};

class htmlParser {

	class enhanced_char {
	public:
		unicode_char ch;
		textAttributes attr;

		enhanced_char(unicode_char chArg, textAttributes attrArg);
		~enhanced_char();
	};

	//
	// Assign linebreaking values to a unicode_char and a textAttribute.
	// Saves them in a list before using linebreak_callback_base. When
	// a linebreaking value comes back from linebreak_callback_base,
	// pop the pair off the list, and there you go.

	class linebuf_calc_linebreak : public mail::linebreak_callback_base {

		std::list< std::pair<unicode_char, textAttributes > > queue;

	public:
		void submit(unicode_char ch, textAttributes attr)
		{
			queue.push_back(std::make_pair(ch, attr));
			linebreak_callback_base::operator<<(ch);
		}

	private:
		// Override in subclass, to receive the linebreak values.

		virtual void operator()(int, unicode_char, textAttributes)=0;

		virtual int operator()(int v)
		{
			std::pair<unicode_char, textAttributes>
				&p=queue.front();

			unicode_char ch=p.first;
			textAttributes attr=p.second;
			queue.pop_front();
			operator()(v, ch, attr);
			return 0;
		}
	};

	class linebuf : private linebuf_calc_linebreak {

		std::vector<enhanced_char> line; // Current line being built.

		size_t lineBreak; // Index of start of current word

		size_t width_beforebreak;
		// Width of the line before lineBreak

		size_t startingCol;
		// This line starts on this column.

		size_t desiredWidth;
		// Desired line width

		// My parent class
		htmlParser *parser;

		bool isempty;

		unicode_char lastchar;

		size_t wantthiswidth();
	public:
		class towidth_iter;

		linebuf(htmlParser *parserArg, size_t desiredWidthArg);
		~linebuf();

		void submit(unicode_char ch, textAttributes attr)
		{
			isempty=false;
			lastchar=ch;
			linebuf_calc_linebreak::submit(ch, attr);
		}

		unicode_char getLastChar()
		{
			return lastchar;
		}

		void flush();

		bool empty()
		{
			return isempty;
		}

		void setDesiredWidth(size_t startingColArg,
				     size_t desiredWidthArg)
		{
			startingCol=startingColArg;
			desiredWidth=desiredWidthArg;
		}

	private:
		virtual void operator()(int, unicode_char, textAttributes);

		size_t wordtoolong(size_t);

		void linebreak_allowed();
	};

	linebuf currentLine;

	class converter : public mail::iconvert::tou {

		htmlParser *parser;

	public:
		converter(htmlParser *parserArg);
		~converter();
		using mail::iconvert::tou::operator();

	private:
		int converted(const unicode_char *, size_t);
	};

	converter tounicode;

	std::vector<unicode_char> leadin_prefix; // Used by <LI>

	size_t desiredLineLength; // Wanted line length.

	char inTag;
	// 0 - eating CDATA, or '<' -- eating tag,
	// or '&' -- eating ext character.

	size_t nblanklines;

	std::vector<unicode_char> currentTag; // Current tag being eaten.

	void newTag();

	// Various tags we recognize, they get pushed on a stack as we
	// see them.  When closing a tag, priority figures out which
	// unclosed tags are popped off.

	struct taginfo {
		const char *name;
		int priority;
		const char *options; // Describes the tag, defined elsewhere.
	};


	static const struct taginfo BLOCKQUOTE, H1, H2, H3, H4, H5, H6, H7, H8,
		H9, P, STYLE, SCRIPT, TABLE, TR, PRE, A, B, U, I;

	static const struct taginfo * const knownTags[];

	class tag {
	public:
		const struct taginfo *name;
		std::list< std::pair<std::string,
				     std::vector<unicode_char> > > attributes;
		// Attributes on a tag.

		textAttributes attr; // Cached current text attrs.
		bool preformatted; // This is a PRE, or a derivative.

		tag();
		~tag();
		void getAttribute(std::string, std::vector<unicode_char> &);
		// Search for an attribute in attributes, and return the
		// attribute's value.


	};

	// Utility functions:

	// Skip whispace in b, up to e.
	static void skipspc(std::vector<unicode_char>::iterator &b,
			    std::vector<unicode_char>::iterator &e);

	static bool isb(unicode_char); // True if this is a blank char.
	static bool isu(unicode_char); // True if char is latin uppercase.

	static void tou(std::string::iterator, std::string::iterator);
	// Convert char string to uppercase.

	static void tou(std::vector<unicode_char>::iterator,
			std::vector<unicode_char>::iterator);
	// Ditto.

	std::list<tag> tagStack; // Current stack of open tags.

	void closetag(std::list<tag>::iterator);
	void opentag(std::string, tag &);
	void updateTagStyle(tag &, std::string);

	bool paragraphBreak; // Paragraph break just seen.
	void newParagraph();

	size_t getLinePrefixWidth(); // Compute indentation level of this line.
	size_t getLineLength(); // Compute desired line length, for this line.

	std::string htmlCharset; // HTML charset
	std::string txtCharset; // Convert to this charset.

	Demoronize &demoronizer; // HTML entity aliases.

	static unicode_char getHtmlEntity(std::string entityName);

protected:
	bool conversionError();
private:
	bool conversionErrorFlag;

public:
	static void urlDecode(std::string url, std::vector<unicode_char> &uc);

	static void cgiDecode(const std::vector<unicode_char> &uc,
			      std::map<std::string,
			      std::vector<unicode_char> > &args);
	htmlParser(const std::string &htmlCharsetArg,
		   // HTML character set

		   const std::string &txtCharsetArg,
		   // Our character set.  This class does the necessary
		   // translation

		   Demoronize &demoronizerArg,
		   // Alternate representation of characters for
		   // txtCharset (smart quotes, and other ms-crap)

		   size_t desiredLineLengthArg=79);

	virtual ~htmlParser();

	virtual void parsedLine(std::string Line, bool wrapped)=0;
	//
	// The subclass should define parsedLine().
	// wrapped: true if this line is wrapped text (false - last line in
	// the paragraph).
	//

	void parse(std::string); // Feed HTML to here.

	void flush();

	friend class linebuf;
	friend class converter;
private:
	void fmtline(const enhanced_char *b, const enhanced_char *e, bool);

	void parse(const std::vector<unicode_char> &vec);
};

#endif
