/*
** Copyright 2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_header_H
#define libmail_header_H

///////////////////////////////////////////////////////////////////////////
//
// Helper classes for parsing headers.

#include <list>
#include <string>
#include <vector>
#include "structure.H"
#include "rfcaddr.H"
#include "namespace.H"

LIBMAIL_START

class emailAddress;

//
// Superclass of all headers
//

class Header {

protected:
	std::string wrap(std::string) const;

	class listitem;
public:
	class plain;
	class encoded;
	class addresslist;
	class mime;
	class list;

	std::string name;	// Header name, without the colon

	Header(std::string nameArg) : name(nameArg) {}
	virtual ~Header();

	Header(const Header &); // UNDEFINED
	Header &operator=(const Header &); // UNDEFINED

	virtual std::string toString() const=0; // Convert to string format.
	virtual Header *clone() const=0; // Clone myself
};

//
// An unstructured header, 7-bit only.
//

class Header::plain : public Header {

	std::string text;
public:
	plain(std::string name, std::string textArg)
		: Header(name), text(textArg) {}
	~plain();

	std::string toString() const;
	mail::Header *clone() const;
};

//
// An unstructured header, MIME-encoded.
//

class Header::encoded : public Header::plain {

	static std::string encode(std::string, std::string, std::string);
public:
	encoded(std::string name,
		std::string text, std::string charset,
		std::string lang="");
	~encoded();
};

//
// A header containing a list of addresses.
//

class Header::addresslist : public Header {

public:
	std::vector<mail::emailAddress> addresses;

	addresslist(std::string name);
	addresslist(std::string name,
		    const std::vector<mail::emailAddress> &addressesArg);
	~addresslist();

#if 0
	addresslist &operator()(std::string name, std::string addr)
	{
		mail::emailAddress addr;

		addr.setDisplayName(name);
		addr.setDisplayAddr(addr);

		addresses.push_back(addr);
		return *this;
	}
#endif

	addresslist &operator()(const mail::emailAddress &a)
	{
		addresses.push_back(a);
		return *this;
	}

	std::string toString() const;
	mail::Header *clone() const;
};

//
// A MIME header, such as Content-Type: or Content-Description:
//

class Header::mime : public Header {

public:
	std::string value;
	mimestruct::parameterList parameters;

	typedef mimestruct::parameterList::iterator parameter_iterator;
	typedef mimestruct::parameterList::const_iterator const_parameter_iterator;

	mime(std::string name);
	mime(std::string name, std::string value);

	static mime fromString(std::string);
private:
	static void cb_type(const char *, void *);
	static void cb_param(const char *, const char *, void *);
public:

	~mime();
	std::string toString() const;

	mime &operator()(std::string name,
			 std::string value)
	{
		parameters.set_simple(name, value);
		return *this;
	}

	mime &operator()(std::string name,
			 std::string value,
			 std::string charset,
			 std::string language="")
	{
		parameters.set(name, value, charset, language);
		return *this;
	}

	parameter_iterator begin() { return parameters.begin(); }
	parameter_iterator end() { return parameters.end(); }

	const_parameter_iterator begin() const
	{
		return parameters.begin();
	}

	const_parameter_iterator end() const
	{
		return parameters.end();
	}
	mail::Header *clone() const;
};

//
// Helper class for a list of headers.  Header::listitem wraps around a
// Header superclass, and pretends to be one.
//

class Header::listitem {

	Header *h;
public:
	listitem(const listitem &);
	listitem(const Header &);
	~listitem();
	listitem &operator=(const Header &);

	listitem &operator=(const listitem &);
	operator Header &() { return *h; }
	operator const Header &() const { return *h; }
};

//
// A list of headers is just a subclass of a list<Header::listitem> which
// defines a convenient string operator that returns the string representation
// of this list of headers
//

class Header::list : public std::list<Header::listitem> {

public:
	list();
	~list();

	operator std::string() const;

	Header::list &operator<<(const Header &h)
	{
		push_back(h);
		return *this;
	}
};


LIBMAIL_END

#endif
