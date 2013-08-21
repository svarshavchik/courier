/*
** Copyright 2002-2009, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"

#include "qp.H"

#include <string.h>

using namespace std;

static inline int xdigit(char c)
{
	static const char xdigits[]="0123456789ABCDEFabcdef";

	const char *p=strchr(xdigits, c);

	if (p == NULL)
		return -1;

	int n= p - xdigits;

	if (n >= 16)
		n -= 6;
	return n;
}

mail::decodeqp::decodeqp()
	: decodeBuffer("")
{
}

mail::decodeqp::~decodeqp()
{
}

void mail::decodeqp::decode(string text)
{
	string decodedString="";

	text=decodeBuffer + text;

	// Just decode as much as possible, and leave the rest for next time.

	string::iterator b=text.begin(), e=text.end(), c=b;

	while (b != e)
	{
		// Look for the next =

		for (c=b; c != e; c++)
			if (*c == '=')
				break;

		decodedString += string(b, c);

		if (c == e) // Got everything.
		{
			b=c;
			break;
		}

		b=c++;

		if (c == e)
			break;

		if (*c == '\r')
			c++;

		if (c == e)
			break;

		if (*c == '\n')
		{
			c++;
			b=c;
			continue;
		}

		int a1=xdigit(*c++);

		if (a1 < 0)
		{
			b=c;
			continue;
		}

		if (c == e)
			break;

		int a2=xdigit(*c++);

		if (a2 < 0)
		{
			b=c;
			continue;
		}

		decodedString += (char)(a1 * 16 + a2);

		b=c;
	}

	decodeBuffer=string(b, e); // Leftovers

	decoded(decodedString);
}

#if 0

class testqp : public mail::decodeqp {

	void decoded(string buffer)
	{
		cout << buffer;
	}
};

int main(int argc, char **argv)
{
	testqp qp;

	int argn;

	for (argn=1; argn < argc; argn++)
	{
		string s=argv[argn];

		size_t p;

		while ((p=s.find('~')) != std::string::npos)
			s[p]='\n';

		qp.decode(s);
	}

	cout << endl;

	return (0);
}
#endif
