#ifndef	authsqlite_h
#define	authsqlite_h

#include	"courier_auth_config.h"
#include	<stdlib.h>
#include	<string.h>
#include	<sys/types.h>
#include	<sqlite3.h>

struct authsqliteuserinfo {
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

extern struct authsqliteuserinfo *auth_sqlite_getuserinfo(const char *,
							  const char *);
extern void auth_sqlite_cleanup();

extern int auth_sqlite_setpass(const char *, const char *, const char *);

struct authinfo;

extern int auth_sqlite_pre(const char *, const char *,
			   int (*)(struct authinfo *, void *), void *arg);

#endif
