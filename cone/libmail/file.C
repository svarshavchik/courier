/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "file.H"
#include "unicode/unicode.h"
#include <ctype.h>

using namespace std;

/////////////////////////////////////////////////////////////////////////////

mail::file::file(int fdArg, const char *mode)
	: fd(dup(fdArg)), fp(NULL), pos(-1)
{
	if (fd >= 0)
		fp=fdopen(fd, mode);

	if (fp && fseek(fp, 0L, SEEK_SET) < 0)
	{
		fclose(fp);
		fp=NULL;
	}
}

mail::file::~file()
{
	if (fp != NULL)
		fclose(fp);

	if (fd >= 0)
		close(fd);
}

void mail::file::seeked()
{
	if (fp)
		pos=ftell(fp);
}

string mail::file::getline()
{
	// First time through, allocate a 1000 bye buffer.

	if (buffer.size() != 1000)
	{
		buffer.clear();
		buffer.insert(buffer.end(), 1000, 0);
	}

	string l;

	for (;;)
	{
		vector<char>::iterator b=buffer.begin(), e=buffer.end();

		while (b != e)
		{
			int ch=getc(fp);

			if (ch == EOF)
				break;
			if (pos != -1)
				pos++;

			*b++ = ch;
			if (ch == '\n')
				break;
		}

		if (b == buffer.begin())
			break; // EOF

		if (b[-1] != '\n')
		{
			l.insert(l.end(), buffer.begin(), b);
			continue;
		}


		--b;
		l.insert(l.end(), buffer.begin(), b);
		break;
	}

	size_t n=l.size();

	if (n > 0 && l[n-1] == '\r')
		l=l.substr(0, n-1);
	return l;
}


void mail::file::genericMessageRead(mail::account *account,
				    size_t messageNumber,
				    mail::readMode readType,
				    mail::callback::message &callback)
{
	ptr<mail::account> ptrAccount(account);

	if (readType == mail::readHeadersFolded
	    || readType == mail::readHeaders)
	{
		string hdr="";

		string l;

		while (!ptrAccount.isDestroyed() && !feof(fp) && !ferror(fp))
		{
			l=getline();

			if (l.size() == 0)
				break;

			if (unicode_isspace((unsigned char)l[0]))
			{
				const char *p=l.c_str();

				if (readType == mail::readHeaders)
					hdr += "\n";
				else // Fold headers
				{
					hdr += " ";

					while (*p &&
					       unicode_isspace((unsigned char)
							       *p))
						p++;
				}
				hdr += p;
				continue;
			}

			if (hdr.size() > 0) // Prev header.
			{
				hdr += "\n";
				callback.messageTextCallback(messageNumber,
							     hdr);
			}
			hdr=l;
		}

		if (hdr.size() > 0 && !ptrAccount.isDestroyed())
		{
			hdr += "\n";
			callback.messageTextCallback(messageNumber, hdr);
		}
		return;
	}

	if (readType == mail::readContents)
		// Skip over the header portion.
	{
		while (!feof(fp) && !ferror(fp))
		{
			if (getline().size() == 0)
				break;
		}
	}

	vector<char> buffer;

	buffer.insert(buffer.end(), BUFSIZ, 0);

	size_t i=0;

	int ch;

	while (!ptrAccount.isDestroyed() && (ch=getc(fp)) != EOF)
	{
		if (ch == '\r')
			continue;

		if (i >= buffer.size())
		{
			callback.messageTextCallback(messageNumber,
						     string(buffer.begin(),
							    buffer.end()));
			i=0;
		}

		buffer[i++]=ch;
	}

	if (i > 0 && !ptrAccount.isDestroyed())
		callback.messageTextCallback(messageNumber,
					     string(buffer.begin(),
						    buffer.begin()+i));
}
