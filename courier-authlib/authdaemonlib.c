/*
** Copyright 2000-2020 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#include	"auth.h"
#include	"courierauthstaticlist.h"
#include	"courierauthsasl.h"
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<fcntl.h>
#include        <sys/types.h>
#include        <sys/socket.h>
#include        <sys/un.h>
#include        <sys/time.h>
#if	HAVE_SYS_SELECT_H
#include	<sys/select.h>
#endif
#include        <unistd.h>
#include        <stdlib.h>
#include        <stdio.h>
#include        <errno.h>
#include	"authdaemonrc.h"
#include	"numlib/numlib.h"

static int TIMEOUT_SOCK=10,
	TIMEOUT_WRITE=10,
	TIMEOUT_READ=30;

static int auth_meta_init_default_envvar(char **p, size_t *i,
					const char *n)
{
	const char *v=getenv(n);

	if (!v)
		return 0;

	if ((p[*i]=malloc(strlen(v)+strlen(n)+2)) == 0)
	{
		return -1;
	}

	strcat(strcat(strcpy(p[*i], n), "="), v);
	++*i;
	return 0;
}

struct auth_meta *auth_meta_init_default()
{
	struct auth_meta *p=malloc(sizeof(struct auth_meta));
	size_t i;

	if (!p)
		return 0;

	if ((p->envvars=malloc(sizeof(char *)*2)) == 0)
	{
		free(p);
		return 0;
	}

	i=0;

	if (auth_meta_init_default_envvar(p->envvars, &i, "TCPREMOTEIP"))
	{
		while (i)
		{
			free(p->envvars[--i]);
		}
		free(p->envvars);
		free(p);
		return 0;
	}

	p->envvars[i]=0;
	return p;
}

void auth_meta_destroy_default(struct auth_meta *ptr)
{
	if (ptr->envvars)
	{
		size_t i;

		for (i=0; ptr->envvars[i]; ++i)
			free(ptr->envvars[i]);

		free(ptr->envvars);
	}
	free(ptr);
}

static int s_connect(int sockfd,
		     const struct sockaddr *addr,
		     size_t addr_s,
		     time_t connect_timeout)
{
	fd_set fdr;
	struct timeval tv;
	int	rc;

#ifdef SOL_KEEPALIVE
	setsockopt(sockfd, SOL_SOCKET, SOL_KEEPALIVE,
		   (const char *)&dummy, sizeof(dummy));
#endif

#ifdef  SOL_LINGER
        {
		struct linger l;

                l.l_onoff=0;
                l.l_linger=0;

                setsockopt(sockfd, SOL_SOCKET, SOL_LINGER,
			   (const char *)&l, sizeof(l));
        }
#endif

        /*
        ** If configuration says to use the kernel's timeout settings,
        ** just call connect, and be done with it.
        */

        if (connect_timeout == 0)
                return ( connect(sockfd, addr, addr_s));

        /* Asynchronous connect with timeout. */

        if (fcntl(sockfd, F_SETFL, O_NONBLOCK) < 0)     return (-1);

        if ( connect(sockfd, addr, addr_s) == 0)
        {
                /* That was easy, we're done. */

                if (fcntl(sockfd, F_SETFL, 0) < 0)      return (-1);
                return (0);
        }

	if (errno != EINPROGRESS)
		return -1;

	/* Wait for the connection to go through, until the timeout expires */

        FD_ZERO(&fdr);
        FD_SET(sockfd, &fdr);
        tv.tv_sec=connect_timeout;
        tv.tv_usec=0;

        rc=select(sockfd+1, 0, &fdr, 0, &tv);
        if (rc < 0)     return (-1);

        if (!FD_ISSET(sockfd, &fdr))
        {
                errno=ETIMEDOUT;
                return (-1);
        }

	{
		int     gserr;
		socklen_t gslen = sizeof(gserr);

		if (getsockopt(sockfd, SOL_SOCKET,
			       SO_ERROR,
			       (char *)&gserr, &gslen)==0)
		{
			if (gserr == 0)
				return 0;

			errno=gserr;
		}
	}
	return (-1);
}

static int opensock()
{
	int	s=socket(PF_UNIX, SOCK_STREAM, 0);
	struct  sockaddr_un skun;

	skun.sun_family=AF_UNIX;
	strcpy(skun.sun_path, AUTHDAEMONSOCK);

	if (s < 0)
	{
		perror("CRIT: authdaemon: socket() failed");
		return (-1);
	}

	{
		const char *p=getenv("TIMEOUT_SOCK");
		int n=atoi(p ? p:"0");

		if (n > 0)
			TIMEOUT_SOCK=n;
	}

	{
		const char *p=getenv("TIMEOUT_READ");
		int n=atoi(p ? p:"0");

		if (n > 0)
			TIMEOUT_READ=n;
	}

	{
		const char *p=getenv("TIMEOUT_WRITE");
		int n=atoi(p ? p:"0");

		if (n > 0)
			TIMEOUT_WRITE=n;
	}

	if (s_connect(s, (const struct sockaddr *)&skun, sizeof(skun),
		      TIMEOUT_SOCK))
	{
		perror("ERR: authdaemon: s_connect() failed");
		if (errno == ETIMEDOUT || errno == ECONNREFUSED)
			fprintf(stderr, "ERR: [Hint: perhaps authdaemond is not running?]\n");
		close(s);
		return (-1);
	}
	return (s);
}

static int writeauth(int fd, const char *p, unsigned pl)
{
fd_set  fds;
struct  timeval tv;

	while (pl)
	{
	int     n;

		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		tv.tv_sec=TIMEOUT_WRITE;
		tv.tv_usec=0;
		if (select(fd+1, 0, &fds, 0, &tv) <= 0 || !FD_ISSET(fd, &fds))
			return (-1);
		n=write(fd, p, pl);
		if (n <= 0)     return (-1);
		p += n;
		pl -= n;
	}
	return (0);
}

static void readauth(int fd, char *p, unsigned pl, const char *term)
{
time_t	end_time, curtime;
unsigned len = 0, tlen = strlen(term);

	--pl;

	time(&end_time);
	end_time += TIMEOUT_READ;

	while (pl)
	{
	int     n;
	fd_set  fds;
	struct  timeval tv;

		time(&curtime);
		if (curtime >= end_time)
			break;

		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		tv.tv_sec=end_time - curtime;
		tv.tv_usec=0;
		if (select(fd+1, &fds, 0, 0, &tv) <= 0 || !FD_ISSET(fd, &fds))
			break;

		n=read(fd, p, pl);
		if (n <= 0)
			break;
		p += n;
		pl -= n;
		/* end-of-message detection for authpipe */
		len += n;
		if (len >= tlen && strncmp(p-tlen, term, tlen) == 0)
			break;
		else if (len == 5 && strncmp(p-5, "FAIL\n", 5) == 0)
			break;
	}
	*p=0;
}

int _authdaemondopasswd(int wrfd, int rdfd, char *buffer, int bufsiz)
{
	if (writeauth(wrfd, buffer, strlen(buffer)))
		return 1;

	readauth(rdfd, buffer, bufsiz, "\n");

	if (strcmp(buffer, "OK\n"))
	{
		errno=EPERM;
		return -1;
	}
	return 0;
}

int authdaemondopasswd(char *buffer, int bufsiz)
{
	int s=opensock();
	int rc;

	if (s < 0)
		return (1);
	rc = _authdaemondopasswd(s, s, buffer, bufsiz);
	close(s);
	return rc;
}

int _authdaemondo(int wrfd, int rdfd, const char *authreq,
	int (*func)(struct authinfo *, void *), void *arg)
{
char	buf[BUFSIZ];
char	*p, *q, *r;
struct	authinfo a;
uid_t	u;

	if (writeauth(wrfd, authreq, strlen(authreq)))
	{
		return (1);
	}

	readauth(rdfd, buf, sizeof(buf), "\n.\n");
	memset(&a, 0, sizeof(a));
	a.homedir="";
	p=buf;
	while (*p)
	{
		for (q=p; *q; q++)
			if (*q == '\n')
			{
				*q++=0;
				break;
			}
		if (strcmp(p, ".") == 0)
		{
			return ( (*func)(&a, arg));
		}
		if (strcmp(p, "FAIL") == 0)
		{
			errno=EPERM;
			return (-1);
		}
		r=strchr(p, '=');
		if (!r)
		{
			p=q;
			continue;
		}
		*r++=0;

		if (strcmp(p, "USERNAME") == 0)
			a.sysusername=r;
		else if (strcmp(p, "UID") == 0)
		{
			u=atol(r);
			a.sysuserid= &u;
		}
		else if (strcmp(p, "GID") == 0)
		{
			a.sysgroupid=atol(r);
		}
		else if (strcmp(p, "HOME") == 0)
		{
			a.homedir=r;
		}
		else if (strcmp(p, "ADDRESS") == 0)
		{
			a.address=r;
		}
		else if (strcmp(p, "NAME") == 0)
		{
			a.fullname=r;
		}
		else if (strcmp(p, "MAILDIR") == 0)
		{
			a.maildir=r;
		}
		else if (strcmp(p, "QUOTA") == 0)
		{
			a.quota=r;
		}
		else if (strcmp(p, "PASSWD") == 0)
		{
			a.passwd=r;
		}
		else if (strcmp(p, "PASSWD2") == 0)
		{
			a.clearpasswd=r;
		}
		else if (strcmp(p, "OPTIONS") == 0)
		{
			a.options=r;
		}
		p=q;
	}
	errno=EIO;
	return (1);
}

static int request_with_meta_create(struct auth_meta *meta,
				    const char *authreq,
				    void (*cb)(const char *, size_t,
					       void *),
				    void *ptr)
{
	if (meta->envvars)
	{
		for (size_t i=0; meta->envvars[i]; ++i)
		{
			const char *p=meta->envvars[i];
			const char *q=p;

			while (*q)
			{
				if (*q < ' ')
					return -1;
				++q;
			}
			(*cb)(p, q-p, ptr);
			(*cb)("\n", 1, ptr);
		}
	}

	(*cb)(authreq, strlen(authreq)+1, ptr);
	return 0;
}

static void count_request_with_meta(const char *ignored, size_t n,
				    void *ptr)
{
	*(size_t *)ptr += n;
}

static void save_request_with_meta(const char *chunk, size_t n,
				   void *ptr)
{
	char **q=(char **)ptr;

	memcpy(*q, chunk, n);

	*q += n;
}

static char *request_with_meta(struct auth_meta *meta,
			       const char *authreq)
{
	size_t cnt=0;
	char *ptr, *q;

	if (request_with_meta_create(meta, authreq, count_request_with_meta,
				     &cnt) < 0)
	{
		errno=EINVAL;
		return 0;
	}

	ptr=malloc(cnt);

	if (!ptr)
		return 0;

	q=ptr;

	request_with_meta_create(meta, authreq, save_request_with_meta, &q);
	return ptr;
}

static int do_authdaemondo_meta(const char *authreq,
				int (*func)(struct authinfo *, void *),
				void *arg)
{
	int	s=opensock();
	int	rc;

	if (s < 0)
	{
		return (1);
	}
	rc = _authdaemondo(s, s, authreq, func, arg);
	close(s);
	return rc;
}

int authdaemondo_meta(struct auth_meta *meta,
		      const char *authreq,
		      int (*func)(struct authinfo *, void *),
		      void *arg)
{
	char *buf;
	int rc;
	struct auth_meta *default_meta=0;

	if (!meta)
	{
		default_meta=auth_meta_init_default();

		if (!default_meta)
			return 1;

		meta=default_meta;
	}

	buf=request_with_meta(meta, authreq);

	if (default_meta)
		auth_meta_destroy_default(default_meta);

	if (!buf)
		return 1;

	rc=do_authdaemondo_meta(buf, func, arg);

	free(buf);

	return rc;
}

void auth_daemon_cleanup()
{
}

struct enum_getch {
	char buffer[BUFSIZ];
	char *buf_ptr;
	size_t buf_left;
};

#define getauthc(fd,eg) ((eg)->buf_left-- ? \
			(unsigned char)*((eg)->buf_ptr)++:\
			fillgetauthc((fd),(eg)))

static int fillgetauthc(int fd, struct enum_getch *eg)
{
	time_t	end_time, curtime;

	time(&end_time);
	end_time += 60;

	for (;;)
	{
		int     n;
		fd_set  fds;
		struct  timeval tv;

		time(&curtime);
		if (curtime >= end_time)
			break;

		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		tv.tv_sec=end_time - curtime;
		tv.tv_usec=0;
		if (select(fd+1, &fds, 0, 0, &tv) <= 0 || !FD_ISSET(fd, &fds))
			break;

		n=read(fd, eg->buffer, sizeof(eg->buffer));
		if (n <= 0)
			break;

		eg->buf_ptr=eg->buffer;
		eg->buf_left=n;

		--eg->buf_left;
		return (unsigned char)*(eg->buf_ptr)++;
	}
	return EOF;
}

static int readline(int fd, struct enum_getch *eg,
		    char *buf,
		    size_t bufsize)
{
	if (bufsize == 0)
		return EOF;

	while (--bufsize)
	{
		int ch=getauthc(fd, eg);

		if (ch == EOF)
			return -1;
		if (ch == '\n')
			break;

		*buf++=ch;
	}
	*buf=0;
	return 0;
}

int _auth_enumerate(int wrfd, int rdfd,
		     void(*cb_func)(const char *name,
				    uid_t uid,
				    gid_t gid,
				    const char *homedir,
				    const char *maildir,
				    const char *options,
				    void *void_arg),
		     void *void_arg)
{
	static char cmd[]="ENUMERATE\n";
	struct enum_getch eg;
	char linebuf[BUFSIZ];

	if (writeauth(wrfd, cmd, sizeof(cmd)-1))
	{
		return 1;
	}

	eg.buf_left=0;

	while (readline(rdfd, &eg, linebuf, sizeof(linebuf)) == 0)
	{
		char *p;
		const char *name;
		uid_t uid;
		gid_t gid;
		const char *homedir;
		const char *maildir;
		const char *options;

		if (strcmp(linebuf, ".") == 0)
		{
			(*cb_func)(NULL, 0, 0, NULL, NULL, NULL, void_arg);
			return 0;
		}

		p=strchr(linebuf, '#');
		if (p) *p=0;

		p=strchr(linebuf, '\t');

		if (p)
		{
			name=linebuf;
			*p++=0;

			uid=libmail_atouid_t(p);
			p=strchr(p, '\t');
			if (uid && p)
			{
				*p++=0;
				gid=libmail_atogid_t(p);
				p=strchr(p, '\t');
				if (gid && p)
				{
					*p++=0;
					homedir=p;
					p=strchr(p, '\t');
					maildir=NULL;
					options=NULL;

					if (p)
					{
						*p++=0;
						maildir=p;
						p=strchr(p, '\t');

						if (p)
						{
							*p++=0;
							options=p;
							p=strchr(p, '\t');
							if (p) *p=0;
						}
					}


					(*cb_func)(name, uid, gid, homedir,
						   maildir, options, void_arg);
				}
			}
		}
	}
	return 1;
}

void auth_enumerate( void(*cb_func)(const char *name,
				    uid_t uid,
				    gid_t gid,
				    const char *homedir,
				    const char *maildir,
				    const char *options,
				    void *void_arg),
		     void *void_arg)
{
	int s=opensock();

	if (s < 0)
		return;

	_auth_enumerate(s, s, cb_func, void_arg);

	close(s);
}
