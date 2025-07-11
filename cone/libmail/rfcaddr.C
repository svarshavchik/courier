/*
** Copyright 2002-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include <string.h>
#include "rfcaddr.H"
#include "mail.H"
#include "misc.H"
#include "rfc2047decode.H"
#include "rfc822/rfc822.h"
#include <courier-unicode.h>

#include <iostream>
#include <algorithm>
#include "rfc822/rfc2047.h"

#if LIBIDN
#include <idn2.h>

namespace mail {
	class libidn_init {

	public:
		libidn_init();
		~libidn_init();
	};

	libidn_init libidn_init_instance;
}

mail::libidn_init::libidn_init()
{
}

mail::libidn_init::~libidn_init()
{
}
#endif

using namespace std;

mail::address::address(string n, string a): name(n), addr(a)
{
}

mail::address::~address()
{
}

string mail::address::toString() const
{
	if (addr.length() == 0)
		return name;

	string q=addr;

	// Only add < > if there's a name

	if (name.length() > 0)
	{
		q=" <" + q + ">";

		// If name is all alphabetics, don't bother with quotes.

		string::const_iterator b=name.begin(), e=name.end();

		while (b != e)
		{
			if (!strchr(" ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789?*+-/=_",
				    *b))
			{
				string s="\"";

				b=name.begin(), e=name.end();

				while (b != e)
				{
					if (strchr("\\\"()<>", *b))
						s += "\\";
					// These chars must be escaped.

					s.append(&*b, 1);
					b++;
				}
				return s + "\"" + q;
			}
			b++;
		}
		q=name + q;
	}
	return q;
}

template<class T>
string mail::address::toString(string hdr, const vector<T> &h,
			       size_t w)
{
	int len=hdr.length();
	string comma="";
	bool first=true;

	string spc="";

	spc.insert(spc.end(), len < 20 ? len:20, ' ');
	// Leading whitespace for folded lines.

	typename std::vector<T>::const_iterator b=h.begin(), e=h.end();

	while (b != e)
	{
		string s= b->toString();

		if (!first && len + comma.length() + s.length() > w)
		{
			// Line too long, wrap it.

			if (comma.size() > 0)
				comma=",";

			hdr=hdr + comma + "\n" + spc;
			len=spc.size();
			comma="";
		}
		first=false;
		hdr = hdr + comma + s;

		len += comma.length() + s.length();
		comma=", ";

		if (b->addr.length() == 0)
			comma="";
		b++;
	}
	return hdr;
}

template
string mail::address::toString(string hdr, const vector<mail::address> &h,
			       size_t w);

template
string mail::address::toString(string hdr,
			       const vector<mail::emailAddress> &h,
			       size_t w);

string mail::address::getCanonAddress() const
{
	string a=addr;

	size_t n=a.find_last_of('@');

	if (n < a.size())
	{
		a=a.substr(0, n) +
			unicode::iconvert::convert_tocase(a.substr(n),
						       "iso-8859-1",
						       unicode_lc);
	}

	return a;
}

bool mail::address::operator==(const mail::address &o) const
{
	return getCanonAddress() == o.getCanonAddress();
}

template<class T> bool mail::address::fromString(string addresses,
						 vector<T> &h,
						 size_t &errIndex)
{
	errIndex=std::string::npos;

	rfc822::tokens tokens{
		addresses,
		[&]
		(size_t n)
		{
			errIndex=n;
		}};

	rfc822::addresses email_addresses{tokens};

	for (auto &a:email_addresses)
	{
		std::string name, addr;

		a.unquote_name(std::back_inserter(name));
		a.address.print(std::back_inserter(addr));
		h.push_back( mail::address(name, addr));
	}
	return true;
}

template
bool mail::address::fromString(string addresses,
			       vector<mail::address> &h,
			       size_t &errIndex);

template
bool mail::address::fromString(string addresses,
			       vector<mail::emailAddress> &h,
			       size_t &errIndex);


void mail::address::setName(std::string s)
{
	name=s;
}

void mail::address::setAddr(std::string s)
{
	addr=s;
}

/////////////////////////////////////////////////////////////////////////

mail::emailAddress::emailAddress() : mail::address("", "")
{
}

mail::emailAddress::~emailAddress()
{
}

mail::emailAddress::emailAddress(const mail::address &a)
	: mail::address(a.getName(), a.getAddr())
{
	decode();
}

std::string mail::emailAddress::setDisplayName(const std::string &s,
					       const std::string &charset)
{
	std::u32string ucbuf;

	if (!unicode::iconvert::convert(s, charset, ucbuf))
		return "Encoding error";

	decodedName=ucbuf;

	name=::rfc2047::encode(s, charset, rfc2047_qp_allow_word).first;

	return "";
}

std::string mail::emailAddress::setDisplayAddr(const std::string &s,
					       const std::string &charset)
{
	std::u32string ucbuf;

	if (!unicode::iconvert::convert(s, charset, ucbuf))
		return "Encoding error";

#if LIBIDN
	std::u32string::iterator b, e;

	for (b=ucbuf.begin(), e=ucbuf.end(); b != e; ++b)
	{
		if (*b == '@')
		{
			++b;
			break;
		}
	}

	size_t addr_start= b-ucbuf.begin();

	// Assume non-latin usernames are simply UTF-8 */

	std::string name_portion=
		unicode::iconvert::convert(std::u32string(ucbuf.begin(),
								  b),
					"utf-8");

	ucbuf.push_back(0);

	char *ascii_ptr;

	int rc=idn2_to_ascii_4z(reinterpret_cast<const uint32_t *>
				(&ucbuf[addr_start]), &ascii_ptr, 0);

	if (rc != IDNA_SUCCESS)
		return std::string(std::string("Address encoding error: ") +
				   idna_strerror((Idna_rc)rc));

	try {
		name_portion += ascii_ptr;
		free(ascii_ptr);
	} catch (...) {
		free(ascii_ptr);
		throw;
	}

	ucbuf.pop_back();
	addr=name_portion;
#else
	std::u32string::iterator b, e;

	for (b=ucbuf.begin(), e=ucbuf.end(); b != e; ++b)
	{
		if (*b < ' ' || *b > 0x7F)
			return "Internationalized addresses not supported";
	}

	addr.clear();
	addr.reserve(ucbuf.size());
	addr.insert(addr.end(), ucbuf.begin(), ucbuf.end());

#endif
	decodedAddr=ucbuf;
	return "";
}

void mail::emailAddress::setName(string s)
{
	mail::address::setName(s);
	decode();
}

void mail::emailAddress::setAddr(string s)
{
	mail::address::setAddr(s);
	decode();
}

void mail::emailAddress::decode()
{
	std::u32string ucname, ucaddr;

	unicode::iconvert::convert(mail::rfc2047::decoder().decode(name,
								"utf-8"),
				"utf-8", ucname);

	ucaddr.reserve(addr.size());

	unicode::iconvert::convert(addr, "utf-8", ucaddr);

#if LIBIDN
	size_t at=std::find(ucaddr.begin(), ucaddr.end(), '@')
		- ucaddr.begin();

	if (at != ucaddr.size())
	{
		++at;
		ucaddr.push_back(0);

		uint32_t *ucbuf;

		int rc=idn2_to_unicode_4z4z(reinterpret_cast
					    <const uint32_t *>(&ucaddr[at]),
					    &ucbuf, 0);

		if (rc == IDNA_SUCCESS)
		{
			size_t i;

			for (i=0; ucbuf[i]; ++i)
				;

			try {
				ucaddr.erase(ucaddr.begin()+at,
					     ucaddr.end());
				ucaddr.insert(ucaddr.end(),
					      ucbuf, ucbuf+i);
				free(ucbuf);
			} catch (...) {
				free(ucbuf);
				throw;
			}
		}
	}
#endif
	decodedName=ucname;
	decodedAddr=ucaddr;
}
