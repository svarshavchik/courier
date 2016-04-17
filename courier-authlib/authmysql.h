#ifndef	authmysql_h
#define	authmysql_h

#include	"courier_auth_config.h"
#include	<stdlib.h>
#include	<sys/types.h>
#include	<mysql.h>
#include	<errmsg.h>

#include        <string>

class authmysqluserinfo {
 public:
	std::string username;
	std::string fullname;
	std::string cryptpw;
	std::string clearpw;
	std::string home;
	std::string maildir;
	std::string quota;
	std::string options;
	uid_t uid;
	gid_t gid;
};

bool auth_mysql_getuserinfo(const char *username,
			    const char *service,
			    authmysqluserinfo &uiret);

extern void auth_mysql_cleanup();

extern bool auth_mysql_setpass(const char *, const char *, const char *);

struct authinfo;

extern int auth_mysql_pre(const char *, const char *,
                int (*)(struct authinfo *, void *), void *arg);

extern void auth_mysql_enumerate( void(*cb_func)(const char *name,
						 uid_t uid,
						 gid_t gid,
						 const char *homedir,
						 const char *maildir,
						 const char *options,
						 void *void_arg),
				  void *void_arg);


#endif
