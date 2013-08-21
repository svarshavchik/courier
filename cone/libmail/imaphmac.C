/*
** Copyright 2002-2005, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "imaphmac.H"
#include "libhmac/hmac.h"

static mail::imaphmac md5(hmac_md5, "MD5");
static mail::imaphmac sha1(hmac_sha1, "SHA1");
static mail::imaphmac sha256(hmac_sha256, "SHA256");

const mail::imaphmac * const mail::imaphmac::hmac_methods[]=
	{ &sha256, &sha1, &md5, NULL};


mail::imaphmac::imaphmac(const struct hmac_hashinfo &hmacArg,
			       const char *nameArg)
	: hmac(hmacArg), name(nameArg)
{
}

mail::imaphmac::~imaphmac()
{
}

std::string mail::imaphmac::operator()(std::string password, std::string challenge) const
{
	std::string i, o, b;

	i.insert(i.end(), hmac.hh_L, (char)0);
	o.insert(o.end(), hmac.hh_L, (char)0);
	b.insert(b.end(), hmac.hh_L, (char)0);

	hmac_hashkey( &hmac, &*password.begin(), password.size(),
		      (unsigned char *)&*o.begin(),
		      (unsigned char *)&*i.begin());

	hmac_hashtext( &hmac, &*challenge.begin(), challenge.size(),
		       (unsigned char *)&*o.begin(),
		       (unsigned char *)&*i.begin(),
		       (unsigned char *)&*b.begin());
	return (b);
}
