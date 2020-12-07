/*
** Copyright 2000-2018 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#include	"courier_auth_config.h"
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/socket.h>
#include	<sys/un.h>
#include	<sys/time.h>
#include	<sys/wait.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<errno.h>
#include	<signal.h>
#include	<fcntl.h>
#include	<ctype.h>
#include	"numlib/numlib.h"
#include	"liblock/liblock.h"
#include	"auth.h"
#include	"authdaemonrc.h"
#include	"courierauthdebug.h"
#include	"pkglibdir.h"
#include	"courierauth.h"
#include	"courierauthstaticlist.h"
#include        "libhmac/hmac.h"
#include	"authdaemond.h"
#include	<ltdl.h>


#ifndef	SOMAXCONN
#define	SOMAXCONN	5
#endif

#include	"courierauthstaticlist.h"

static unsigned ndaemons;

struct authstaticinfolist {
	struct authstaticinfolist *next;
	struct authstaticinfo *info;
	lt_dlhandle h;
};

static struct authstaticinfolist *modulelist=NULL;

char tcpremoteip[40];

static int mksocket()
{
int	fd=socket(PF_UNIX, SOCK_STREAM, 0);
struct	sockaddr_un skun;

	if (fd < 0)	return (-1);
	skun.sun_family=AF_UNIX;
	memcpy(skun.sun_path, AUTHDAEMONSOCK, sizeof(AUTHDAEMONSOCK)-1);
	memcpy(&skun.sun_path[0]+sizeof(AUTHDAEMONSOCK)-1, ".tmp", 5);
	unlink(skun.sun_path);
	if (bind(fd, (const struct sockaddr *)&skun, sizeof(skun)) ||
	    listen(fd, SOMAXCONN) ||
	    chmod(skun.sun_path, 0777) ||
	    rename(skun.sun_path, AUTHDAEMONSOCK) ||
	    fcntl(fd, F_SETFD, FD_CLOEXEC) < 0 ||
	    fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
	{
		perror(AUTHDAEMONSOCK);
		close(fd);
		return (-1);
	}
	return (fd);
}


static int initmodules(const char *p)
{
	char buf[100];
	char buf2[100];
	struct authstaticinfolist **modptr= &modulelist;
	struct authstaticinfolist *m;

	if (ndaemons <= 0)
	{
		ndaemons=1;

		fprintf(stderr, "ERR: Configuration error - missing 'daemons' setting, using %u\n",
			ndaemons);
	}

	while ((m=modulelist) != NULL)
	{
		modulelist=m->next;
		fprintf(stderr, "INFO: Uninstalling %s\n",
			m->info->auth_name);
		lt_dlclose(m->h);
		free(m);
	}

	while (p && *p)
	{
		size_t i;
		lt_dlhandle h;
		lt_ptr pt;
		struct authstaticinfo *a;

		if (isspace((int)(unsigned char)*p))
		{
			++p;
			continue;
		}

		for (i=0; p[i] && !isspace((int)(unsigned char)p[i]); ++i)
			;

		strcpy(buf, "lib");
		strncat(buf, p, i>40 ? 40:i);
		strcpy(buf2, buf);
		strcat(buf2, COURIERDLEXT);

		fprintf(stderr, "INFO: Installing %s\n", buf2);
		p += i;
		h=lt_dlopen(buf2);

		if (h == NULL)
		{
			fprintf(stderr, "INFO: %s\n", lt_dlerror());
			continue;
		}

		buf2[snprintf(buf2, sizeof(buf2)-1, "courier_%s_init",
			      buf+3)]=0;

		pt=lt_dlsym(h, buf2);
		if (pt == NULL)
		{
			fprintf(stderr,
				"ERR: Can't locate init function %s.\n",
				buf2);
			fprintf(stderr, "ERR: %s\n", lt_dlerror());
			continue;
		}

		a= (*(struct authstaticinfo *(*)(void))pt)();

		if ((m=malloc(sizeof(*modulelist))) == NULL)
		{
			perror("ERR");
			lt_dlclose(h);
			continue;
		}
		*modptr=m;
		m->next=NULL;
		m->info=a;
		m->h=h;
		modptr= &m->next;
		fprintf(stderr, "INFO: Installation complete: %s\n",
			a->auth_name);
	}
	return (0);
}

static int readconfig()
{
	char	buf[BUFSIZ];
	FILE	*fp;
	char	*modlist=0;
	unsigned daemons=0;

	if ((fp=fopen(AUTHDAEMONRC, "r")) == NULL)
	{
		perror(AUTHDAEMONRC);
		return (-1);
	}

	while (fgets(buf, sizeof(buf), fp))
	{
	char	*p=strchr(buf, '\n'), *q;

		if (!p)
		{
		int	c;

			while ((c=getc(fp)) >= 0 && c != '\n')
				;
		}
		else	*p=0;
		if ((p=strchr(buf, '#')) != 0)	*p=0;

		for (p=buf; *p; p++)
			if (!isspace((int)(unsigned char)*p))
				break;
		if (*p == 0)	continue;

		if ((p=strchr(buf, '=')) == 0)
		{
			fprintf(stderr, "ERR: Bad line in %s: %s\n",
				AUTHDAEMONRC, buf);
			fclose(fp);
			if (modlist)
				free(modlist);
			return (-1);
		}
		*p++=0;
		while (*p && isspace((int)(unsigned char)*p))
			++p;
		if (*p == '"')
		{
			++p;
			q=strchr(p, '"');
			if (q)	*q=0;
		}
		if (strcmp(buf, "authmodulelist") == 0)
		{
			if (modlist)
				free(modlist);
			modlist=strdup(p);
			if (!modlist)
			{
				perror("malloc");
				fclose(fp);
				return (-1);
			}
			continue;
		}
		if (strcmp(buf, "daemons") == 0)
		{
			daemons=atoi(p);
			continue;
		}
	}
	fclose(fp);

	fprintf(stderr, "INFO: modules=\"%s\", daemons=%u\n",
		modlist ? modlist:"(none)",
		daemons);
	ndaemons=daemons;
	return (initmodules(modlist));
}

static char buf[BUFSIZ];
static char *readptr;
static int readleft;
static char *writeptr;
static int writeleft;

static int getauthc(int fd)
{
fd_set	fds;
struct	timeval	tv;

	if (readleft--)
		return ( (int)(unsigned char)*readptr++ );

	readleft=0;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	tv.tv_sec=10;
	tv.tv_usec=0;
	if (select(fd+1, &fds, 0, 0, &tv) <= 0 ||
		!FD_ISSET(fd, &fds))
		return (EOF);
	readleft=read(fd, buf, sizeof(buf));
	readptr=buf;
	if (readleft <= 0)
	{
		readleft=0;
		return (EOF);
	}
	--readleft;
	return ( (int)(unsigned char)*readptr++ );
}

static int writeauth(int fd, const char *p, unsigned pl)
{
fd_set	fds;
struct	timeval	tv;

	while (pl)
	{
	int	n;

		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		tv.tv_sec=30;
		tv.tv_usec=0;
		if (select(fd+1, 0, &fds, 0, &tv) <= 0 ||
			!FD_ISSET(fd, &fds))
			return (-1);
		n=write(fd, p, pl);
		if (n <= 0)	return (-1);
		p += n;
		pl -= n;
	}
	return (0);
}

static int writeauthflush(int fd)
{
	if (writeptr > buf)
	{
		if (writeauth(fd, buf, writeptr - buf))
			return (-1);
	}
	writeptr=buf;
	writeleft=sizeof(buf);
	return (0);
}

static int writeauthbuf(int fd, const char *p, unsigned pl)
{
unsigned n;

	while (pl)
	{
		if (pl < writeleft)
		{
			memcpy(writeptr, p, pl);
			writeptr += pl;
			writeleft -= pl;
			return (0);
		}

		if (writeauthflush(fd))	return (-1);

		n=pl;
		if (n > writeleft)	n=writeleft;
		memcpy(writeptr, p, n);
		p += n;
		writeptr += n;
		writeleft -= n;
		pl -= n;
	}
	return (0);
}

static int writeenvval(int fd, const char *env, const char *val)
{
	if (writeauthbuf(fd, env, strlen(env)) ||
		writeauthbuf(fd, "=", 1) ||
		writeauthbuf(fd, val, strlen(val)) ||
		writeauthbuf(fd, "\n", 1))
		return (-1);
	return (0);
}

static const char *findopt(const char *options, const char *keyword)
{
	size_t keyword_l=strlen(keyword);

	while (options)
	{
		if (strncmp(options, keyword, keyword_l) == 0)
		{
			switch (options[keyword_l])
			{
			case '=':
				return options + keyword_l + 1;
			case ',': case '\0':
				return options + keyword_l;
			}
		}
		options=strchr(options, ',');
		if (options)
			++options;
	}
	return NULL;
}

/* Returns a malloc'd string containing the merge of the options string
   and any default options which apply, or NULL if no options at all */
static char *mergeoptions(const char *options)
{
	char	*defoptions = getenv("DEFAULTOPTIONS");
	char	*p;

	if (options && *options && defoptions && *defoptions)
	{
		char *result = malloc(strlen(options) +
				 strlen(defoptions) + 2);
		if (!result)
		{
			perror("malloc");
			return NULL;
		}
		strcpy(result, options);

		defoptions = strdup(defoptions);
		if (!defoptions)
		{
			perror("malloc");
			free(result);
			return NULL;
		}

		for (p = strtok(defoptions, ","); p; p = strtok(0, ","))
		{
			char *q = strchr(p, '=');
			if (q) *q = '\0';
			if (findopt(result, p)) continue;
			if (q) *q = '=';
			strcat(result, ",");
			strcat(result, p);
		}
		free(defoptions);
		return result;
	}
	else if (options && *options)
	{
		return strdup(options);
	}
	else if (defoptions && *defoptions)
	{
		return strdup(defoptions);
	}
	else
		return 0;
}

static int printauth(struct authinfo *authinfo, void *vp)
{
	int	fd= *(int *)vp;
	char	buf2[NUMBUFSIZE];
	char	*fullopt;

	writeptr=buf;
	writeleft=sizeof(buf);

	courier_authdebug_authinfo("Authenticated: ", authinfo,
				   authinfo->clearpasswd,
				   authinfo->passwd);

	if (authinfo->sysusername)
		if (writeenvval(fd, "USERNAME", authinfo->sysusername))
			return (1);
	if (authinfo->sysuserid)
		if (writeenvval(fd, "UID", libmail_str_uid_t(*authinfo->sysuserid,
			buf2)))
			return (1);
	if (writeenvval(fd, "GID", libmail_str_uid_t(authinfo->sysgroupid, buf2)))
			return (1);

	if (writeenvval(fd, "HOME", authinfo->homedir))
			return (1);
	if (authinfo->address)
		if (writeenvval(fd, "ADDRESS", authinfo->address))
			return (1);
	if (authinfo->fullname)
	{
		/*
		 * Only the first field of the comma-seperated GECOS field is the
		 * full username.
		 */
	 	char *fullname;
		char *p;
		int retval;

		fullname=strdup(authinfo->fullname);
		if(fullname == NULL)
		{
			perror("strdup");
			return (1);
		}

		p = fullname;
		while (*p != ',' && *p != '\0')
			p++;
		*p=0;
		retval = writeenvval(fd, "NAME", fullname);
		free(fullname);
		if(retval)
			return (1);
	}
	if (authinfo->maildir)
		if (writeenvval(fd, "MAILDIR", authinfo->maildir))
			return (1);
	if (authinfo->quota)
		if (writeenvval(fd, "QUOTA", authinfo->quota))
			return (1);
	if (authinfo->passwd)
		if (writeenvval(fd, "PASSWD", authinfo->passwd))
			return (1);
	if (authinfo->clearpasswd)
		if (writeenvval(fd, "PASSWD2", authinfo->clearpasswd))
			return (1);
	fullopt = mergeoptions(authinfo->options);
	if (fullopt)
	{
		int rc = writeenvval(fd, "OPTIONS", fullopt);
		free(fullopt);
		if (rc)
			return (1);
	}
	if (writeauthbuf(fd, ".\n", 2) || writeauthflush(fd))
		return (1);
	return (0);
}

static void pre(int fd, char *prebuf)
{
char	*p=strchr(prebuf, ' ');
char	*service;
struct authstaticinfolist *l;

	if (!p)	return;
	*p++=0;
	while (*p == ' ')	++p;
	service=p;
	p=strchr(p, ' ');
	if (!p) return;
        *p++=0;
        while (*p == ' ')     ++p;

	DPRINTF("received userid lookup request: %s", p);

	for (l=modulelist; l; l=l->next)
	{
		struct authstaticinfo *auth=l->info;
		const char	*modname = auth->auth_name;
		int rc;

		if (strcmp(prebuf, ".") && strcmp(prebuf, modname))
			continue;

		DPRINTF("%s: trying this module", modname);


		rc=(*auth->auth_prefunc)(p, service,
					 &printauth, &fd);

		if (rc == 0)
			return;

		if (rc > 0)
		{
			DPRINTF("%s: TEMPFAIL - no more modules will be tried",
				modname);
			return;	/* Temporary error */
		}
		DPRINTF("%s: REJECT - try next module", modname);
	}
	writeauth(fd, "FAIL\n", 5);
	DPRINTF("FAIL, all modules rejected");
}

struct enumerate_info {
	int fd;
	char *buf_ptr;
	size_t buf_left;
	char buffer[BUFSIZ];
	int enumerate_ok;
};

static void enumflush(struct enumerate_info *ei)
{
	if (ei->buf_ptr > ei->buffer)
		writeauth(ei->fd, ei->buffer, ei->buf_ptr - ei->buffer);
	ei->buf_ptr=ei->buffer;
	ei->buf_left=sizeof(ei->buffer);
}

static void enumwrite(struct enumerate_info *ei, const char *s)
{
	while (s && *s)
	{
		size_t l;

		if (ei->buf_left == 0)
			enumflush(ei);

		l=strlen(s);
		if (l > ei->buf_left)
			l=ei->buf_left;
		memcpy(ei->buf_ptr, s, l);
		ei->buf_ptr += l;
		ei->buf_left -= l;
		s += l;
	}
}

static void enum_cb(const char *name,
		    uid_t uid,
		    gid_t gid,
		    const char *homedir,
		    const char *maildir,
		    const char *options,
		    void *void_arg)
{
	struct enumerate_info *ei=(struct enumerate_info *)void_arg;
	char buf[NUMBUFSIZE];
	char *fullopt;

	if (name == NULL)
	{
		ei->enumerate_ok=1;
		return;
	}

	enumwrite(ei, name);
	enumwrite(ei, "\t");
	enumwrite(ei, libmail_str_uid_t(uid, buf));
	enumwrite(ei, "\t");
	enumwrite(ei, libmail_str_gid_t(gid, buf));
	enumwrite(ei, "\t");
	enumwrite(ei, homedir);
	enumwrite(ei, "\t");
	enumwrite(ei, maildir ? maildir : "");
	enumwrite(ei, "\t");
	fullopt = mergeoptions(options);
	if (fullopt)
	{
		enumwrite(ei, fullopt);
		free (fullopt);
	}
	enumwrite(ei, "\n");
}

static void enumerate(int fd)
{
	struct enumerate_info ei;
	struct authstaticinfolist *l;

	ei.fd=fd;
	ei.buf_ptr=ei.buffer;
	ei.buf_left=0;

	for (l=modulelist; l; l=l->next)
	{
		struct authstaticinfo *auth=l->info;

		if (auth->auth_enumerate == NULL)
			continue;

		enumwrite(&ei, "# ");
		enumwrite(&ei, auth->auth_name);
		enumwrite(&ei, "\n\n");
		ei.enumerate_ok=0;
		(*auth->auth_enumerate)(enum_cb, &ei);
		if (!ei.enumerate_ok)
		{
			enumflush(&ei);
			DPRINTF("enumeration terminated prematurely in module %s",
				auth->auth_name);
			return;
		}
	}
	enumwrite(&ei, ".\n");
	enumflush(&ei);
}

static void dopasswd(int, const char *, const char *, const char *,
		     const char *);

static void passwd(int fd, char *prebuf)
{
	char *p;
	const char *service;
	const char *userid;
	const char *opwd;
	const char *npwd;

	for (p=prebuf; *p; p++)
	{
		if (*p == '\t')
			continue;
		if ((int)(unsigned char)*p < ' ')
		{
			writeauth(fd, "FAIL\n", 5);
			return;
		}
	}

	service=prebuf;

	if ((p=strchr(service, '\t')) != 0)
	{
		*p++=0;
		userid=p;
		if ((p=strchr(p, '\t')) != 0)
		{
			*p++=0;
			opwd=p;
			if ((p=strchr(p, '\t')) != 0)
			{
				*p++=0;
				npwd=p;
				if ((p=strchr(p, '\t')) != 0)
					*p=0;
				dopasswd(fd, service, userid, opwd, npwd);
				return;
			}
		}
	}
}

static void dopasswd(int fd,
		     const char *service,
		     const char *userid,
		     const char *opwd,
		     const char *npwd)
{
struct authstaticinfolist *l;


	for (l=modulelist; l; l=l->next)
	{
		struct authstaticinfo *auth=l->info;
		int	rc;

		int (*f)(const char *, const char *, const char *,
			 const char *)=
			auth->auth_changepwd;

		if (!f)
			continue;

		rc= (*f)(service, userid, opwd, npwd);

		if (rc == 0)
		{
			writeauth(fd, "OK\n", 3);
			return;
		}
		if (rc > 0)
			break;
	}
	writeauth(fd, "FAIL\n", 5);
}

static void auth(int fd, char *p)
{
	char *service;
	char *authtype;
	char	*pp;
	struct authstaticinfolist *l;

	service=p;
	if ((p=strchr(p, '\n')) == 0)	return;
	*p++=0;
	authtype=p;
	if ((p=strchr(p, '\n')) == 0)	return;
	*p++=0;

	pp=malloc(strlen(p)+1);
	if (!pp)
	{
		perror("CRIT: malloc() failed");
		return;
	}

	DPRINTF("received auth request, service=%s, authtype=%s", service, authtype);
	for (l=modulelist; l; l=l->next)
	{
		struct authstaticinfo *auth=l->info;
		const char	*modname = auth->auth_name;
		int rc;

		DPRINTF("%s: trying this module", modname);

		rc=(*auth->auth_func)(service, authtype,
				      strcpy(pp, p),
				      &printauth, &fd);

		if (rc == 0)
		{
			free(pp);
			return;
		}

		if (rc > 0)
		{
			DPRINTF("%s: TEMPFAIL - no more modules will be tried", modname);
			free(pp);
			return;	/* Temporary error */
		}
		DPRINTF("%s: REJECT - try next module", modname);
	}
	DPRINTF("FAIL, all modules rejected");
	writeauth(fd, "FAIL\n", 5);
	free(pp);
}

static void idlefunc()
{
	struct authstaticinfolist *l;

	for (l=modulelist; l; l=l->next)
	{
		struct authstaticinfo *auth=l->info;

		if (auth->auth_idle)
			(*auth->auth_idle)();
	}
}

static void doauth(int fd)
{
char	buf[BUFSIZ];
int	i, ch;
char	*p;

	readleft=0;
	tcpremoteip[0]=0;

	while (1)
	{
		for (i=0; (ch=getauthc(fd)) != '\n'; i++)
		{
			if (ch < 0 || i >= sizeof(buf)-2)
				return;
			buf[i]=ch;
		}
		buf[i]=0;

		for (p=buf; *p; p++)
		{
			if (*p == ' ' || *p == '=')
				break;
		}

		if (!*p)
			break;

		if (*p == ' ')
		{
			*p++=0;
			while (*p == ' ')	++p;
			break;
		}

		*p++=0;

		if (strcmp(buf, "TCPREMOTEIP") == 0)
		{
			tcpremoteip[sizeof(tcpremoteip)-1]=0;
			strncpy(tcpremoteip, p, sizeof(tcpremoteip)-1);
		}
	}

	if (strcmp(buf, "PRE") == 0)
	{
		pre(fd, p);
		return;
	}

	if (strcmp(buf, "PASSWD") == 0)
	{
		passwd(fd, p);
		return;
	}

	if (strcmp(buf, "AUTH") == 0)
	{
	int j;

		i=atoi(p);
		if (i < 0 || i >= sizeof(buf))	return;
		for (j=0; j<i; j++)
		{
			ch=getauthc(fd);
			if (ch < 0)	return;
			buf[j]=ch;
		}
		buf[j]=0;
		auth(fd, buf);
	}

	if (strcmp(buf, "ENUMERATE") == 0)
	{
		enumerate(fd);
	}
}

static int sighup_pipe= -1;

static void sighup(int n)
{
	if (sighup_pipe >= 0)
	{
		close(sighup_pipe);
		sighup_pipe= -1;
	}
	signal(SIGHUP, sighup);
}

static int sigterm_received=0;

static void sigterm(int n)
{
	sigterm_received=1;
	if (sighup_pipe >= 0)
	{
		close(sighup_pipe);
		sighup_pipe= -1;
	}
	else
	{
		kill(0, SIGTERM);
		_exit(0);
	}
}

static int startchildren(int *pipefd)
{
unsigned i;
pid_t	p;

	signal(SIGCHLD, sighup);
	for (i=0; i<ndaemons; i++)
	{
		p=fork();
		while (p == -1)
		{
			perror("CRIT: fork() failed");
			sleep(5);
			p=fork();
		}
		if (p == 0)
		{
			sighup_pipe= -1;
			close(pipefd[1]);
			signal(SIGHUP, SIG_DFL);
			signal(SIGTERM, SIG_DFL);
			signal(SIGCHLD, SIG_DFL);
			return (1);
		}
	}
	return (0);
}

static int killchildren(int *pipefd)
{
int	waitstat;

	while (wait(&waitstat) >= 0 || errno != ECHILD)
		;

	if (pipe(pipefd))
	{
		perror("CRIT: pipe() failed");
		return (-1);
	}

	return (0);
}

int start()
{
	int	s;
	int	fd;
	int	pipefd[2];
	int	do_child;

	for (fd=3; fd<256; fd++)
		close(fd);

	if (pipe(pipefd))
	{
		perror("pipe");
		return (1);
	}

	if (lt_dlinit())
	{
		fprintf(stderr, "ERR: lt_dlinit() failed: %s\n",
			lt_dlerror());
		exit(1);
	}

	if (lt_dlsetsearchpath(PKGLIBDIR))
	{
		fprintf(stderr, "ERR: lt_dlsetsearchpath() failed: %s\n",
			lt_dlerror());
		exit(1);
	}

	if (readconfig())
	{
		close(pipefd[0]);
		close(pipefd[1]);
		return (1);
	}

	s=mksocket();
	if (s < 0)
	{
		perror(AUTHDAEMONSOCK);
		close(pipefd[0]);
		close(pipefd[1]);
		return (1);
	}

	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, sighup);
	signal(SIGTERM, sigterm);

	close(0);
	if (open("/dev/null", O_RDWR) != 0)
	{
		perror("open");
		exit(1);
	}
	dup2(0, 1);
	sighup_pipe= pipefd[1];

	do_child=startchildren(pipefd);

	for (;;)
	{
		struct sockaddr	saddr;
		socklen_t saddr_len;
		fd_set	fds;
		struct timeval tv;

		FD_ZERO(&fds);
		FD_SET(pipefd[0], &fds);

		if (do_child)
			FD_SET(s, &fds);

		tv.tv_sec=300;
		tv.tv_usec=0;

		if (select( (s > pipefd[0] ? s:pipefd[0])+1,
			&fds, 0, 0, &tv) < 0)
			continue;
		if (FD_ISSET(pipefd[0], &fds))
		{
			close(pipefd[0]);
			if (do_child)
				return (0);	/* Parent died */
			fprintf(stderr, "INFO: stopping authdaemond children\n");
			while (killchildren(pipefd))
				sleep(5);
			if (sigterm_received)
				return (0);
			fprintf(stderr, "INFO: restarting authdaemond children\n");
			readconfig();
			sighup_pipe=pipefd[1];
			do_child=startchildren(pipefd);
			continue;
		}

		if (!FD_ISSET(s, &fds))
		{
			idlefunc();
			continue;
		}

		saddr_len=sizeof(saddr);
		if ((fd=accept(s, &saddr, &saddr_len)) < 0)
			continue;
		if (fcntl(fd, F_SETFL, 0) < 0 ||
		    fcntl(fd, F_SETFD, FD_CLOEXEC) < 0)
		{
			perror("CRIT: fcntl() failed");
		}
		else
			doauth(fd);
		close(fd);
	}
}
