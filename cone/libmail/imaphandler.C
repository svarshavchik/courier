/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "imap.H"
#include "imaphandler.H"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

using namespace std;

//////////////////////////////////////////////////////////////////////////
//
// Tokenize server response
//

int mail::imapHandlerStructured::process(mail::imap &imapAccount, string &buffer)
{
	string::iterator b=buffer.begin();
	string::iterator e=buffer.end();
	int cnt=0;

	errbuf=buffer;

	if (stringmode)	// Collecting a string
	{
		string s="";

		while (b != e)
		{
			if (*b == '"')
			{
				stringmode=0;
				largestringsize=s.size();
				process(imapAccount, Token(STRING, s));
				return cnt+1;
			}

			if (*b == '\\')
			{
				b++;
				cnt++;
				if (b == e)
					break;
			}
			if (*b == '\n')
			{
				stringmode=0;
				process(imapAccount, Token(EOL));
				imapAccount.uninstallHandler(this);
				return cnt+1;
			}

			if (s.length() < 10000)
				s.append(&*b, 1);
			++b;
			++cnt;
		}
		return 0;
	}

	if (largestringleft > 0)  // Collecting {nnn}\r\n strings
	{
		size_t n=buffer.length();

		if (n > largestringleft)
			n=largestringleft;

		string buffer_cpy=buffer;

		buffer_cpy.erase(n);

		largestringleft -= n;

		setTimeout(); // Don't time me out just yet...

		if (wantLargeString())
		{
			setTimeout();

			if (!errormode)
			{
				if (largestringleft == 0)
					process(imapAccount, Token(STRING,
							    buffer_cpy));
				else	// More later...
					process(imapAccount, Token(LARGESTRING,
							    buffer_cpy));
			}
			return n;
		}

		// Subclass is not prepared to handle {nnn}\r\n strings.
		// For simplicity, they are converted to regular strings,
		// but to guard against hostile servers truncate large
		// strings at 8K

		stringbuffer += buffer_cpy;

		if (stringbuffer.length() > 8192)
			stringbuffer.erase(8192);

		setTimeout();
		if (largestringleft == 0 && !errormode)
			process(imapAccount, Token(STRING, stringbuffer));
		return n;
	}

	while (b != e)
	{
		if (*b == '\n')
		{
			setTimeout();
			if (!donemode && !errormode)
				process(imapAccount, Token(EOL));
			imapAccount.uninstallHandler(this);
			return (cnt+1);
		}

		if (unicode_isspace((unsigned char)*b))
		{
			b++;
			cnt++;
			continue;
		}

		if (*b == '{')	// Possibly a large string?
		{
			size_t n=0;

			b++;
			cnt++;

			for (;;)
			{
				if (b == e)
					return 0;

				if (*b == '}')
				{
					b++;
					cnt++;
					if (b == e)
						return 0;

					if (*b == '\r')
					{
						b++;
						cnt++;
						if (b == e)
							return 0;
					}

					if (*b != '\n')
						break;

					b++;
					cnt++;

					largestringsize=n;
					largestringleft=n;
					stringbuffer="";

					if (donemode && !errormode)
					{
						error(imapAccount);
						return cnt;
					}

					setTimeout();
					if (!errormode && n == 0)
						// Make sure we recognize a
						// 0-length string
					{
						process(imapAccount,
							Token(STRING,
							      stringbuffer));
					}
					return cnt;
				}

				if (!isdigit((int)(unsigned char)*b))
					break;

				n=n * 10 + (*b-'0');
				b++;
				cnt++;
			}

			if (!errormode)
				error(imapAccount);
			return cnt;
		}
		if (errormode)
			// Recovering from an error, just eat input until \n.
		{
			b++;
			cnt++;
			continue;
		}


		if (donemode)
		{
			error(imapAccount);
			continue;
		}

		if (strchr("*()", *b) != NULL)
		{
			setTimeout();
			process(imapAccount, Token((int)(char)*b));
			return cnt+1;
		}

		if (*b == '"')
		{
			stringmode=1;
			return cnt+1;
		}

#define ATOMCHAR(c) ATOMCHAR2((int)(unsigned char)(c))
#define ATOMCHAR2(c) ( (c) > ' ' && (c) != '"' && \
			(c) != '(' && (c) != ')' && \
			(c) != '{' && (c) != '%' && (c) != '*')

		if (ATOMCHAR(*b))	// ATOM
		{
			string s="";

			s.append(&*b, 1);
			b++;
			cnt++;

			// Swallow up BODY[HEADER.FIELDS (list)]

			unsigned bracket_cnt=0;

			while (b != e)
			{
				if (bracket_cnt > 0)
				{
					if ( *b == '\n' || *b == '\r')
					{
						process(imapAccount,
							Token(ATOM, s));
						return cnt;
					}
					
					if (s.length() < 10000)
						s.append(&*b, 1);

					if ( *b == ']' )
					{
						if (--bracket_cnt == 0)
						{
							process(imapAccount,
								Token(ATOM, s)
								);
							return cnt+1;
						}
					}
					++b;
					++cnt;

					continue;
				}

				if (!ATOMCHAR(*b))
				{
					setTimeout();

					if (strcasecmp(s.c_str(), "NIL") == 0)
						process(imapAccount, Token(NIL));
					else
						process(imapAccount, Token(ATOM, s));
					return cnt;
				}

				if (*b == '[')
					++bracket_cnt;

				if (s.length() < 10000)
					s.append(&*b, 1);
				++b;
				++cnt;
			}
			return (0);
		}

		error(imapAccount);
		return cnt+1;
	}

	return cnt;
}

bool mail::imapHandlerStructured::wantLargeString()
{
	return false; // The default, only some subclasses have large string
	// handler code
}

void mail::imapHandlerStructured::done()
{
	donemode=1;
}

//
// Go into error recovery mode, eating input until next endofline

void mail::imapHandlerStructured::error(mail::imap &imapAccount)
{
	if (errbuf.length() > 32)
	{
		errbuf.erase(32);
		errbuf.append("...");
	}

	size_t p=errbuf.find('\n');

	if (p < std::string::npos)
		errbuf.erase(p);

	imapAccount.fatalError("IMAP reply parse failure: ... " + errbuf);
	errormode=1;
}


mail::imapHandlerStructured::Token::operator string()
{
	switch ((enum TokenType)token) {
	case EOL:
		return "EOL";
	case ATOM:
		return text;
	case STRING:
	case LARGESTRING:
		return ( "STRING[" + text + "]");
	case NIL:
		return ( "NIL" );
	}

	char c[4];

	c[0]='\'';
	c[1]=(char)token;
	c[2]='\'';
	c[3]=0;

	return c;
}
