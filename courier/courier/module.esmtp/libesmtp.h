/*
** Copyright 2017 S. Varshavchik.
** See COPYING for distribution information.
*/

#ifndef	libesmtp_h
#define	libesmtp_h

#if	HAVE_CONFIG_H
#include	"config.h"
#endif

#include	<time.h>
#if HAVE_SYS_TIME_H
#include	<sys/time.h>
#endif
#define	mybuf_readfunc	sox_read
#include	"mybuf.h"
#include	"soxwrap/soxwrap.h"
#include	"soxwrap/sconnect.h"
#include	"rfc1035/rfc1035.h"
#include	"rfc1035/rfc1035mxlist.h"
#include	"rfc822/rfc822.h"
#include	"rfc2045/rfc2045.h"
#include	<ctype.h>
#include	<functional>

#define	ISFINALLINE(p)	( isdigit((int)(unsigned char)p[0]) &&	\
			isdigit((int)(unsigned char)p[1]) && \
			isdigit((int)(unsigned char)p[2]) \
				&& p[3] == ' ')

struct rw_info;

struct rw_rewritten_address {
	virtual void operator()(const rfc822::tokens &)=0;
};

struct esmtp_info {

	void (*log_talking)(struct esmtp_info *, void *)=nullptr;
	void (*log_sent)(struct esmtp_info *, const char *, int, void *)=nullptr;
	void (*log_reply)(struct esmtp_info *, const char *, int, void *)=nullptr;
	void (*log_smtp_error)(struct esmtp_info *, const char *, int, void *)=nullptr;
	void (*log_rcpt_error)(struct esmtp_info *, int, int, void *)=nullptr;
	void (*log_net_error)(struct esmtp_info *, int, void *)=nullptr;

	void (*log_success)(struct esmtp_info *, unsigned, const char *,
			    int, void *)=nullptr;
	void (*report_broken_starttls)(struct esmtp_info *,
				       const char *,
				       void *)=nullptr;
	int (*lookup_broken_starttls)(struct esmtp_info *,
				      const char *,
				      void *)=nullptr;
	int (*is_local_or_loopback)(struct esmtp_info *,
				    const char *,
				    const char *,
				    void *)=nullptr;
	int (*get_sourceaddr)(struct esmtp_info *info,
			      const RFC1035_ADDR *dest_addr,
			      RFC1035_ADDR *source_addr,
			      void *arg)=nullptr;

	char *authclientfile=nullptr;

	time_t esmtpkeepaliveping{0};
	time_t quit_timeout{0};
	time_t connect_timeout{0};
	time_t helo_timeout{0};
	time_t data_timeout{0};
	time_t cmd_timeout{0};
	time_t delay_timeout{0};

	void (*log_good_reply)(struct esmtp_info *, const char *, int, void *)
	=nullptr;

	int hasdsn{0};
	int has8bitmime{0};
	int hassmtputf8{0};
	int hasverp{0};
	int hassize{0};
	int hasexdata{0};
	int hascourier{0};
	int hasstarttls{0};
	int hassecurity_starttls{0};

	/* This connection has TLS enabled.  */
	int is_tls_connection{0};

	/* TLS connection with Courier's SECURITY extension. */
	int is_secure_connection{0};

	void (*rewrite_func)(struct rw_info *, void (*)(struct rw_info *))=nullptr;
	std::function<std::string(const rfc822::tokens &,
				  std::string)> rewrite_addr_header;

	std::string host;
	char *smtproute{nullptr};
	int smtproutes_flags{0};
	int esmtp_sockfd{0};

	const char *helohost{nullptr};
	RFC1035_ADDR sockfdaddr{};
	RFC1035_ADDR laddr{};

	char *sockfdaddrname{nullptr};

	int esmtp_cork{0};
	int corked{0};
	int haspipelining{0};

	char *authsasllist{nullptr};
	int auth_error_sent{0};
	int quit_needed{0};
	int rset_needed{0};
	time_t	esmtp_timeout_time{0};

	time_t net_timeout{0};
	/*
	** If all MXs are unreachable, wait until this tick before attempting
	** any new connections.
	*/
	int net_error{0};

	struct mybuf esmtp_sockbuf;

	char socklinebuf[sizeof(((struct mybuf *)0)->buffer)+1]={};
	unsigned socklinesize{0};

	char esmtp_writebuf[BUFSIZ]={};
	char *esmtp_writebufptr{nullptr};
	unsigned esmtp_writebufleft{0};
};

extern struct esmtp_info *esmtp_info_alloc(const char *host);

extern void esmtp_setauthclientfile(struct esmtp_info *, const char *);
extern void esmtp_info_free(struct esmtp_info *);
extern int esmtp_connected(struct esmtp_info *);
extern int esmtp_ping(struct esmtp_info *info);
extern int esmtp_connect(struct esmtp_info *info, void *arg);
extern void esmtp_quit(struct esmtp_info *, void *arg);
extern void esmtp_unicode_required_error(struct esmtp_info *, void *);

extern int esmtp_misccommand(struct esmtp_info *info,
			     const char *cmd,
			     void *arg);


struct esmtp_mailfrom_info {
	const char *sender;
	int is8bitmsg;
	int issmtputf8;
	int dsn_format;
	const char *envid;
	int verp;

	unsigned long msgsize;
};

char *esmtp_mailfrom_cmd(struct esmtp_info *info,
			 struct esmtp_mailfrom_info *mf_info,
			 const char **errmsg);

struct esmtp_rcpt_info {
	const char *address;
	const char *dsn_options;
	const char *orig_receipient;

	const char * const *our_rcpt_error;
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
		      rfc822::fdstreambuf &fdbuf,
		      void *arg);

extern void esmtp_sockipname(struct esmtp_info *, char *);

#endif
