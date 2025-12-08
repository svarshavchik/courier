/*
** Copyright 2002-2008, S. Varshavchik.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "rfc2047decode.H"
#include "rfc822/rfc822.h"
#include "rfc822/rfc2047.h"
#include "mail.H"
#include <cstring>
#include <string>
#include <iterator>

using namespace std;

mail::rfc2047::decoder::decoder()
{
}

mail::rfc2047::decoder::~decoder()
{
}

void mail::rfc2047::decoder::rfc2047_decode_callback(const char *text,
						    size_t text_len,
						    void *voidarg)
{
	((mail::rfc2047::decoder *)voidarg)->rfc2047_callback(text, text_len);
}

void mail::rfc2047::decoder::rfc2047_callback(const char *text, size_t text_len)
{
	decodedbuf.append(text, text+text_len);
}

string mail::rfc2047::decoder::decode(string text, string charset)
{
	decodedbuf.clear();

	auto iter=std::back_inserter(decodedbuf);
	rfc822::display_header("subject", text, charset, iter);

	return decodedbuf;
}


string mail::rfc2047::decoder::decodeSimple(string str)
{
	std::string output;

	bool error=false;
	::rfc2047::decode(str.begin(), str.end(),
			  [&]
			  (auto charset, auto language, auto closure)
			  {
				  auto iter=std::back_inserter(output);

				  closure(iter);
			  },
			  [&]
			  (auto b, auto e, auto ignore)
			  {
				  error=true;
			  });

	if (error)
		return str;

	return output;
}
