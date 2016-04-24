#ifndef	authpgsql_h
#define	authpgsql_h

#include	"courier_auth_config.h"
#include	<stdlib.h>
#include	<string.h>
#include	<sys/types.h>
#include	<libpq-fe.h>

/*
#include	<errmsg.h>
*/

extern int auth_pgsql_login(const char *service, char *authdata,
			    int (*callback_func)(struct authinfo *, void *),
			    void *callback_arg);
extern int auth_pgsql_changepw(const char *service, const char *user,
			       const char *pass,
			       const char *newpass);

extern void auth_pgsql_cleanup();

struct authinfo;

extern int auth_pgsql_pre(const char *, const char *,
                int (*)(struct authinfo *, void *), void *arg);

#endif
