/*
** Copyright 2003-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "smapaddmessage.H"
#include "smapadd.H"
#include "rfc822/rfc822.h"
#include <errno.h>
#include <sstream>
#include <cstring>

using namespace std;

mail::smapAddMessage::smapAddMessage(mail::imap &imapAccountArg,
				     mail::callback &callbackArg)
	: addMessage(&imapAccountArg),
	  imapAccount(imapAccountArg),
	  callback(&callbackArg),
	  tfile(tmpfile())
{
}

mail::smapAddMessage::~smapAddMessage()
{
	if (tfile)
		fclose(tfile);
	if (callback)
		callback->fail("Operation aborted.");
}

void mail::smapAddMessage::saveMessageContents(std::string contents)
{
	size_t n;

	while ((n=contents.find('\r')) != std::string::npos)
		contents.erase(contents.begin()+n,
			       contents.begin()+n+1);

	if (tfile)
		if (fwrite(&contents[0], contents.size(), 1, tfile) != 1)
			; // Ignore gcc warning
}

void mail::smapAddMessage::go()
{
	if (!tfile || ferror(tfile) || fflush(tfile) ||
	    fseek(tfile, 0L, SEEK_SET) < 0)
	{
		fail(strerror(errno));
		return;
	}

	if (!checkServer())
		return;

	{
		string flags;

#define FLAG
#define NOTFLAG !
#define DOFLAG(NOT, field, name) \
		if (NOT messageInfo.field) flags += "," name

		LIBMAIL_SMAPFLAGS;
#undef DOFLAG
#undef NOTFLAG
#undef FLAG

		if (flags.size() > 0)
			flags=flags.substr(1);

		ostringstream o;

		o << "ADD FLAGS=" << flags;

		if (messageDate)
		{
			char buf[80];

			rfc822_mkdate_buf(messageDate, buf);

			o << " \"INTERNALDATE=" << buf << "\"";
		}

		o << "\n";

		cmds.push_back(o.str());
	}

	smapAdd *add=new smapAdd(cmds, *callback);

	if (add)
	{
		callback=NULL;
		add->tfile=tfile;
		tfile=NULL;
	}

	try {
		imapAccount.installForegroundTask(add);
	} catch (...) {
		if (add)
			delete add;
		delete this;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	delete this;
}

void mail::smapAddMessage::fail(std::string errmsg)
{
	mail::callback *c=callback;

	callback=NULL;
	delete this;

	if (c)
		c->fail(errmsg);
}

