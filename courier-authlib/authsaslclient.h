#ifndef	authsaslclient_h
#define	authsaslclient_h

/*
** Copyright 2000-2008 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#if	HAVE_CONFIG_H
#include	"courier_auth_config.h"
#endif
#include	<sys/types.h>
#include	"courierauthsaslclient.h"

#ifdef	__cplusplus
extern "C" {
#endif



#define SASL_LIST \
	SASL("EXTERNAL", NO_SERVER_FUNC(), authsaslclient_external) \
	SASL("CRAM-SHA256", SERVER_FUNC(authsasl_cram), authsaslclient_cramsha256) \
	SASL("CRAM-SHA1", SERVER_FUNC(authsasl_cram), authsaslclient_cramsha1) \
	SASL("CRAM-MD5", SERVER_FUNC(authsasl_cram), authsaslclient_crammd5) \
	SASL("PLAIN", SERVER_FUNC(authsasl_plain), authsaslclient_plain) \
	SASL("LOGIN", SERVER_FUNC(authsasl_login), authsaslclient_login)

/* A list of SASL client functions */

struct authsaslclientlist_info {
	const char *name;
	int (*func)(const struct authsaslclientinfo *);
	} ;

extern int authsaslclient_login(const struct authsaslclientinfo *);
extern int authsaslclient_plain(const struct authsaslclientinfo *);
extern int authsaslclient_external(const struct authsaslclientinfo *);
extern int authsaslclient_crammd5(const struct authsaslclientinfo *);
extern int authsaslclient_cramsha1(const struct authsaslclientinfo *);
extern int authsaslclient_cramsha256(const struct authsaslclientinfo *);

#ifdef	__cplusplus
}
#endif

#endif
