/*
** Copyright 1998 - 2006 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif

#include	"courier.h"
#include	"rw.h"
#include	"rfc822/rfc822.h"
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	<errno.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#if	HAVE_FCNTL_H
#include	<fcntl.h>
#endif
#include	<stdio.h>
#include	<pwd.h>
#include	"waitlib/waitlib.h"
#include	"modulelist.h"
#include	"sysconfdir.h"
#include	<courierauth.h>

#if	HAVE_SYSLOG_H
#include	<syslog.h>
#else
#define	syslog(a, b)
#endif

#define DEFAULTNAME	"alias@"


extern char *local_dotcourier(const char *, const char *, const char **);

static void rw_local(struct rw_info *, void (*)(struct rw_info *));
static void rw_del_local(struct rw_info *, void (*)(struct rw_info *),
		void (*)(struct rw_info *, const struct rfc822token *,
			const struct rfc822token *));

static int rw_local_filter(const char *,	/* Sending module */
			int,			/* File descriptor */
			const char *,		/* Host */
			const char *,		/* Address */
			const char *,		/* Envelope sender */
			char *,			/* Buffer for optional msg */
			unsigned);		/* Sizeof(buffer) */

struct rw_list *local_rw_install(const struct rw_install_info *p)
{
static struct rw_list local_info={0, "module.local - " COURIER_COPYRIGHT,
			 rw_local, rw_del_local, rw_local_filter};

	return (&local_info);
}

const char *local_rw_init()
{
	return (0);
}

static void rw_local(struct rw_info *p, void (*func)(struct rw_info *))
{
	if (p->mode & RW_SUBMIT)
	{
		if (p->mode & RW_ENVSENDER)
		{
		char	*q=rfc822_gettok(p->ptr);

			if (!q)	clog_msg_errno();

/*********************************************************************
**
** To filter messages originating from the command line based on the
** sender, insert code here to call p->rw_info to reject the message.
**
** The sender can be identified via getuid().  Note, though, that the
** process is running setgid to MAILGID.
**
*********************************************************************/

			free(q);
		}
	}

	rw_local_defaulthost(p, func);
}

static int local_callback(struct authinfo *, void *);

struct localauthinfo {
	const char *address;
	const char *ext;
	struct rw_info *rwi;
	void (*delfunc)(struct rw_info *, const struct rfc822token *,
			const struct rfc822token *);
	int found;
	int exists;
} ;

static int cleanup_set=0;

/* Before terminating, nicely shut down everything */

static void cleanup()
{
}

/*

   Accept delivery for a local address, if:

   A) the address does NOT contain the ! character, AND
   B) the address does not contain the @ character, or the domain after the @
      is local.

*/

static void rw_del_local(struct rw_info *rwi,
			void (*nextfunc)(struct rw_info *),
		void (*delfunc)(struct rw_info *, const struct rfc822token *,
				const struct rfc822token *))
{
struct rfc822token *p, **prevp;
char	*addr, *ext;
int	i;
struct localauthinfo lai;
char	*hostdomain=NULL;
struct	rfc822token	hostdomaint;
#if LOCAL_EXTENSIONS
char *atdomain;
#endif

	if (!cleanup_set)
	{
		atexit(cleanup);
		cleanup_set=1;
	}

	prevp=0;
	for (p= *(prevp=&rwi->ptr); p; p= *(prevp= &p->next))
	{
		if (p->token == '!')
		{
			(*nextfunc)(rwi);
			return;
		}
		if (p->token == '@')	break;
	}

	if (p)
	{
		/* If not a local domain, ignore */

		if (!configt_islocal(p->next, &hostdomain))
		{
			(*nextfunc)(rwi);
			return;
		}

		if (hostdomain)
		{
			char *pp;

			p->next= &hostdomaint;

			for (pp=hostdomain; *pp; pp++)
				*pp=tolower((int)(unsigned char)*pp);

			hostdomaint.next=0;
			hostdomaint.token=0;
			hostdomaint.ptr=hostdomain;
			hostdomaint.len=strlen(hostdomain);
		}
		else
			*prevp=0;
	}

	/* Remove quotes from the local portion */

	for (p=rwi->ptr; p; p=p->next)
		if (p->token == '"')	p->token=0;
	addr=rfc822_gettok(rwi->ptr);

	if (!addr)	clog_msg_errno();
	locallower(addr);

	if (strchr(addr, '!') || *addr == 0)
	{
		/* Can't have !s in an address, that's used a delimiter
		** for the returned data.
		** Can't have NULL addresses either.
		*/

		goto not_found;
	}

	/*
	** We do not accept local addresses that start with a period.  That is
	** reserved for legacy alias handling.  If this is called from
	** submit, we reject explicit addresses of that form.  These addresses
	** are expanded from aliases, and then are inserted without calling
	** the rewriting functions.  Subsequently we get here when
	** we're in courierd, but RW_SUBMIT is not set.
	*/

	if (*addr == '.')
	{
		if ((rwi->mode & (RW_SUBMIT|RW_SUBMITALIAS))
			== RW_SUBMIT)
			goto not_found;		/* Sorry */

		goto toalias;			/* Sorry again */
	}

#if LOCAL_EXTENSIONS
	atdomain=strrchr(addr, '@');
	if (atdomain)
		*atdomain++=0;
#endif

	i=0;
	for (ext=addr; *ext; ext++)
	{
#if LOCAL_EXTENSIONS
		if (*ext == '-' && ++i > 3)
			break;
#endif
	}

	for (;;)
	{
	char *search_addr=addr;

#if LOCAL_EXTENSIONS
	char	c;
	char *alloc_buf=0;

		c= *ext;
		*ext=0;

		lai.ext=c ? ext+1:ext;

		if (atdomain)
		{
			alloc_buf=malloc(strlen(addr)+strlen(atdomain)+2);
			if (!alloc_buf)
				clog_msg_errno();
			strcat(strcat(strcpy(alloc_buf, addr), "@"),
			       atdomain);
			search_addr=alloc_buf;
		}
#else
		lai.ext=0;
#endif
		lai.address=search_addr;
		lai.delfunc=delfunc;
		lai.rwi=rwi;
		lai.found=0;

		i=auth_getuserinfo("courier", search_addr,
				   local_callback, &lai);

#if LOCAL_EXTENSIONS
		if (alloc_buf)
			free(alloc_buf);
#endif
		if (i == 0)
		{
			if (lai.exists)
			{
				free(addr);
				if (hostdomain)	free(hostdomain);
				return;
			}
			goto not_found;
		}

		if (i > 0)
		{
			free(addr);
			if (hostdomain)	free(hostdomain);
			(*rwi->err_func)(450,
				"Service temporarily unavailable.", rwi);
			return;
		}
#if LOCAL_EXTENSIONS
		*ext=c;

		while (ext > addr)
			if (*--ext == '-')	break;
		if (ext == addr)
		{
			if (atdomain)
			{
				lai.ext=addr;
				lai.delfunc=delfunc;
				lai.rwi=rwi;
				lai.found=0;

				alloc_buf=malloc(sizeof(DEFAULTNAME)
						 +strlen(atdomain));
				if (!alloc_buf)
					clog_msg_errno();

				strcat(strcpy(alloc_buf, DEFAULTNAME),
				       atdomain);

				i=auth_getuserinfo("courier",
						   alloc_buf,
						   &local_callback, &lai);
				free(alloc_buf);
				if (i == 0)
				{
					if (lai.exists)
					{
						free(addr);
						if (hostdomain)
							free(hostdomain);
						return;
					}
					goto not_found;
				}
				if (i > 0)
				{
					free(addr);
					if (hostdomain)	free(hostdomain);
					(*rwi->err_func)(450,
							 "Service temporarily unavailable.", rwi);
					return;
				}
			}
			break;
		}
#else
		break;
#endif
	}

#if LOCAL_EXTENSIONS
	if (atdomain)
		atdomain[-1]='@';	/* Put back what was taken */
#endif

	/* Try to deliver to an alias defined in ${sysconfdir}/aliasdir */

toalias:

	{
	struct	authinfo aa;
	static const uid_t	mailuid=MAILUID;

		memset(&aa, 0, sizeof(aa));
		aa.homedir= ALIASDIR;
		aa.sysuserid= &mailuid;
		aa.sysgroupid=MAILGID;
		lai.address=aa.address="alias";

		lai.ext=addr;
		lai.delfunc=delfunc;
		lai.rwi=rwi;
		lai.found=0;

		i=local_callback( &aa, &lai );

		if (i == 0)
		{
			if (lai.exists)
			{
				free(addr);
				if (hostdomain)	free(hostdomain);
				return;
			}
			goto not_found;
		}

		if (i > 0)
		{
			free(addr);
			if (hostdomain)	free(hostdomain);
			(*rwi->err_func)(450,
				"Service temporarily unavailable.", rwi);
			return;
		}
	}

not_found:
	/*
	** When submit is being called by the sendmail command line,
	** don't reject unknown addresses, instead accept them (and bounce
	** them later, when we get here from courierd's main loop).
	** Otherwise, couriermlm will barf when a local address disappears.
	*/

	if ((rwi->mode & (RW_SUBMIT|RW_EXPN|RW_VERIFY)) == RW_SUBMIT &&
	    rwi->smodule &&
	    (strcmp(rwi->smodule, "local") == 0 ||
	     strcmp(rwi->smodule, "uucp") == 0))
	{
		free(addr);
		(*delfunc)(rwi, rwi->ptr, rwi->ptr);
		if (hostdomain)	free(hostdomain);

		return;
	}

	{
		char buf[256];

		char *orig_addr=rfc822_gettok(rwi->ptr);

		buf[255]=0;

		snprintf(buf, 255, "User <%s> unknown",
			 orig_addr ? orig_addr:"");
		free(addr);
		if (hostdomain)	free(hostdomain);

		if (orig_addr)
			free(orig_addr);
		(*rwi->err_func)(550, buf, rwi);
	}
	return;
}


static int local_callback(struct authinfo *a, void *vp)
{
struct localauthinfo *lai=(struct localauthinfo *)vp;
struct	rfc822token tacct, text, tuid, tgid, thomedir, tmaildir, tquota, trecip;
struct	rfc822token te1, te2, te3, te4, te5, te6;
char	ubuf[40], gbuf[40];
char	*p;
uid_t	un;
gid_t	gn;

	lai->found=1;
	lai->exists=0;

	if (lai->ext && *lai->ext)
	{
	char	*ext=local_dotcourier(a->homedir, lai->ext, 0);

		if (!ext && errno == ENOENT)
			return (0);	/* Not found */

		if (ext)	free(ext);
	}
	if (a->homedir == 0 || strchr(a->homedir, '\n') ||
		strchr(a->homedir, '!') ||
		(a->maildir && (strchr(a->maildir, '\n') ||
			strchr(a->maildir, '!'))))
	{
		syslog(LOG_DAEMON|LOG_CRIT,
			"Invalid homedir or maildir for %s - has \\n, !, is null.",
				lai->address);
		return (1);
	}

	lai->exists=1;
/*
	Ok, we return the following information:

Host:

	account ! ext ! uid ! gid ! homedir ! maildir ! quota

Ext:
	non standard mailbox
*/
	tacct.next= &te1;
	tacct.token=0;
	tacct.ptr=a->address;
	tacct.len=strlen(tacct.ptr);

	te1.next= &text;
	te1.token='!';
	te1.ptr="!";
	te1.len=1;

	text.next= &te2;
	text.token=0;
	text.ptr=lai->ext;
	if (text.ptr == 0)	text.ptr="";
	text.len=strlen(text.ptr);

	te2.next= &tuid;
	te2.token='!';
	te2.ptr="!";
	te2.len=1;

	if (a->sysuserid)
		un= *a->sysuserid;
	else
	{
	struct	passwd *pw=getpwnam(a->sysusername);

		if (!pw)
		{
			syslog(LOG_DAEMON|LOG_CRIT,
				"getpw(%s) failed - returned by authlib.",
					a->sysusername);

			return (1);
		}
		un= pw->pw_uid;
	}

#if ALLOW_ROOT

#else
	if (un == 0)
	{
		return (-1);		/* Do not deliver to root */
	}
#endif
	gn=a->sysgroupid;

	p=ubuf+sizeof(ubuf)-1;
	*p=0;

	do
	{
		*--p= "0123456789"[ un % 10];
		un=un/10;
	} while (un);

	tuid.next=&te3;
	tuid.token=0;
	tuid.ptr=p;
	tuid.len=strlen(p);

	te3.next= &tgid;
	te3.token='!';
	te3.ptr="!";
	te3.len=1;

	p=gbuf+sizeof(gbuf)-1;
	*p=0;
	do
	{
		*--p= "0123456789"[ gn % 10];
		gn=gn/10;
	} while (gn);

	tgid.next= &te4;
	tgid.token=0;
	tgid.ptr=p;
	tgid.len=strlen(p);

	te4.next= &thomedir;
	te4.token='!';
	te4.ptr="!";
	te4.len=1;

	thomedir.next=&te5;
	thomedir.token=0;
	thomedir.ptr= a->homedir;
	thomedir.len=strlen(thomedir.ptr);

	te5.next= &tmaildir;
	te5.token='!';
	te5.ptr="!";
	te5.len=1;

	tmaildir.next=&te6;
	tmaildir.token=0;
	tmaildir.ptr= a->maildir;
	if (tmaildir.ptr == 0)	tmaildir.ptr="";
	tmaildir.len=strlen(tmaildir.ptr);

	te6.next= &tquota;
	te6.token='!';
	te6.ptr="!";

	tquota.next=0;
	tquota.token=0;
	tquota.ptr= a->quota;
	if (tquota.ptr == 0)	tquota.ptr="";
	tquota.len=strlen(tquota.ptr);

	trecip.next=0;
	trecip.token=0;
	trecip.ptr=lai->address;
	trecip.len=strlen(trecip.ptr);

	(*lai->delfunc)(lai->rwi, &tacct, &trecip);
	return (0);
}

/*
** Reject messages to local recipients based upon local criteria.
**
** The difference between this and the check in rw_del_local is that
** this is performed after the message has been received, so we
** now have the message contents available.
**
** To reject the message to this receipient, return a negative value
** for a permenant rejection, and a positive value for a temporary
** rejection (currently unused due to limitations in SMTP).
**
** To accept the message, return 0.
**
** Optional descriptive text can be saved in 'buf', whose size in bytes
** is given by bufsize.
*/

static int rw_local_filter(const char *smodule,
			int fd,
			const char *host,
			const char *addr,
			const char *sender,
			char *buf,
			unsigned bufsize)
{
int	p[2];
pid_t	pid;
const	char *maildrop;
char	errbuf[256];
int	l;
char	*s, *t;
const char *uidgids;
const char *homedir;
const char *defaults;
const char *quota;

	if ((maildrop=config_maildropfilter()) == 0 || *maildrop != '/')
		return (0);

	/* host is account!ext!blah... */

	s=strdup(host);
	if (!s)
	{
		strcpy(buf, "450 malloc() failed.");
		return (1);
	}
	
	host=s;

	t=strchr(s, '!');
	if (!t)
	{
		free(s);
		return (0);
	}
	*t++=0;
	addr=t;
	t=strchr(t, '!');
	if (!t)
	{
		free(s);
		return (0);
	}
	*t++=0;
	uidgids=t;
	t=strchr(t, '!');
	if (!t)
	{
		free(s);
		return (0);
	}
	*t++ = '/';
	t=strchr(t, '!');
	if (!t)
	{
		free(s);
		return (0);
	}
	*t++=0;
	homedir=t;
	t=strchr(t, '!');
	if (!t)
	{
		free(s);
		return (0);
	}
	*t++=0;
	defaults=t;
	t=strchr(t, '!');
	if (!t)
	{
		free(s);
		return (0);
	}
	*t++=0;
	quota=t;

	if (pipe(p) < 0)
	{
		free(s);
		strcpy(buf, "450 pipe() failed.");
		return (1);
	}

	if ((pid=fork()) < 0)
	{
		free(s);
		strcpy(buf, "450 fork() failed.");
		return (1);
	}

	if (pid == 0)
	{
	char	*argv[11];

		close(0);
		if (fd < 0)
			open("/dev/null", O_RDONLY);
		else
		{
			dup2(fd, 0);
			close(fd);
		}
		dup2(p[1], 1);
		dup2(p[1], 2);
		close(p[0]);
		close(p[1]);

		argv[0]=strrchr(maildrop, '/')+1;
		argv[1]="-D";
		argv[2]=(char *)uidgids;
		argv[3]="-M";
		argv[4]=malloc((addr ? strlen(addr):0)+sizeof("smtpfilter-"));
		if (!argv[4])
		{
			printf("450 Out of memory.\n");
			fflush(stdout);
			_exit(0);
		}
		strcpy(argv[4], fd < 0 ? "rcptfilter":"smtpfilter");
		if (addr && *addr)
			strcat(strcat(argv[4], "-"), addr);
		argv[5]=getenv("TCPREMOTEHOST");
		if (!argv[5])	argv[5]="";
		argv[6]=getenv("TCPREMOTEIP");
		if (!argv[6])	argv[6]="";
		argv[7]=(char *)sender;
		if (!argv[7])	argv[7]="";
		putenv(strcat(strcpy(courier_malloc(sizeof("SENDER=")+
					strlen(argv[7])), "SENDER="),
					argv[7]));

		putenv(strcat(strcpy(courier_malloc(sizeof("HOME=")+
			strlen(homedir)), "HOME="), homedir));
		putenv(strcat(strcpy(courier_malloc(sizeof("DEFAULT=")+
			strlen(defaults)), "DEFAULT="), defaults));
		putenv(strcat(strcpy(courier_malloc(sizeof("MAILDIRQUOTA=")+
			strlen(quota)), "MAILDIRQUOTA="), quota));


		argv[8]=getenv("BOUNCE");
		if (!argv[8])	argv[8]="";
		argv[9]=getenv("BOUNCE2");
		if (!argv[9])	argv[9]="";
		argv[10]=0;
		execv(maildrop, argv);
		_exit(0);	/* Assume spam filter isn't installed */
	}
	free(s);

	close(p[1]);

	while ((l=read(p[0], errbuf, sizeof(errbuf))) > 0)
	{
		if (l > bufsize-1)
			l=bufsize-1;
		memcpy(buf, errbuf, l);
		buf += l;
		bufsize -= l;
	}
	close(p[0]);
	*buf=0;

	while (wait(&l) != pid)
		;

	if (WIFEXITED(l))
		return (WEXITSTATUS(l));

	return (1);
}
