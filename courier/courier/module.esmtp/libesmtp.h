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

struct rw_info;

struct esmtp_info {

	void (*log_talking)(struct esmtp_info *, void *);
	void (*log_sent)(struct esmtp_info *, const char *, int, void *);
	void (*log_reply)(struct esmtp_info *, const char *, int, void *);
	void (*log_smtp_error)(struct esmtp_info *, const char *, int, void *);
	void (*log_rcpt_error)(struct esmtp_info *, int, int, void *);
	void (*log_net_error)(struct esmtp_info *, int, void *);

	void (*log_success)(struct esmtp_info *, unsigned, const char *,
			    int, void *);
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

	char *authclientfile;

	time_t esmtpkeepaliveping;
	time_t quit_timeout;
	time_t connect_timeout;
	time_t helo_timeout;
	time_t data_timeout;
	time_t cmd_timeout;
	time_t delay_timeout;

	void (*log_good_reply)(struct esmtp_info *, const char *, int, void *);

	int hasdsn;
	int has8bitmime;
	int hasverp;
	int hassize;
	int hasexdata;
	int hascourier;
	int hasstarttls;
	int hassecurity_starttls;
	int is_secure_connection;

	void (*rewrite_func)(struct rw_info *, void (*)(struct rw_info *));

	char *host;
	char *smtproute;
	int smtproutes_flags;
	int esmtp_sockfd;

	const char *helohost;
	RFC1035_ADDR sockfdaddr;
	RFC1035_ADDR laddr;

	char *sockfdaddrname;

	int esmtp_cork;
	int corked;
	int haspipelining;

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

extern struct esmtp_info *esmtp_info_alloc(const char *host,
					   const char *smtproute,
					   int smtproutes_flags);

extern void esmtp_setauthclientfile(struct esmtp_info *, const char *);
extern void esmtp_info_free(struct esmtp_info *);
extern int esmtp_connected(struct esmtp_info *);
extern int esmtp_ping(struct esmtp_info *info);
extern int esmtp_connect(struct esmtp_info *info, void *arg);
extern void esmtp_quit(struct esmtp_info *, void *arg);

extern int esmtp_misccommand(struct esmtp_info *info,
			     const char *cmd,
			     void *arg);


struct esmtp_mailfrom_info {
	const char *sender;
	int is8bitmsg;
	int dsn_format;
	const char *envid;
	int verp;

	unsigned long msgsize;
};

struct esmtp_rcpt_info {
	const char *address;
	const char *dsn_options;
	const char *orig_receipient;
};

extern char *esmtp_rcpt_create(struct esmtp_info *,
			       struct esmtp_rcpt_info *,
			       size_t);

extern char *esmtp_rcpt_data_create(struct esmtp_info *,
				    struct esmtp_rcpt_info *,
				    size_t);

extern int esmtp_send(struct esmtp_info *info,
		      struct esmtp_mailfrom_info *mf_info,
		      struct esmtp_rcpt_info *rcpt_info,
		      size_t nreceipients,
		      int fd,
		      void *arg);

extern void esmtp_sockipname(struct esmtp_info *, char *);

#endif
