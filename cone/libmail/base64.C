/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "base64.H"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace std;

// base64 decoding table.

static const unsigned char decode64tab[256]={
	100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
	100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
	100,100,100,100,100,100,100,100,100,100,100,62,100,100,100,63,
	52,53,54,55,56,57,58,59,60,61,100,100,100,99,100,100,
	100,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,
	15,16,17,18,19,20,21,22,23,24,25,100,100,100,100,100,
	100,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
	41,42,43,44,45,46,47,48,49,50,51,100,100,100,100,100,
	100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
	100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
	100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
	100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
	100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
	100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
	100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
	100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100};


mail::decodebase64::decodebase64()
{
	decodeBuffer="";
}

mail::decodebase64::~decodebase64()
{
}

void mail::decodebase64::decode(string text)
{
	char workbuf[BUFSIZ];

	// Toss out the crud (newlines, spaces, etc..)

	string::iterator b=text.begin(), e=text.end();
	string::iterator c=b;

	while (b != e)
	{
		if (decode64tab[(int)(unsigned char)*b] == 100)
		{
			// Ignore non-base64 characters.
			b++;
			continue;
		}
		*c++=*b++;
	}

	// Something might be left over from the previous run.  Tis ok.

	b=text.begin();

	if (c != b)
		decodeBuffer.append(b, c);


	size_t i=decodeBuffer.length()/4; // The remainder gets left over.

	char    aa,bb,cc;

	b=decodeBuffer.begin();

	size_t k=0;

	string decodedbuf="";

	while (i)
	{
		unsigned char d=*b++;
		unsigned char e=*b++;
		unsigned char f=*b++;
		unsigned char g=*b++;

		int     w=decode64tab[d];
		int     x=decode64tab[e];
		int     y=decode64tab[f];
		int     z=decode64tab[g];

		aa= (w << 2) | (x >> 4);
		bb= (x << 4) | (y >> 2);
		cc= (y << 6) | z;

		workbuf[k++]=aa;
		if ( f != '=')
			workbuf[k++]=bb;
		if ( g != '=')
			workbuf[k++]=cc;

		if (k >= sizeof(workbuf)-10)
		{
			decodedbuf.append(workbuf, k);
			k=0;
		}
		--i;
	}
	if (k > 0)
		decodedbuf.append(workbuf, k);

	string left="";
	left.append(b, decodeBuffer.end());
	decodeBuffer=left;

	decoded(decodedbuf);
}

//
// The encoder does NOT insert newline breaks.  That's the superclass's job.

mail::encodebase64::encodebase64()
{
	libmail_encode_start(&encodeInfo, "base64", &callback_func, this);
}

mail::encodebase64::~encodebase64()
{
}

void mail::encodebase64::encode(string text)
{
	libmail_encode(&encodeInfo, &text[0], text.size());
}

void mail::encodebase64::flush()
{
	libmail_encode_end(&encodeInfo);
}

int mail::encodebase64::callback_func(const char *ptr, size_t len, void *vp)
{
	string s=string(ptr, ptr+len);
	size_t n;

	while ((n=s.find('\n')) != std::string::npos)
		s.erase(s.begin()+n);

	((mail::encodebase64 *)vp)->encoded(s);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////
//
// The convenience classes.

mail::decodebase64str::decodebase64str()
	: challengeStr("")
{
}

mail::decodebase64str::decodebase64str(string s)
	: challengeStr("")
{
	decode(s);
}

mail::decodebase64str::~decodebase64str()
{
}

void mail::decodebase64str::decoded(string s)
{
	challengeStr += s;
}

mail::encodebase64str::encodebase64str() : responseStr("")
{
}

mail::encodebase64str::encodebase64str(string s)
	: responseStr("")
{
	encode(s);
	flush();
}

mail::encodebase64str::~encodebase64str()
{
}

void mail::encodebase64str::encoded(string s)
{
	responseStr += s;
}

