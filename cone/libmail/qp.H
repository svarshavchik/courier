/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_qp_H
#define libmail_qp_H

#include "libmail_config.h"
#include "decoder.H"
#include "namespace.H"

#include <string>

LIBMAIL_START

//
// MIME quoted-printable decoder

class decodeqp : public decoder {
	std::string decodeBuffer;

public:
	decodeqp();
	virtual ~decodeqp();

	void decode(std::string text);
private:
	virtual void decoded(std::string buffer)=0;
};

LIBMAIL_END

#endif
