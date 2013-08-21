/*
** Copyright 2002-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "rfc2047decode.H"
#include "rfc822/rfc822.h"
#include "rfc822/rfc2047.h"
#include "mail.H"
#include <cstring>

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

	if (rfc822_display_hdrvalue("subject",
				    text.c_str(),
				    charset.c_str(),
				    &mail::rfc2047::decoder::
				    rfc2047_decode_callback,
				    NULL,
				    this) < 0)
		return text;

	return decodedbuf;
}

void mail::rfc2047::decoder::decode(vector<mail::address> &addr_cpy,
				    std::string charset)
{
	vector<mail::address>::iterator b=addr_cpy.begin(), e=addr_cpy.end();

	while (b != e)
	{
		b->setName(decode(b->getName(), charset));
		b++;
	}
}

string mail::rfc2047::decoder::decodeSimple(string str)
{
	std::string output;

	if (rfc2047_decoder(str.c_str(), &decodeSimpleCallback, &output) < 0)
		return str;

	return output;
}

void mail::rfc2047::decoder::decodeSimpleCallback(const char *chset,
						  const char *lang,
						  const char *content,
						  size_t cnt,
						  void *dummy)
{
	((std::string *)dummy)->append(content, content+cnt);
}
