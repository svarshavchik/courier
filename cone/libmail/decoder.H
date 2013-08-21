/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_decoder_H
#define libmail_decoder_H

#include "libmail_config.h"
#include "namespace.H"

#include <string>

LIBMAIL_START

///////////////////////////////////////////////////////////////////////////
//
// Common superclass for objects that decode MIME encoding. (quoted-printable,
// base64).  Each superclass decoder repeatedly invokes the decode() method,
// with partial decoded contents as arguments.

class decoder {
public:
	decoder();
	virtual ~decoder();
	virtual void decode(std::string)=0;
};

LIBMAIL_END

#endif
