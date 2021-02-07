#ifndef	courierauth_h
#define	courierauth_h

/*
** Copyright 2004 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#include	<sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if 0
}
#endif

/*
** Additional authentication parameters.
**
** Extra parameters passed to auth_meta_* functions.
*/

struct auth_meta {

	/*
	** A list of strings, ending with a NULL pointers.
	**
	** Current supported parameters:
	**
	** TCPREMOTEIP=<ipv4 or ipv6 address>
	**
	** Source of the authentication request.
	*/
	char **envvars;
};

/*
** Create a default auth_meta object.
**
** The TCPREMOTEIP setting is taken from an environment variable of the
** same name, if it set.
*/

struct auth_meta *auth_meta_init_default();

/*
** Free the memory created by auth_meta_init_default().
*/
void auth_meta_destroy_default(struct auth_meta *);

/*
  Callback authentication structure:
*/

struct authinfo {
	const char *sysusername;
	const uid_t *sysuserid;
	gid_t sysgroupid;
	const char *homedir;

	const char *address;
	const char *fullname;
	const char *maildir;
	const char *quota;

	const char *passwd;
	const char *clearpasswd;	/* For authldap */

	const char *options;

	} ;
/*
	Either sysusername or sysuserid may be NULL, but not both of them.
	They, and sysgroupid, specify the authenticated user's system
	userid and groupid.  homedir points to the authenticated user's
	home directory.  address, fullname, and maildir, are obvious.
	quota is populated with any maildir quota (see
	maildir/README.maildirquota).

	'options' is an optional string that contains per-user custom settings.
	See "OPTIONS" above.

	After populating this tructure, the lookup function calls the
	callback function that's specified in its second argument.  The
	callback function receives a pointer to the authinfo structure.

	The callback function also receives a context pointer, which is
	the third argument to the lookup function.

	The lookup function should return a negative value if he userid
	does not exist, a positive value if there was a temporary error
	looking up the userid, or whatever is the return code from the
	callback function, if the user exists.
*/


#define	AUTHTYPE_LOGIN	"login"		/* authdata is userid\npassword\n */
#define	AUTHTYPE_CRAMMD5 "cram-md5"	/* authdata is challenge\nresponse\n */
#define	AUTHTYPE_CRAMSHA1 "cram-sha1"	/* authdata is challenge\nresponse\n */
#define	AUTHTYPE_CRAMSHA256 "cram-sha256"	/* authdata is challenge\nresponse\n */

/* auth_generic: INTERNAL */

int auth_generic(const char *service,
		 const char *authtype,
		 char *authdata,
		 int (*callback_func)(struct authinfo *, void *),
		 void *callback_arg);

int auth_generic_meta(struct auth_meta *meta,
		      const char *service,
		      const char *authtype,
		      char *authdata,
		      int (*callback_func)(struct authinfo *, void *),
		      void *callback_arg);


/* Login request: */
int auth_login(const char *service,
	       const char *userid,
	       const char *passwd,
	       int (*callback_func)(struct authinfo *, void *),
	       void *callback_arg);

int auth_login_meta(struct auth_meta *meta,
		    const char *service,
		    const char *userid,
		    const char *passwd,
		    int (*callback_func)(struct authinfo *, void *),
		    void *callback_arg);

/* Return account info: */
int auth_getuserinfo(const char *service, const char *uid,
		     int (*callback)(struct authinfo *, void *),
		     void *arg);

int auth_getuserinfo_meta(struct auth_meta *meta,
			  const char *service, const char *uid,
			  int (*callback)(struct authinfo *, void *),
			  void *arg);

	/* Enumerate accounts */
void auth_enumerate( void(*cb_func)(const char *name,
				    uid_t uid,
				    gid_t gid,
				    const char *homedir,
				    const char *maildir,
				    const char *options,
				    void *void_arg),
		     void *void_arg);

	/* Change the password */
int auth_passwd(const char *service,
		const char *uid,
		const char *opwd,
		const char *npwd);

/* Utility function: parse OPTIONS string for a particular keyword */

extern int auth_getoptionenvint(const char *keyword);
extern char *auth_getoptionenv(const char *keyword);
extern char *auth_getoption(const char *options, const char *keyword);


	/*
	** Utility function: typical action in a callback for auth_generic
	** or auth_login.  Does the following:
	**
	** Drops root, takes uid/gid in ainfo.
	**
	** Changes current directory to the home directory.
	**
	** Returns: <0 - fatal error before dropping root.
	**          >0 - fatal error after dropping root.
	**          =0 - all's OK.
	*/

int auth_callback_default(struct authinfo *ainfo);

/*
** If the AUTH_MKHOMEDIR_SKEL environment variable is set, and the
** authenticated account's home directory does not exist, the home directory
** gets created, with its initial contents copied from AUTH_MKHOMEDIR_SKEL
** which must be a directory, typically /etc/skel.
*/

int auth_mkhomedir(struct authinfo *info);

/*
** Like auth_callback_default, but calls auth_mkhomedir().
*/

int auth_callback_default_autocreate(struct authinfo *ainfo);

	/* Utility function: escape LDAP special characters */

char *courier_auth_ldap_escape(const char *str);

struct cram_callback_info {
	struct hmac_hashinfo *h;
	char *user;
	char *challenge;
	char *response;
	int (*callback_func)(struct authinfo *, void *);
	void *callback_arg;
	};

extern int auth_cram_callback(struct authinfo *a, void *vp);

/*
** auth_get_cram parses out an authentication request.  It checks whether
** we have the requisite hash function installed, and, if so, base64decodes
** the challenge and the response.
*/

struct hmac_hashinfo;

int auth_get_cram(const char *authtype,			/* authtype */
		  char *authdata,			/* authdata */

		  struct cram_callback_info *craminfo);
		/* Initializes craminfo */

/* auth_get_cram_silent() is auth_get_cram(), but without logging */

int auth_get_cram_silent(const char *authtype, char *authdata,
			 struct cram_callback_info *craminfo);

/*
** auth_verify_cram attempts to verify the secret cookie.
*/

int auth_verify_cram(struct hmac_hashinfo *,	/* The hash function */
	const char *,				/* The challenge */
	const char *,				/* The response */
	const char *);				/* Hashed secret, in hex */

#if 0
{
#endif

#ifdef	__cplusplus
}
#endif

#endif
