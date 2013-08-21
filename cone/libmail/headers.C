/*
** Copyright 2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "rfcaddr.H"
#include "headers.H"
#include "rfc2047encode.H"
#include "rfc2047decode.H"
#include "rfc2045/rfc2045.h"
#include <errno.h>

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

	return mail::rfc2047::encode(text, charset);
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
	: mail::Header(name)
{
}

mail::Header::mime::mime(string name, string valueArg)
	: mail::Header(name), value(valueArg)
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

	header=header.substr(n);

	rfc2045_parse_mime_header(header.c_str(), cb_type, cb_param, &m);
	return m;
}

void mail::Header::mime::cb_type(const char *t, void *void_arg)
{
	mail::Header::mime *a=(mail::Header::mime *)void_arg;

	a->value=t;
	mail::upper(a->value);
}

void mail::Header::mime::cb_param(const char *name,
				  const char *value,
				  void *void_arg)
{
	mail::Header::mime *a=(mail::Header::mime *)void_arg;

	string n=name;

	mail::upper(n);

	if (!a->parameters.exists(name))
		a->parameters.set_simple(name, value);
}

mail::Header::mime::~mime()
{
}

string mail::Header::mime::toString() const
{
	string h=name + ": " + value;
	size_t init_offset=name.size()+2;

	const_parameter_iterator b=begin();
	const_parameter_iterator e=end();

	size_t offset=h.size();

	while (b != e)
	{
		string w=b->first;
		string s=b->second;

		string::iterator sb=s.begin(), se=s.end();

		if (sb != se)
			w += "=";

		while (sb != se)
		{
			if (!strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789?*+-/=_", *sb))
				break;
			++sb;
		}

		if (sb == se)
		{
			w += s;
		}
		else
		{
			w += "\"";

			string::iterator p=s.begin();

			for (sb=p; sb != se; sb++)
			{
				if (*sb == '\\' || *sb == '"')
				{
					w += string(p, sb) + "\\";
					w += *sb;
					p=sb+1;
				}
			}
			w += string(p, sb);
			w += "\"";
		}

		if (offset + w.size() > 74)
		{
			h += ";\n";
			h.insert(h.end(), init_offset, ' ');
			offset=init_offset;
		}
		else
		{
			h += "; ";
			offset += 2;
		}
		h += w;
		offset += w.size();
		++b;
	}
	return h;
}

mail::Header *mail::Header::mime::clone() const
{
	mail::Header::mime *c=new mime(name, value);

	if (!c)
		throw strerror(errno);
	c->parameters=parameters;
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
