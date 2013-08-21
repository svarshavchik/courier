/*
** Copyright 2003-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "htmlparser.H"
#include "htmlentity.h"
#include "unicode/unicode.h"
#include <iomanip>

/*
** Regression tests for the HTML parser.
*/

class myParser: public Demoronize,
		public htmlParser {

	void parsedLine(std::string l, bool dummy);
public:
	myParser();
	~myParser();
};

myParser::myParser() : Demoronize("iso-8859-1", Demoronize::none),
		       htmlParser("iso-8859-1", "iso-8859-1",
				  static_cast<Demoronize &>(*this))
{
}

myParser::~myParser()
{
}

void myParser::parsedLine(std::string l, bool dummy)
{
	std::vector<std::pair<textAttributes, std::string> > parsedLine;

	textAttributes::getAttributedText(l, parsedLine);

	std::vector<std::pair<textAttributes, std::string> >::iterator b, e;

	b=parsedLine.begin();
	e=parsedLine.end();

	while (b != e)
	{
		std::cout << b->second;
		b++;
	}

	std::cout << std::endl;
}

int main(int argc, char *argv[])
{
#if 0
	if (argc > 1 && strcmp(argv[1], "entities") == 0)
	{
		size_t i;
		for (i=0; unicodeEntityList[i].name; i++)
		{
			std::string s=unicodeEntityList[i].name;

			s += ":";

			size_t l=s.size();

			while (l < 16)
			{
				s += "&nbsp;";
				++l;
			}

			std::cout << s << "&" << unicodeEntityList[i].name
			     << ";&nbsp;(" << unicodeEntityList[i].iso10646
			     << ")<BR>" << std::endl;
		}

		exit(0);
	}

	if (argc > 1 && strcmp(argv[1], "utf8") == 0)
	{
		size_t i;
		for (i=0; unicodeEntityList[i].name; i++)
		{
			std::string s=unicodeEntityList[i].name;

			s += ":";

			size_t l=s.size();

			while (l < 16)
			{
				s += " ";
				++l;
			}

			std::cout << s;

			std::vector<unicode_char> uc;

			uc.push_back(unicodeEntityList[i].iso10646);

			std::string p=
				mail::iconvert::convert(uc, "utf-8");

			if (p.size())
			{
				std::cout << p;
			}
			else std::cout << " ";

			std::cout << " (" << std::hex << std::setw(4)
				  << std::setfill('0')
				  << unicodeEntityList[i].iso10646
				  << ")" << std::endl;
		}

		exit(0);
	}
#endif
	myParser p;

	char buffer[8192];

	while (std::cin.read(buffer, sizeof(buffer)).gcount() > 0)
	{
		p.parse(std::string(buffer, buffer + std::cin.gcount()));
	}

	p.flush();
	return 0;
}
