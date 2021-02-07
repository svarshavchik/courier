#ifndef	courierauthsasl_h
#define	courierauthsasl_h

/*
** Copyright 1998 - 2013 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#include	<sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif


/*
	These family of functions are used to implement the SASL interface
	on top of authlib.  It is mainly used by the authentication user
	process to build the authentication request data for authmod()
	based upon the SASL challenge/response interaction.
*/

/*
**	auth_sasl searches for the right method, and calls the appropriate
**	sasl function.  authsasl received the following arguments:
**
**	initresponse -- initial response for the authentication request,
**	if provided.  If provided, the actual response MUST BE PROVIDED
**	in initresponse using base64 encoding!!!
**
**	sasl_func -- the callback function which is used to carry out the
**	SASL conversation.  The function receives a single argument, the
**	base64-encoded challenge.  The callback function must return
**	a malloced pointer to the base64-encoded response, or NULL to abort
**	SASL.
**
**	authsasl returns two values, provided via call by reference:
**	the authtype and authdata, suitable for direct arguments to
**	auth_generic().
*/

int auth_sasl(const char *,		/* Method */
	      const char *,		/* Initial response - base64encoded */
	      char *(*)(const char *, void *),
					/* Callback conversation functions */
	      void *,			/* Passthrough arg */
	      char **,			/* Returned - AUTHTYPE */
	      char **);			/* Returned - AUTHDATA */

/*
** auth_sasl_ex() is a version of auth_sasl that takes an extra parameter,
** externalauth. If method is "EXTERNAL" and externalauth is not a NULL pointer
** and does not point to an empty string, it is recognized as a SASL EXTERRNAL
** authentication.
*/

int auth_sasl_ex(const char *method,
		 const char *initresponse,
		 const char *externalauth, /* out-of-band authentified identity */
		 char *(*callback_func)(const char *, void *),
		 void *callback_arg,
		 char **authtype_ptr,
		 char **authdata_ptr);

	/*
	** Given authtype and authdata, return the userid in the authentication
	** request. Returns a malloced buffer.
	*/
char *auth_sasl_extract_userid(const char *authtype,
			       const char *authdata);



	/* INTERNAL FUNCTIONS BELOW */

/*
** The authsasl_info is built dynamically by configure, it lists the supported
** SASL methods.  Each method is implemented by a function that's prototyped
** like this:
**
**  int authsasl_function(const char *method, const char *initresponse,
**      char *(*getresp)(const char *),
**
**	char **authtype,
**	char **authdata)
**
** Normally, there's no need to call the appropriate function directly, as
** authsasl() automatically searches this array, and finds one.
**
*/

struct authsasl_info {
	const char *sasl_method;	/* In uppercase */
	int (*sasl_func)(const char *method, const char *initresponse,
			 char *(*getresp)(const char *, void *),
			 void *,
			 char **,
			 char **);
	} ;

extern struct authsasl_info authsasl_list[];
/* Some convenience functions */

char *authsasl_tobase64(const char *, int);
int authsasl_frombase64(char *);

/* Return values from authsasl */

#define	AUTHSASL_OK	0
#define	AUTHSASL_ERROR	-1	/*
				** System error, usually malloc failure,
				** authsasl reports the error to stderr.
				*/

#define	AUTHSASL_ABORTED -2	/*
				** SASL exchange aborted. authsasl does NOT
				** report any errors.
				*/

#ifdef	__cplusplus
}
#endif

#endif
