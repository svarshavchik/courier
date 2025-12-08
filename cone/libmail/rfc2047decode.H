/*
** Copyright 2002, S. Varshavchik.
**
** See COPYING for distribution information.
*/
#ifndef libmail_rfc2047_decode_h
#define libmail_rfc2047_decode_h

#include "libmail_config.h"
#include <courier-unicode.h>
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
		// Decode to charset 'tocharset'.

		std::string decode(std::string rfc2047_text,
				   std::string charset);

		// Decode without transcoding to a specific charset.

		static std::string decodeSimple(std::string str);
	};
}

LIBMAIL_END

#endif
