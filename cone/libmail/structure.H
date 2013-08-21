/*
** Copyright 2002-2005, Double Precision Inc.
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

	// The Content-Type: and Content-Disposition: headers have an
	// optional parameter=value; list, which is parsed into a hash.
	//
	// The get() method fetches the parameter.  It's a pretty smart get()
	// method.  If a character set is provided, the parameter value is
	// mapped to the specified character set.  Additionally, the get()
	// method automatically handles RFC 2231-encoded attributes.
	// Ain't that cool?

	class parameterList {
		std::map<std::string, std::string> param_map;

	public:
		typedef std::map<std::string, std::string>::iterator iterator;
		typedef std::map<std::string, std::string>::const_iterator
			const_iterator;

		iterator begin() { return param_map.begin(); }
		iterator end() { return param_map.end(); }
		const_iterator begin() const { return param_map.begin(); }
		const_iterator end() const { return param_map.end(); }

		parameterList();
		~parameterList();

		void set_simple(std::string name, std::string value)
		{
			mail::upper(name);
			param_map.insert(std::make_pair(name, value));
		}

		void erase(std::string name)
		{
			mail::upper(name);
			iterator p=param_map.find(name);

			if (p != end())
				param_map.erase(p);
		}

		void set(std::string name, std::string value,
			 std::string charset,
			 std::string language="");

		bool exists(std::string name) const;
		std::string get(std::string name,
				std::string chset="") const;

		std::string toString(std::string type) const;
		// Create a header from "type", plus these parameters
	};

	std::string mime_id;	// An opaque identifier of this MIME section.


	std::string type, subtype;	// text, plain

	bool messagerfc822() const
	{
		return type == "MESSAGE" && subtype == "RFC822";
	}

	bool multipartsigned() const
	{
		return type == "MULTIPART" && subtype == "SIGNED";
	}

	bool multipartencrypted() const
	{
		return type == "MULTIPART" && subtype == "ENCRYPTED";
	}

	parameterList type_parameters;

	// <charset, iso-8859-1>

	std::string content_id;
	std::string content_description;
	std::string content_transfer_encoding;

	size_t content_size;
	size_t content_lines;

	std::string content_md5;
	std::string content_language;

	std::string content_disposition;

	parameterList content_disposition_parameters;

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
