/*
** Copyright 2017 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	libesmtp_h
#define	libesmtp_h

#if	HAVE_CONFIG_H
#include	"config.h"
#endif

#if TIME_WITH_SYS_TIME
#include	<sys/time.h>
#include	<time.h>
#else
#if HAVE_SYS_TIME_H
#include	<sys/time.h>
#else
#include	<time.h>
#endif
#endif
#define	mybuf_readfunc	sox_read
#include	"mybuf.h"
#include	"soxwrap/soxwrap.h"
#include	"soxwrap/sconnect.h"
#include	"rfc1035/rfc1035.h"
#include	"rfc1035/rfc1035mxlist.h"

#include	<ctype.h>

#define	ISFINALLINE(p)	( isdigit((int)(unsigned char)p[0]) && \
			isdigit((int)(unsigned char)p[1]) && \
			isdigit((int)(unsigned char)p[2]) \
				&& p[3] == ' ')

struct esmtp_info {

	void (*log_talking)(struct esmtp_info *, void *);
	void (*log_sent)(struct esmtp_info *, const char *, void *);
	void (*log_reply)(struct esmtp_info *, const char *, void *);
	void (*log_smtp_error)(struct esmtp_info *, const char *, int, void *);

	char *host;
	char *smtproute;
	int smtproutes_flags;

	RFC1035_ADDR laddr;

	int haspipelining;
	int hasdsn;
	int has8bitmime;
	int hasverp;
	int hassize;
	int hasexdata;
	int hascourier;
	int hasstarttls;
	int hassecurity_starttls;
	int is_secure_connection;

	char *authsasllist;
	int auth_error_sent;
	int quit_needed;


	time_t net_timeout;
	/*
	** If all MXs are unreachable, wait until this tick before attempting
	** any new connections.
	*/
	int net_error;
};

struct esmtp_info *esmtp_info_alloc(const char *host);
void esmtp_info_free(struct esmtp_info *);

extern void esmtp_init();
extern void esmtp_timeout(struct esmtp_info *info, unsigned nsecs);
extern int esmtp_sockfd;
extern time_t	esmtp_timeout_time;
extern struct mybuf esmtp_sockbuf;
extern void esmtp_wait_rw(int *waitr, int *waitw);
extern int esmtp_wait_read();
extern int esmtp_wait_write();
extern int esmtp_writeflush();
extern int esmtp_dowrite(const char *, unsigned);
extern int esmtp_writestr(const char *);
extern char esmtp_writebuf[BUFSIZ];
extern char *esmtp_writebufptr;
extern unsigned esmtp_writebufleft;

extern const char *esmtp_readline();

extern int esmtp_helo(struct esmtp_info *info, int using_tls,
		      void *arg);
extern int esmtp_get_greeting(struct esmtp_info *info,
			      void *arg);
extern int esmtp_enable_tls(struct esmtp_info *,
			    const char *,
			    int,
			    void *arg);
extern int esmtp_auth(struct esmtp_info *info,
		      const char *auth_key,
		      void *arg);

#endif
