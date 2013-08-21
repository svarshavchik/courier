/*
** Copyright 2002-2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_rfc2047_encode_h
#define libmail_rfc2047_encode_h

#include <string>

//
// Encode 8-bit header content.  It's basically a C++ wrapper for
// rfc2047_encode_str in librfc822.a
//
//
// Usage:
//
// string hdr=mail::account::rfc2047::encode(hdr8bitContents, charset);
//

#include "namespace.H"

LIBMAIL_START

namespace rfc2047 {

	class encode {

		std::string encodedString;

	public:
		encode(std::string txt, std::string charset);
		~encode();

		operator std::string() const { return encodedString; }
	};

	class encodeAddrName {
		std::string encodedString;
	public:
		encodeAddrName(std::string txt,
			       std::string charset);
		~encodeAddrName();

		operator std::string() const { return encodedString; }
	};
}

LIBMAIL_END

#endif

