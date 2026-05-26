/*
** Copyright 2004, S. Varshavchik.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "rfcaddr.H"
#include "headers.H"
#include "rfc2045/rfc2045.h"
#include "rfc822/rfc2047.h"
#include <errno.h>
#include <fstream>

using namespace std;

mail::Header::~Header()
{
}

string mail::Header::wrap(string s) const
{
	string ws=name + ": ";
	size_t init_offset=ws.size();
	size_t offset=init_offset;
	string::iterator b=s.begin(), e=s.end();
	bool first=true;

	while (b != e)
	{
		string::iterator c=b;

		while (c != e)
		{
			if (*c != ' ' && *c != '\t')
				break;
			++c;
		}

		while (c != e)
		{
			if (*c == ' ' || *c == '\t')
				break;
			++c;
		}

		if (c-b + offset > 76 && !first)
		{
			while (b != c && (*b == ' ' || *b == '\t'))
				++b;
			if (b == e)
				break;

			ws += "\n";
			ws.insert(ws.end(), init_offset, ' ');
			offset=init_offset;
		}
		ws.insert(ws.end(), b, c);
		offset += c-b;
		b=c;
		first=false;
	}
	return ws;
}

mail::Header::plain::~plain()
{
}

mail::Header *mail::Header::plain::clone() const
{
	mail::Header::plain *c=new plain(name, text);

	if (!c)
		throw strerror(errno);
	return c;
}

string mail::Header::plain::toString() const
{
	return wrap(text);
}

string mail::Header::encoded::encode(string text, string charset, string lang)
{
	if (lang.size() > 0)
		charset += "*" + lang;

	return ::rfc2047::encode(text, charset, rfc2047_qp_allow_any).first;
}

mail::Header::encoded::encoded(string name,
			       string text, string charset, string lang)
	: Header::plain(name, encode(text, charset, lang))
{
}

mail::Header::encoded::~encoded()
{
}

mail::Header::addresslist::addresslist(string name)
	: mail::Header(name)
{
}

mail::Header::addresslist::addresslist(string name,
				       const std::vector<mail::emailAddress>
				       &addressesArg)
	: mail::Header(name), addresses(addressesArg)
{
}

mail::Header::addresslist::~addresslist()
{
}

mail::Header *mail::Header::addresslist::clone() const
{
	mail::Header::addresslist *c=new addresslist(name, addresses);

	if (!c)
		throw strerror(errno);
	return c;
}

std::string mail::Header::addresslist::toString() const
{
	return mail::address::toString(name + ": ",
				       addresses);
}

mail::Header::mime::mime(string name)
	: mail::Header(name), header{"", true}
{
}

mail::Header::mime::mime(string name, string valueArg)
	: mail::Header(name), header{valueArg, true}
{
}

mail::Header::mime mail::Header::mime::fromString(string header)
{
	size_t n=header.find(':');

	if (n != std::string::npos)
		++n;
	else
		n=header.size();

	mime m(header.substr(0, n));

	m.header=rfc2231::header{header.substr(n), true};
	return m;
}

mail::Header::mime::~mime()
{
}

string mail::Header::mime::toString() const
{
	string h=name;

	char prevch=0;
	for (char &c:h)
	{
		if ((prevch < 'a' || prevch > 'z') &&
		    (c >= 'a' && c <= 'z'))
		{
			c += 'A'-'a';
		}

		prevch=c;
		if (prevch >= 'A' && prevch <= 'Z')
			prevch += 'a'-'A';
	}

	h += ": " + header.value;

	const char *sep="; ";

	for (auto &[name, value] : header.parameters)
	{
		rfc2231::attr_encode(
			name,
			value.value,
			value.charset,
			value.language,
			[&]
			(const char *param, const char *value)
			{
				h += sep;
				sep=";\n  ";
				h += param;
				h += "=";
				h += value;
			}
		);
	}

	return h;
}

mail::Header *mail::Header::mime::clone() const
{
	mail::Header::mime *c=new mime(name, header.value);

	if (!c)
		throw strerror(errno);
	c->header=header;
	return c;
}

mail::Header::listitem::listitem(const listitem &hArg)
	: h(hArg.h->clone())
{
}

mail::Header::listitem::listitem(const mail::Header &hArg)
	: h(hArg.clone())
{
}

mail::Header::listitem::~listitem()
{
	delete h;
}

mail::Header::listitem &mail::Header::listitem::operator=(const listitem &hArg)
{
	Header *p=hArg.h->clone();

	delete h;
	h=p;
	return *this;
}

mail::Header::listitem &mail::Header::listitem::operator=(const Header &hArg)
{
	Header *p=hArg.clone();

	delete h;
	h=p;
	return *this;
}

mail::Header::list::list()
{
}

mail::Header::list::~list()
{
}

mail::Header::list::operator string() const
{
	string h;

	const_iterator b=begin();
	const_iterator e=end();

	while (b != e)
	{
		const Header &hh= *b;
		h += hh.toString();
		h += "\n";
		++b;
	}
	return h;
}
