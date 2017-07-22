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

#define	ISFINALLINE(p)	( isdigit((int)(unsigned char)p[0]) &&	\
			isdigit((int)(unsigned char)p[1]) && \
			isdigit((int)(unsigned char)p[2]) \
				&& p[3] == ' ')

struct esmtp_info {

	void (*log_talking)(struct esmtp_info *, void *);
	void (*log_sent)(struct esmtp_info *, const char *, void *);
	void (*log_reply)(struct esmtp_info *, const char *, void *);
	void (*log_smtp_error)(struct esmtp_info *, const char *, int, void *);

	void (*report_broken_starttls)(struct esmtp_info *,
				       const char *,
				       void *);
	int (*lookup_broken_starttls)(struct esmtp_info *,
				      const char *,
				      void *);
	int (*is_local_or_loopback)(struct esmtp_info *,
				    const char *,
				    const char *,
				    void *);
	int (*get_sourceaddr)(struct esmtp_info *info,
			      const RFC1035_ADDR *dest_addr,
			      RFC1035_ADDR *source_addr,
			      void *arg);

	char *host;
	char *smtproute;
	int smtproutes_flags;
	int esmtp_sockfd;

	time_t esmtpkeepaliveping;
	time_t quit_timeout;
	time_t connect_timeout;
	time_t helo_timeout;
	time_t data_timeout;
	time_t cmd_timeout;
	time_t delay_timeout;

	RFC1035_ADDR sockfdaddr;
	RFC1035_ADDR laddr;

	char *sockfdaddrname;

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
	time_t	esmtp_timeout_time;

	time_t net_timeout;
	/*
	** If all MXs are unreachable, wait until this tick before attempting
	** any new connections.
	*/
	int net_error;

	struct mybuf esmtp_sockbuf;

	char socklinebuf[sizeof(((struct mybuf *)0)->buffer)+1];
	unsigned socklinesize;

	char esmtp_writebuf[BUFSIZ];
	char *esmtp_writebufptr;
	unsigned esmtp_writebufleft;
};

extern struct esmtp_info *esmtp_info_alloc(const char *host);
extern void esmtp_info_free(struct esmtp_info *);

extern int esmtp_connected(struct esmtp_info *);
extern void esmtp_disconnect(struct esmtp_info *);
extern void esmtp_init(struct esmtp_info *);
extern void esmtp_timeout(struct esmtp_info *info, unsigned nsecs);
extern void esmtp_wait_rw(struct esmtp_info *info, int *waitr, int *waitw);
extern int esmtp_wait_read(struct esmtp_info *info);
extern int esmtp_wait_write(struct esmtp_info *info);
extern int esmtp_writeflush(struct esmtp_info *info);
extern int esmtp_dowrite(struct esmtp_info *info, const char *, unsigned);
extern int esmtp_writestr(struct esmtp_info *info, const char *);

extern const char *esmtp_readline(struct esmtp_info *info);

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
extern int esmtp_connect(struct esmtp_info *info, void *arg);
extern void esmtp_quit(struct esmtp_info *, void *arg);
extern int esmtp_parsereply(struct esmtp_info *info,
			    const char *cmd,
			    void *arg);

#endif
