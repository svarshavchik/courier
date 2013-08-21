#ifndef	authmysql_h
#define	authmysql_h

#include	"courier_auth_config.h"
#include	<stdlib.h>
#include	<string.h>
#include	<sys/types.h>
#include	<mysql.h>
#include	<errmsg.h>

struct authmysqluserinfo {
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

extern struct authmysqluserinfo *auth_mysql_getuserinfo(const char *,
							const char *);
extern void auth_mysql_cleanup();

extern int auth_mysql_setpass(const char *, const char *, const char *);

struct authinfo;

extern int auth_mysql_pre(const char *, const char *,
                int (*)(struct authinfo *, void *), void *arg);

#endif
