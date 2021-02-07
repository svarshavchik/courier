#ifndef courierauthdebug_h
#define courierauthdebug_h

/*
** Copyright 2002-2004 Double Precision, Inc.  See COPYING for
** distribution information.
*/
#ifdef	__cplusplus
extern "C" {
#endif


#define DEBUG_LOGIN_ENV "DEBUG_LOGIN"
#define DEBUG_MESSAGE_SIZE (1<<10)

void courier_authdebug_login_init( void );
void courier_authdebug_login( int level, const char *fmt, ... );
int courier_authdebug_printf( const char *fmt, ... );
int courier_safe_printf( const char *fmt, ... );
int courier_auth_err( const char *fmt, ... );
struct authinfo;
int courier_authdebug_authinfo(const char *pfx, const struct authinfo *auth,
	const char *clearpasswd, const char *passwd);

extern int courier_authdebug_login_level;

#define DPRINTF if (courier_authdebug_login_level) courier_authdebug_printf

#define DPWPRINTF if (courier_authdebug_login_level >= 2) courier_authdebug_printf

#ifdef	__cplusplus
}
#endif

#endif
