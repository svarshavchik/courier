/*
** Copyright 2002-2005, S. Varshavchik.
**
** See COPYING for distribution information.
*/
#ifndef libmail_structure_H
#define libmail_structure_H

#include "libmail_config.h"

#include <string>
#include <vector>
#include <map>

#include "namespace.H"
#include "misc.H"
#include	"rfc2045/rfc2045.h"
LIBMAIL_START

class envelope;
class xenvelope;

///////////////////////////////////////////////////////////////////////////
//
// The structure of a message is described by a tree of mail::mimestruct
// objects.  Each mail::mimestruct objects represents a MIME section in the
// message, and may contain pointers to mail::mimestruct objects for content
// sections if the MIME section is a multipart section.

class mimestruct {

public:

	std::string mime_id;	// An opaque identifier of this MIME section.


	rfc2231::header content_type, content_disposition;

	bool messagerfc822() const;

	bool multipartsigned() const
	{
		return content_type.value == "multipart/signed";
	}

	bool multipartencrypted() const
	{
		return content_type.value == "multipart/encrypted";
	}

	// <charset, iso-8859-1>

	std::string content_id;
	std::string content_description;
	std::string content_transfer_encoding;

	size_t content_size;
	size_t content_lines;

	std::string content_md5;
	std::string content_language;

	bool smtputf8;
	/* 8 bit headers exist, set only when parsing a message from a file. */

	mimestruct();
	~mimestruct();

	// Copy constructors, and assignment operators, are fully functional.

	mimestruct(const mimestruct &cpy);
	mimestruct &operator=(const mimestruct &cpy);

private:
	mail::envelope *message_rfc822_envelope;

	std::vector<mimestruct *> children;
	mimestruct *parent;

	void destroy();
public:

	// MESSAGE/RFC822 content also has an associated mail::envelope
	// object.  getEnvelope() automatically creates this object,
	// which is automatically destroyed by the destructor

	class mail::envelope &getEnvelope();
	const class mail::envelope &getEnvelope() const
	{
		return *message_rfc822_envelope;
	}

	mimestruct *addChild();
	size_t getNumChildren() const { return children.size(); }

	const mimestruct *getChild(size_t n)
		const { return children[n]; }

	const mimestruct *getParent() const { return parent; }

	mimestruct *getChild(size_t n) { return children[n]; }
	mimestruct *getParent() { return parent; }

	mimestruct *find(std::string mime_idArg)
	{
		if (mime_idArg == mime_id)
			return this;

		std::vector<mimestruct *>::iterator b=children.begin(),
			e=children.end();

		while (b != e)
		{
			mimestruct *p= (*b)->find(mime_idArg);

			++b;

			if (p)
				return p;
		}

		return NULL;
	}
};
LIBMAIL_END

#endif
