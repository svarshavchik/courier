/*
** Copyright 1998 - 1999 S. Varshavchik.  See COPYING for
** distribution information.
*/



#include	"auth.h"

extern int auth_userdb_pre_common(const char *, const char *, int,
        int (*callback)(struct authinfo *, void *),
                        void *arg);

int auth_userdb_pre(const char *userid, const char *service,
        int (*callback)(struct authinfo *, void *),
                        void *arg)
{
	return (auth_userdb_pre_common(userid, service, 1, callback, arg));
}
