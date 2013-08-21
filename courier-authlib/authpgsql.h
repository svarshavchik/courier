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
struct authpgsqluserinfo {
	char *username;
	char *fullname;
	char *cryptpw;
	char *clearpw;
	char *home;
	char *maildir;
	char *quota;
	char *options;
	uid_t uid;
	gid_t gid;
	} ;

extern struct authpgsqluserinfo *auth_pgsql_getuserinfo(const char *,
							const char *service);
extern void auth_pgsql_cleanup();

extern int auth_pgsql_setpass(const char *, const char *, const char *);

struct authinfo;

extern int auth_pgsql_pre(const char *, const char *,
                int (*)(struct authinfo *, void *), void *arg);

#endif
