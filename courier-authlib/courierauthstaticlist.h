/*
** Copyright 1998 - 2004 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#ifndef	courierauthstaticlist_h
#define	courierauthstaticlist_h

#include	<sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif


struct authinfo;

struct authstaticinfo {
	const char *auth_name;
	int (*auth_func)(const char *, const char *, char *,
			 int (*)(struct authinfo *, void *),
			 void *);
	int (*auth_prefunc)(const char *, const char *,
			    int (*)(struct authinfo *, void *),
			    void *);
	void (*auth_cleanupfunc)();
	int (*auth_changepwd)(const char *, /* service */
			      const char *, /* userid */
			      const char *, /* oldpassword */
			      const char *); /* new password */

	void (*auth_idle)();
	/* Not null - gets called every 5 mins when we're idle */

	void (*auth_enumerate)( void(*cb_func)(const char *name,
					       uid_t uid,
					       gid_t gid,
					       const char *homedir,
					       const char *maildir,
					       const char *options,
					       void *void_arg),
				void *void_arg);
	} ;

extern int auth_syspasswd(const char *,
			  const char *,
			  const char *,
			  const char *);

#ifdef	__cplusplus
}
#endif

#endif
