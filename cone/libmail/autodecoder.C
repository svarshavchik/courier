/*
** Copyright 2002-2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "misc.H"
#include "autodecoder.H"

using namespace std;

mail::autodecoder::base64::base64(mail::autodecoder &meArg)
	: me(meArg)
{
}

mail::autodecoder::base64::~base64()
{
}

void mail::autodecoder::base64::decoded(string s)
{
	me.decoded(s);
}

mail::autodecoder::qp::qp(mail::autodecoder &meArg)
	: me(meArg)
{
}

mail::autodecoder::qp::~qp()
{
}

void mail::autodecoder::qp::decoded(string s)
{
	me.decoded(s);
}

//////////////////////////////////////////////////////////////////////

mail::autodecoder::autodecoder(string cte)
	: base64Decoder(*this),
	  qpDecoder(*this),
	  decoder(NULL)
{
	mail::upper(cte);

	if (cte == "QUOTED-PRINTABLE")
		decoder= &qpDecoder;

	if (cte == "BASE64")
		decoder= &base64Decoder;
}

mail::autodecoder::~autodecoder()
{
}

void mail::autodecoder::decode(string s)
{
	if (decoder)
		decoder->decode(s);	// 7bit or 8bit, or something...
	else
		decoded(s);
}

