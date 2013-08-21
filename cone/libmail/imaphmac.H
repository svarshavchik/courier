/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_imaphmac_h
#define libmail_imaphmac_h

#include "libmail_config.h"
#include "namespace.H"

#include <string>

struct hmac_hashinfo;

///////////////////////////////////////////////////////////////////////////
//
// Helper class for calculating HMAC hashes.

LIBMAIL_START

class imaphmac {

	const struct hmac_hashinfo &hmac; // The HMAC function
	const char *name;		// The name of this function.

public:
	imaphmac(const struct hmac_hashinfo &hmacArg,
		       const char *nameArg);

	~imaphmac();

	std::string operator()(std::string password,
			       std::string challenge) const;

	static const imaphmac * const hmac_methods[];

	const char *getName() const { return (name); }

};

LIBMAIL_END

#endif
