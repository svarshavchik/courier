/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "misc.H"
#include "mboxmagictag.H"

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <sstream>

using namespace std;

static const char magicTag[]="X-MailPP-UID: ";

mail::mboxMagicTag::mboxMagicTag(string header,
				 mail::keywords::Hashtable &h)
	: tag("")
{
	if (strncmp(header.c_str(), magicTag, sizeof(magicTag)-1) == 0)
	{
		header=header.substr(sizeof(magicTag)-1);

		size_t colon=header.find(':');

		if (colon != std::string::npos)
			colon=header.find(':', colon+1);

		if (colon != std::string::npos)
		{
			// keywords

			string::iterator b=header.begin()+colon+1;
			string::iterator e=header.end();

			while (b != e)
			{
				if (*b == ' ')
				{
					++b;
					continue;
				}

				string::iterator c=b;

				while (b != e)
				{
					if (*b == ' ')
						break;
					++b;
				}
				keywords.addFlag(h, string(c, b));
			}

			header=header.substr(0, colon);
		}
		tag=header;
	}
}

mail::mboxMagicTag::mboxMagicTag(string uid, mail::messageInfo flags,
				 mail::keywords::Message keywordsArg)
	: tag(""), keywords(keywordsArg)
{
	string t="";

	if (!flags.unread)
		t += "R";

	if (!flags.recent)
		t += "O";

	if (flags.marked)
		t += "F";

	if (flags.replied)
		t += "A";

	if (flags.deleted)
		t += "D";

	if (flags.draft)
		t += "d";

	tag=t + ":" + uid;
}

mail::mboxMagicTag::mboxMagicTag()
{
	string buffer;

	static size_t counter=0;

	{
		ostringstream o;

		o << time(NULL) << '-' << getpid() << '-' << counter++;
		buffer=o.str();
	}

	string h=buffer + '-' + mail::hostname();

	size_t p;

	// Just in case, dump any colons from the uid.

	while ((p=h.find(':')) != std::string::npos)
		h=h.substr(0, p) + h.substr(p+1);

	tag=":" + h;
}

mail::mboxMagicTag::~mboxMagicTag()
{
}

mail::messageInfo mail::mboxMagicTag::getMessageInfo() const
{
	mail::messageInfo info;

	size_t n=tag.find(':');

	if (n != std::string::npos)
		info.uid=tag.substr(n+1);

	info.recent=true;
	info.unread=true;

	const char *p=tag.c_str();

	while (*p && *p != ':')
		switch (*p++) {
		case 'F':
			info.marked=true;
			break;
		case 'A':
			info.replied=true;
			break;
		case 'R':
			info.unread=false;
			break;
		case 'O':
			info.recent=false;
			break;
		case 'D':
			info.deleted=true;
			break;
		case 'd':
			info.draft=true;
			break;
		}

	return info;
}

string mail::mboxMagicTag::toString() const
{
	set<string> kwSet;
	set<string>::iterator b, e;

	keywords.getFlags(kwSet);

	string v=string(magicTag) + tag;

	const char *sep=":";

	b=kwSet.begin();
	e=kwSet.end();

	while (b != e)
	{
		v += sep;
		v += *b;
		sep=" ";
		++b;
	}

	return v;
}
