/*
** Copyright 2002-20011, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_rfcaddr_h
#define libmail_rfcaddr_h

#include "libmail_config.h"
#include "unicode/unicode.h"
#include "namespace.H"

#include <stdlib.h>
#include <vector>
#include <string>

//
// An RFC 2822 address, a name and user@domain.
//

LIBMAIL_START

class address {

protected:
	std::string name;
	std::string addr;
public:

	std::string getName() const { return name; }
	std::string getAddr() const { return addr; }

	virtual void setName(std::string s);
	virtual void setAddr(std::string a);

	// If addr.size() == 0 this is the obsolete rfc 822
	// group list notation, and name is either ";" or "name:"

	address(std::string n, std::string a);

	virtual ~address();

	// Convert myself to "name" <addr> format.

	std::string toString() const;

	//
	// Create an RFC 2822 address header, from a vector of addresss.
	//

	template<class T>
	static std::string toString(std::string hdr, // Name:
				    const std::vector<T> &h,
				    size_t w=76); // Max length of lines.

	//
	// Create a vector of addresses from a string
	//

	template<class T> static bool fromString(std::string addresses,
						 std::vector<T> &h,
						 size_t &errIndex);

	// Addresses are appended to h
	// Returns false if memory allocation failed or addresses is
	// syntactically invalid.  errIndex points where in addresses the
	// syntax error is (or is string::npos if memory allocation failed)

	std::string getCanonAddress() const;
	// Return addr with domain portion converted to lowercase

	bool operator==(const address &) const;
	// Compare the addr portions only, ignoring domain case

	bool operator!=(const address &a) const
	{
		return !operator==(a);
	}

};

// INPROGRESS: slowly migrate to automatic MIME encoding of names.
//
// A subclass of address that MIME-decodes the name portion of the address,
// and IDN-decodes the hostname portion of the address (if libidn is available).
//
// emailAddress transparently casts to mail::address, importing/exporting
// the name and address in its decoded form.

class emailAddress : public address {

	// The name field as unicode characters

	std::vector<unicode_char> decodedName;

	//! The address field, with the hostname portion decoded

	std::vector<unicode_char> decodedAddr;

	void decode();
public:
	emailAddress();
	emailAddress(const address &);
	virtual ~emailAddress();

	std::string getDisplayName(const std::string &charset,
				   bool &errflag) const
	{
		return mail::iconvert::convert(decodedName, charset, errflag);
	}

	std::string getDisplayAddr(const std::string &charset,
				   bool &errflag) const
	{
		return mail::iconvert::convert(decodedAddr, charset, errflag);
	}

	std::string getDisplayName(const std::string &charset) const
	{
		bool dummy;

		return getDisplayName(charset, dummy);
	}

	std::string getDisplayAddr(const std::string &charset) const
	{
		bool dummy;

		return getDisplayAddr(charset, dummy);
	}

	std::string setDisplayName(const std::string &s,
				   const std::string &charset);
	std::string setDisplayAddr(const std::string &a,
				   const std::string &charset);

	virtual void setName(std::string s);
	virtual void setAddr(std::string s);

	bool operator==(const address &a) const
	{
		return mail::address::operator==(a);
	}

	bool operator!=(const address &a) const
	{
		return !operator==(a);
	}

};

LIBMAIL_END

#endif
