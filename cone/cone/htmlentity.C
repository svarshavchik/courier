#include "htmlentity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <courier-unicode.h>


struct unicodeEntity {
	char32_t iso10646;
	const char *alt;	/* Not null - plaintext alternative */
};

const struct unicodeEntity unicodeEntityList[] = {
	{162, "[cent]"},/* cent sign */
	{163, "[lb]"},	/* pound sign */
	{165, "[yen]"},	/* yen sign = yuan sign */
	{166, "|"},	/* broken bar = broken vertical bar, */
	{169, "(C)"},	/* copyright sign */
	{171, "<<"},	/* left-pointing double angle quotation mark */
	{174, "(R)"},	/* registered sign = registered trade mark sign, */
	{177, "+/-"},	/* plus-minus sign = plus-or-minus sign, */
	{180, "'"},	/* acute accent = spacing acute, */
	{187, ">>"},	/* right-pointing double angle quotation mark */
	{188, "1/4"},	/* vulgar fraction one quarter */
	{189, "1/2"},	/* vulgar fraction one half */
	{190, "3/4"},	/* vulgar fraction three quarters */
	{215, "X"},	/* multiplication sign */
	{247, "/"},	/* division sign */

	{8226, "*"},	/* bullet = black small circle, */

	{8230, "..."},	/* horizontal ellipsis = three dot leader, */
	{8242, "'"},	/* prime = minutes = feet */
	{8243, "''"},	/* double prime = seconds = inches, */
	{8260, "/"},	/* fraction slash */

	{8482, "[tm]"},	/* trade mark sign */
	{8592, "<-"},	/* leftwards arrow */
	{8594, "->"},	/* rightwards arrow */
	{8596, "<->"},	/* left right arrow */
	{8656, "<="},	/* leftwards double arrow */
	{8658, "=>"},	/* rightwards double arrow, */
	{8660, "<=>"},	/* left right double arrow, */

	{8722, "-"},	/* minus sign */
	{8727, "*"},	/* asterisk operator */
	{8800, "<>"},	/* not equal to */
	{8801, "=="},	/* identical to */
	{8804, "<="},	/* less-than or equal to */
	{8805, ">="},	/* greater-than or equal to, */
	{8853, "(+)"},	/* circled plus = direct sum, */
	{8855, "(x)"},	/* circled times = vector product, */
	{8901, "."},	/* dot operator */

	{710, "^"},	/* modifier letter circumflex accent, */
	{732, "~"},	/* small tilde */
	{8211, "--"},	/* en dash */
	{8212, "---"},	/* em dash */
	{8216, "`"},	/* left single quotation mark, */
	{8217, "'"},	/* right single quotation mark, */
	{8218, ","},	/* single low-9 quotation mark */
	{8220, "``"},	/* left double quotation mark, */
	{8221, "''"},	/* right double quotation mark, */
	{8222, ",,"},	/* double low-9 quotation mark */
	{8224, "+"},	/* dagger */
	{8225, "++"},	/* double dagger */
	{8240, "0/00"},	/* per mille sign */
	{8249, "<"},	/* single left-pointing angle quotation mark, */
	{8250, ">"},	/* single right-pointing angle quotation mark, */
	{8364, "C="},	/* euro sign */

	{0, 0}
};

Demoronize::Demoronize(const std::string &chsetArg,
		       demoron_t dodemoronize) : chset(chsetArg)
{
	size_t i;

	if (dodemoronize == none)
		return;

	for (i=0; unicodeEntityList[i].iso10646; i++)
	{
		if (chsetArg.size() > 0)
		{
			std::u32string uc;
			bool errflag;

			uc.push_back(unicodeEntityList[i].iso10646);

			std::string s(unicode::iconvert::convert(uc, chsetArg,
							      errflag));

			if (dodemoronize == maximum)
				s.clear();

			if (s.size() > 0 && !errflag)
				continue;
		}

		altlist.insert(std::make_pair(unicodeEntityList[i].iso10646,
					      unicodeEntityList[i]));
	}
}

Demoronize::~Demoronize()
{
}

std::string Demoronize::alt(char32_t ch) const
{
	std::string s;

	std::map<char32_t, unicodeEntity>::const_iterator
		iter(altlist.find(ch));

	if (iter != altlist.end() && iter->second.alt)
		s=iter->second.alt;

	return s;
}

std::string Demoronize::operator()(const std::u32string &uc,
				   bool &err)
{
	std::u32string buffer;

	return unicode::iconvert::convert(expand(uc, buffer), chset, err);
}

const std::u32string &
Demoronize::expand(const std::u32string &uc,
		   std::u32string &newbuffer)
{
	std::u32string::const_iterator b, e;
	size_t cnt=uc.size();
	bool found=false;

	for (b=uc.begin(), e=uc.end(); b != e; ++b)
	{
		std::map<char32_t, unicodeEntity>::const_iterator
			iter(altlist.find(*b));

		if (iter != altlist.end() && iter->second.alt)
		{
			cnt += strlen(iter->second.alt);
			found=true;
		}
	}

	if (!found)
		return uc;

	newbuffer.reserve(cnt*2);

	for (b=uc.begin(), e=uc.end(); b != e; ++b)
	{
		std::map<char32_t, unicodeEntity>::const_iterator
			iter(altlist.find(*b));

		if (iter != altlist.end() && iter->second.alt)
		{
			newbuffer.insert(newbuffer.end(),
					 iter->second.alt,
					 iter->second.alt+
					 strlen(iter->second.alt));
			continue;
		}
		newbuffer.push_back(*b);
	}

	return newbuffer;
}

	
