/*
** Copyright 2002-2008, S. Varshavchik.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "misc.H"
#include "mail.H"
#include "structure.H"
#include "envelope.H"
#include "rfcaddr.H"
#include <cstring>

#include "rfc822/rfc2047.h"
#include "rfc2045/rfc2045.h"
#include <courier-unicode.h>

#include <errno.h>

#include <map>
#include <string>
#include <fstream>

using namespace std;

mail::mimestruct::mimestruct()
	: content_type{"", true},
	  content_disposition{"", true},
	  content_size(0), content_lines(0), smtputf8(false),
	  message_rfc822_envelope(0), parent(0)
{
}

mail::mimestruct::~mimestruct()
{
	destroy();
}

mail::mimestruct::mimestruct(const mail::mimestruct &cpy)
	: content_type{"", true},
	  content_disposition{"", true},
	  content_size(0), content_lines(0),
	  message_rfc822_envelope(0), parent(0)
{
	(*this)=cpy;
}

bool mail::mimestruct::messagerfc822() const
{
	return rfc2045::message_content_type(content_type.value);
}

mail::mimestruct &mail::mimestruct::operator=(const mail::mimestruct &cpy)
{
	destroy();

	vector<mail::mimestruct *>::const_iterator
		b=cpy.children.begin(),
		e=cpy.children.end();

	while (b != e)
	{
		(*addChild())= **b;
		b++;
	}

#define CPY(x) (x)=(cpy.x)

	CPY(mime_id);
	CPY(content_type);
	CPY(content_disposition);
	CPY(content_id);
	CPY(content_description);
	CPY(content_transfer_encoding);
	CPY(content_size);
	CPY(content_lines);
	CPY(content_md5);
	CPY(content_language);
#undef CPY

	if (cpy.message_rfc822_envelope)
	{
		message_rfc822_envelope=new mail::envelope;

		if (!message_rfc822_envelope)
			LIBMAIL_THROW("Out of memory.");

		*message_rfc822_envelope= *cpy.message_rfc822_envelope;
	}

	return *this;
}

mail::envelope &mail::mimestruct::getEnvelope()
{
	if (!message_rfc822_envelope)
		if ((message_rfc822_envelope=new mail::envelope) == NULL)
			LIBMAIL_THROW("Out of memory.");
	return *message_rfc822_envelope;
}

void mail::mimestruct::destroy()
{
	mail::envelope *p=message_rfc822_envelope;

	if (p)
	{
		message_rfc822_envelope=NULL;
		delete p;
	}

	vector<mail::mimestruct *>::iterator b=children.begin(),
		e=children.end();

	while (b != e)
	{
		delete *b;
		b++;
	}
	children.clear();
}

mail::mimestruct *mail::mimestruct::addChild()
{
	mail::mimestruct *p=new mail::mimestruct;

	if (!p)
		LIBMAIL_THROW("Out of memory.");

	try {
		p->parent=this;
		children.push_back(p);
	} catch (...)
	{
		delete p;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	return p;
}
