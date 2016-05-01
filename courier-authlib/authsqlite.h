#ifndef	authsqlite_h
#define	authsqlite_h

#include	"courier_auth_config.h"

#include	<string>
#include	<sqlite3.h>

class authsqliteuserinfo {

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

	authsqliteuserinfo() : uid(0), gid(0) {}
} ;

extern bool auth_sqlite_getuserinfo(const char *,
				    const char *,
				    authsqliteuserinfo &);

extern void auth_sqlite_cleanup();

extern int auth_sqlite_setpass(const char *, const char *, const char *);

#endif
