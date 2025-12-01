/*
** Copyright 1998 - 2018 Double Precision, Inc.
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
#include	<idn2.h>
#include	"numlib/numlib.h"
#include	"comuidgid.h"

#if	HAVE_SYSLOG_H
#include	<syslog.h>
#else
#define	syslog(a, b)
#endif

extern "C" char *local_dotcourier(const char *, const char *, const char **);

static void rw_local(struct rw_info *, void (*)(struct rw_info *));
static void rw_del_local(struct rw_info *, void (*)(struct rw_info *),
			 void (*)(struct rw_info *, const rfc822::tokens &,
				  const rfc822::tokens &));

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
			std::string addr;
			size_t l=rfc822::tokens::print(
				p->addr.begin(), p->addr.end(),
				rfc822::length_counter{} );
			addr.reserve(l);

			rfc822::tokens::print(p->addr.begin(),
					      p->addr.end(),
					      std::back_inserter(addr));


/*********************************************************************
**
** To filter messages originating from the command line based on the
** sender, insert code here to call p->err_func to reject the message.
**
** The sender can be identified via getuid().  Note, though, that the
** process is running setgid to MAILGID.
**
*********************************************************************/
		}
	}

	rw_local_defaulthost(p, func);
}

static int local_callback(struct authinfo *, void *);

struct localauthinfo {
	std::string address;
	std::string ext;
	struct rw_info *rwi{nullptr};
	void (*delfunc)(struct rw_info *, const rfc822::tokens &,
			const rfc822::tokens &);
	int found{0};
	int exists{0};
} ;

/*

   Accept delivery for a local address, if:

   A) the address does NOT contain the ! character, AND
   B) the address does not contain the @ character, or the domain after the @
      is local.

*/

static void rw_del_local2(struct rw_info *rwi,
			  const std::string &addr,
			  void (*nextfunc)(struct rw_info *),
			  void (*delfunc)(struct rw_info *,
					  const rfc822::tokens &,
					  const rfc822::tokens &));

static void rw_del_local(struct rw_info *rwi,
			 void (*nextfunc)(struct rw_info *),
			 void (*delfunc)(struct rw_info *,
					 const struct rfc822::tokens &,
					 const struct rfc822::tokens &))
{
	auto ab=rwi->addr.begin(), ae=rwi->addr.end(), p=ab;

	for (; p != ae; ++p)
	{
		if (p->type == '!')
		{
			(*nextfunc)(rwi);
			return;
		}
		if (p->type == '@')	break;
	}

	std::string hostdomain;

	if (p != ae)
	{
		/* If not a local domain, ignore */

		if (!configt_islocal(++p, ae, hostdomain))
		{
			(*nextfunc)(rwi);
			return;
		}

		if (!hostdomain.empty())
		{
			rwi->addr.erase(p, ae);

			char *q=ualllower(hostdomain.c_str());

			if (!q)
				clog_msg_errno();
			hostdomain=q;
			free(q);

			rwi->addr.push_back({0, hostdomain});
			ae=rwi->addr.end();
		}
		else
		{
			rwi->addr.erase(--p, ae);
		}
		ae=rwi->addr.end();
	}

	/* Remove quotes from the local portion */

	for (auto &p:rwi->addr)
		if (p.type == '"') p.type=0;

	std::string addr;
	size_t l=rfc822::tokens::print(rwi->addr.begin(), rwi->addr.end(),
				       rfc822::length_counter{} );
	addr.reserve(l);

	rfc822::tokens::print(rwi->addr.begin(), rwi->addr.end(),
			      std::back_inserter(addr));

	{
		char *p=ulocallower(addr.c_str());

		if (!p)
			clog_msg_errno();

		addr=p;
		free(p);
	}

	rw_del_local2(rwi, addr, nextfunc, delfunc);
}

static const char *defaultsep(char c)
{
	return c == '-' ? "-" : strchr(config_defaultsep(), c);
}

static void rw_del_local2(struct rw_info *rwi,
			  const std::string &addr,
			  void (*nextfunc)(struct rw_info *),
			  void (*delfunc)(struct rw_info *,
					  const rfc822::tokens &,
					  const rfc822::tokens &))
{
	size_t ext;
	int	i;
	struct localauthinfo lai;
#if LOCAL_EXTENSIONS
	std::string_view atdomain;
#endif

	std::string_view search_addr{addr};

	if (addr.empty() || addr.find('!') != addr.npos)
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

	if (*addr.c_str() == '.')
	{
		if ((rwi->mode & (RW_SUBMIT|RW_SUBMITALIAS))
			== RW_SUBMIT)
			goto not_found;		/* Sorry */

		goto toalias;			/* Sorry again */
	}

#if LOCAL_EXTENSIONS
	{
		auto p=search_addr.find('@');

		if (p != search_addr.npos)
		{
			atdomain=search_addr.substr(p);
			search_addr=search_addr.substr(0, p);
		}
	}
#endif

	i=0;

	for (ext=0; ext<search_addr.size(); ++ext)
	{
#if LOCAL_EXTENSIONS
		if (defaultsep(search_addr[ext]) && ++i > 3)
			break;
#endif
	}

	for (;;)
	{
#if LOCAL_EXTENSIONS

		lai.ext.assign(
			(search_addr.begin() + (ext < search_addr.size()
						? ext+1:ext)),
			search_addr.end()
		);

		lai.address.assign(
			search_addr.begin(),
			search_addr.begin()+ext
		);

		lai.address += atdomain;
#else
		lai.address=addr;
		lai.ext="";
#endif

		lai.delfunc=delfunc;
		lai.rwi=rwi;
		lai.found=0;

		i=auth_getuserinfo("courier", lai.address.c_str(),
				   local_callback, &lai);

		if (i == 0)
		{
			if (lai.exists)
			{
				return;
			}
			goto not_found;
		}

		if (i > 0)
		{
			(*rwi->err_func)(450,
				"Service temporarily unavailable.", rwi);
			return;
		}

#if LOCAL_EXTENSIONS

		while (ext > 0)
			if (defaultsep(search_addr[--ext])) break;

		if (ext == 0)
		{
			if (!atdomain.empty())
			{
				lai.ext=addr;
				lai.delfunc=delfunc;
				lai.rwi=rwi;
				lai.found=0;

				lai.address="alias";
				lai.address += atdomain;

				i=auth_getuserinfo("courier",
						   lai.address.c_str(),
						   &local_callback, &lai);
				if (i == 0)
				{
					if (lai.exists)
					{
						return;
					}
					goto not_found;
				}
				if (i > 0)
				{
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

	/* Try to deliver to an alias defined in ${sysconfdir}/aliasdir */

toalias:

	{
		struct	authinfo aa;
		uid_t	mailuid=MAILUID;

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
				return;
			}
			goto not_found;
		}

		if (i > 0)
		{
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
		(*delfunc)(rwi, rwi->addr, rwi->addr);
		return;
	}

	{
		std::string addr;
		size_t l=rfc822::tokens::print(
			rwi->addr.begin(), rwi->addr.end(),
			rfc822::length_counter{} );
		addr.reserve(l);

		rfc822::tokens::print(rwi->addr.begin(), rwi->addr.end(),
				      std::back_inserter(addr));

		/* This can come out in SMTP, so make it an ACE domain */

		char *orig_addr_ace=udomainace(addr.c_str());

		addr="User <";
		addr += orig_addr_ace;
		free(orig_addr_ace);
		addr += "> unknown";

		(*rwi->err_func)(550, addr.c_str(), rwi);
	}
}

static int local_callback(struct authinfo *a, void *vp)
{
struct localauthinfo *lai=(struct localauthinfo *)vp;
rfc822::tokens tacct, trecip;
char	ubuf[40], gbuf[40];
char	*p;
uid_t	un;
gid_t	gn;

	for (char &c:lai->ext)
	{
		if (defaultsep(c))
			c='-';
	}

	lai->found=1;
	lai->exists=0;

	if (!lai->ext.empty())
	{
		char	*ext=local_dotcourier(a->homedir, lai->ext.c_str(), 0);

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
		       lai->address.c_str());
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
	tacct.reserve(13);
	tacct.push_back({0, a->address});
	tacct.push_back({'!', "!"});
	tacct.push_back({0, lai->ext.c_str()});
	tacct.push_back({'!', "!"});

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

	tacct.push_back({0, p});
	tacct.push_back({'!', "!"});

	p=gbuf+sizeof(gbuf)-1;
	*p=0;
	do
	{
		*--p= "0123456789"[ gn % 10];
		gn=gn/10;
	} while (gn);

	tacct.push_back({0, p});
	tacct.push_back({'!', "!"});
	tacct.push_back({0, a->homedir});
	tacct.push_back({'!', "!"});
	tacct.push_back({0, a->maildir ? a->maildir:""});
	tacct.push_back({'!', "!"});
	tacct.push_back({0, a->quota ? a->quota:""});

	trecip.push_back({0, lai->address});

	(*lai->delfunc)(lai->rwi, tacct, trecip);
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

#ifndef EXEC_MAILDROP
#define EXEC_MAILDROP execv
#endif

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

		argv[0]=const_cast<char *>(strrchr(maildrop, '/')+1);

		static char d_str[]="-D";
		argv[1]=d_str;
		argv[2]=(char *)uidgids;

		static char m_str[]="-M";
		argv[3]=m_str;
		argv[4]=(char *)malloc((addr ? strlen(addr):0)+sizeof("smtpfilter-"));
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

		static char nullstr[]="";
		if (!argv[5])	argv[5]=nullstr;
		argv[6]=getenv("TCPREMOTEIP");
		if (!argv[6])	argv[6]=nullstr;
		argv[7]=(char *)sender;
		if (!argv[7])	argv[7]=nullstr;
		putenv(strcat(strcpy((char *)courier_malloc(sizeof("SENDER=")+
					strlen(argv[7])), "SENDER="),
					argv[7]));

		putenv(strcat(strcpy((char *)courier_malloc(sizeof("HOME=")+
			strlen(homedir)), "HOME="), homedir));
		putenv(strcat(strcpy((char *)courier_malloc(sizeof("DEFAULT=")+
			strlen(defaults)), "DEFAULT="), defaults));
		putenv(strcat(strcpy((char *)courier_malloc(sizeof("MAILDIRQUOTA=")+
			strlen(quota)), "MAILDIRQUOTA="), quota));


		argv[8]=getenv("BOUNCE");
		if (!argv[8])	argv[8]=nullstr;
		argv[9]=getenv("BOUNCE2");
		if (!argv[9])	argv[9]=nullstr;
		argv[10]=0;
		EXEC_MAILDROP(maildrop, argv);
		_exit(0);	/* Assume spam filter isn't installed */
	}
	free(s);

	close(p[1]);

	while ((l=read(p[0], errbuf, sizeof(errbuf))) > 0)
	{
		if ((size_t)l > bufsize-1)
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
