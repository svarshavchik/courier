/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_rfc2047_decode_h
#define libmail_rfc2047_decode_h

#include "libmail_config.h"
#include "unicode/unicode.h"
#include "rfcaddr.H"

#include <vector>
#include <string>

#include "namespace.H"

LIBMAIL_START

//
// Mail header decoder.  A variety of decoding options are available.
// This is basically a wrapper for librfc822.a's functions, with some
// value-added code.
//

namespace rfc2047 {

	class decoder {

		std::string decodedbuf;

	public:
		decoder();
		~decoder();

	private:
		static void rfc2047_decode_callback(const char *text,
						    size_t text_len,
						    void *voidarg);

		void rfc2047_callback(const char *text, size_t text_len);
	public:

		// Decode to unicode chars.
		const unicode_char *decode(std::string rfc2047_text);

		// Decode to charset 'tocharset'.

		std::string decode(std::string rfc2047_text,
				   std::string charset);


		// Decode the name portion in a parsed list of addresses.
		// Decode to charset 'nativeInfo', and prepend [CHARSET] to
		// content in any charset other than 'nativeInfo'.

		void decode(std::vector<mail::address> &addr_cpy,
			    std::string charset);

		// Decode without transcoding to a specific charset.

		static std::string decodeSimple(std::string str);

	private:
		static void decodeSimpleCallback(const char *chset,
						 const char *lang,
						 const char *content,
						 size_t cnt,
						 void *dummy);

	};
}

LIBMAIL_END

#endif

