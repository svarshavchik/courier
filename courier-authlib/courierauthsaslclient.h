#ifndef	courierauthsaslclient_h
#define	courierauthsaslclient_h

/*
** Copyright 2000-2004 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#include	"courierauthsasl.h"
#include	<sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif


/*
	These family of functions are used to implement the SASL client
	interface on top of authlib.
*/


/*
**  The authsaslclientinfo structure is initialized by the sasl client.
**  It's the sole argument to authsaslclient().
*/

struct authsaslclientinfo {

	const char *userid;		/* Usually required */
	const char *password;		/* Usually required */

	const char *sasl_funcs;		/* A list of SASL methods supported
					** by the server, space-separated.
					*/

	const char *(*start_conv_func)(const char *, const char *, void *);
			/*
			** Start SASL conversation.  First argument is the
			** SASL method name.  The second argument is the
			** initial message to send to the SASL server, base64-
			** encoded, or NULL if there is no initial message.
			*/

	const char *(*conv_func)(const char *, void *);
			/* The conversation function.  It receives a base64
			** string to send to the server, and returns a
			** base64 response (or NULL of there was an error).
			*/

	int (*final_conv_func)(const char *, void *);
			/*
			** The "final" base64 message to send to the server.
			*/

	int (*plain_conv_func)(const char *, const char *, void *);
			/*
			** plain_conv_func is used when the SASL method is
			** a simple method involving a single message, like
			** PLAIN.  plain_conv_func is basically a merge between
			** start_conv_func and final_conv_func, a one-shot
			** deal.
			*/

	void *conv_func_arg;	/* Callback argument to conv_func */
	} ;

int auth_sasl_client(const struct authsaslclientinfo *);
	/* Returns 0 for success, non zero for failure */

/* Additional error codes */

#define	AUTHSASL_NOMETHODS	-3
#define	AUTHSASL_CANCELLED	-4

#ifdef	__cplusplus
}
#endif

#endif
