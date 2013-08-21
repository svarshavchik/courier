/*
**
** Copyright 2004-2009 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "courier_socks_config.h"
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#if HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif
#if HAVE_PTHREAD_H
#include <pthread.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

#include "configfile.h"
#include "cidr.h"
#include "toipv4.h"

#define INITFUNC connect_sox_init
#define NEXTFUNC connect_sox_next
#include "connectfunc.h"
#undef INITFUNC
#undef NEXTFUNC
#include "printaddr.h"

#define DEBUG_CONFIG 1
#define DEBUG_CONNECT 2
#define DEBUG_SELECT 4
#define DEBUG_MISC 8
#define DEBUG_ERRORS 128
static int debug_level;

#define D(n) if (debug_level & (n))

/* Parsed configuration file */

struct config {
	CIDR dest;
	char *server;
	char *uid;
	char *pwd;
	struct config *next;
};


/* States of monitored sockets */

enum sox_state {
	connecting,		/* Monitored socket is connecting */
	connectfailed,		/* Connect failed */
	connected,		/* Socket connected */
	listening,		/* Socket is listening */
	accepted,		/* Socket received connection */
	dead,			/* Listening socket accepted recv'd connection, now dead */
	dupe			/* Dupe of some other socket */
};

static const char *sox_state_str(enum sox_state s)
{
	switch (s) {
	case connecting:
		return "connecting";

	case connectfailed:
		return "connectfailed";

	case connected:
		return "connected";

	case listening:
		return "listening";

	case accepted:
		return "accepted";

	case dead:
		return "dead";

	case dupe:
		return "dupe";
	}
	return "Unknown";
}

struct sox_info {
	struct sox_info *next;	/* Next on the hash bucket */
	int fd;			/* File descriptor */
	enum sox_state state;	/* The state of this socket connection */
	int errnumber;

	SOCKADDR_STORAGE remote_addr;
	SOCKADDR_STORAGE local_addr;

	struct config *server;	/* Socks server for this socket */

	int connect_flags;
	char *redirect;

#define FLAG_IPV4 1

	/*
	** When an async socket is connecting, connect_func is the current
	** handler for the connection negotiation.
	**
	** connect_func returns 0 if the socket is connected, <0 if the
	** socket connection attempt failed, and >0 if the connection is
	** still in progress, pending I/O.
	*/

	int (*connect_func)(struct sox_info *);

	char *misc_buffer;
	/* Miscellaneous buffer used for connection negotiation */

	char *misc_ptr;
	size_t misc_left;

	/*
	** When connection is in progress on an async socket, connect_func
	** will initialize, before exiting with >0, connect_want_read and
	** connect_want_write to indicate whether connection negotiation
	** wants to read, or write
	*/
	char connect_want_read;
	char connect_want_write;
	struct connect_info *connect_info;
};

static struct sox_info *soxtab[16]; /* Hashtable */
#define HASHSIZE (sizeof(soxtab)/sizeof(soxtab[0]))

struct func_info {
	const char *funcname;
	int (**funcptr)();
};

#include "stubfuncs.h"

static struct func_info functab[]={
	{"connect",	(int (**)())(void *)&ptr_connect},
	{"bind",	(int (**)())(void *)&ptr_bind},
	{"select",	(int (**)())(void *)&ptr_select},
	{"getsockopt",	(int (**)())(void *)&ptr_getsockopt},
	{"close",	(int (**)())(void *)&ptr_close},
	{"fclose",	(int (**)())(void *)&ptr_fclose},
	{"listen",	(int (**)())(void *)&ptr_listen},
	{"accept",	(int (**)())(void *)&ptr_accept},
	{"getsockname",	(int (**)())(void *)&ptr_getsockname},
	{"getpeername",	(int (**)())(void *)&ptr_getpeername},
#if HAVE_POLL
	{"poll",	(int (**)())(void *)&ptr_poll},
#endif
	{"dup",		(int (**)())(void *)&ptr_dup},
	{"dup2",	(int (**)())(void *)&ptr_dup2}
};

static int initflag=0;

struct config *configfile=NULL;

/*
** Swallow a single configuration file.
*/

static int initproxy(char *str, struct config *c)
{
	const char *uid, *pwd;
	char *p;

	p=strtok(str, " \t\r");

	if (!p)
	{
		free(c);
		return (-1);
	}

	if ((c->server=strdup(p)) == NULL)
	{
		perror("strdup");
		exit(1);
	}

	if ((uid=strtok(NULL, " \t\r")) && (pwd=strtok(NULL, " \t\r")))
	{
		if ((c->uid=strdup(uid)) == NULL ||
		    (c->pwd=strdup(pwd)) == NULL)
		{
			perror("strdup");
			exit(1);
		}
	}
	return (0);
}

static void parse_config_file(FILE *f, const char *filename,
			      int *global_opts);
static int sys_fclose(FILE *fp);

/* First-time initialization */

#define PARSE(fp, fn) parse_config_file(fp, fn, (int *)voidarg)
#define DEBUG_PREFIX "socks"
#include "parseconfigfile.h"
#undef PARSE
#undef DEBUG_PREFIX

static void parse_config_file(FILE *f, const char *filename,
			      int *global_opts)
{
	char linebuf[1024];
	struct config *c, *t;
	int linenum=0;

	while (fgets(linebuf, sizeof(linebuf), f))
	{
		char *p=strchr(linebuf, '\n');
		char *action;

		++linenum;

		if (p) *p=0;

		p=strchr(linebuf, '#');
		if (p) *p=0;

		action=strtok(linebuf, " \t\r");
		if (!action)
			continue;
		if (!knownconfigword(action))
		{
			fprintf(stderr, "%s(%d): \"%s\" not recognized\n",
				filename, linenum, action);
		}

		if (strcmp(action, "allowenv") == 0)
		{
			*global_opts=1;
			continue;
		}
		if (strcmp(action, "proxy") && strcmp(action, "noproxy"))
		{
			continue;
		}

		p=strtok(NULL, " \t\r");

		if (!p)
		{
			fprintf(stderr, "%s(%d): Missing address\n",
				filename, linenum);
			continue;
		}

		c=(struct config *)malloc(sizeof(struct config));

		if (!c)
		{
			perror("malloc");
			exit(1);
		}
		memset(c, 0, sizeof(*c));

		if (tocidr(&c->dest, p))
		{
			free(c);
			fprintf(stderr,
				"%s(%d): Cannot parse %s as a valid "
				"address mask\n",
				filename, linenum, p);
			continue;
		}

		for (t=configfile; t; t=t->next)
		{
			if (memcmp(&t->dest.addr, &c->dest.addr,
				   sizeof(c->dest.addr)) == 0 &&
			    t->dest.pfix == c->dest.pfix)
			{
				break;
			}
		}

		if (t)
		{
			free(c);
			fprintf(stderr,
				"%s(%d): Duplicate %s "
				"address mask\n",
				filename, linenum, p);
			continue;
		}

		if (strcmp(action, "proxy") == 0)
		{
			if (initproxy(NULL, c))
			{
				fprintf(stderr,
					"%s(%d): Invalid proxy statement.\n",
					filename, linenum);
				continue;
			}
		}
		c->next=configfile;
		configfile=c;
	}
}

static void init()
{
	size_t j;
	const char *p=getenv("SOCKS_DEBUG");
	int global_options=0;

	initflag=1;

	if (p)
		debug_level=atoi(p);

	/*
	** Try to locate the real system functions in the standard C libraries.
	*/

	for (j=0; j<sizeof(functab)/sizeof(functab[0]); j++)
	{
		union {
			void *s;
			int (*f)();
		} u;

		if ((u.s=dlsym(RTLD_NEXT, functab[j].funcname)) == NULL)
		{
			fprintf(stderr,
				"libsocks: unable to find the %s() function: %s\n",
				functab[j].funcname,
				dlerror());
			exit(1);
		}

		*functab[j].funcptr=u.f;
	}

	parseconfigfile(&global_options);

	if (global_options && (p=getenv("SOCKS_PROXY")) != NULL)
	{
		/*
		** Global proxy override.
		**
		** Remove all proxy settings read from the config file,
		** and replace all of them with a single 0.0.0.0/0
		** setting.
		*/

		struct config **ptr;
		struct config *s;
		struct config *newproxy=(struct config *)
			malloc(sizeof(struct config));
		char *buffer;

		if (!newproxy)
		{
			perror("malloc");
			exit(1);
		}
		memset(newproxy, 0, sizeof(*newproxy));

		if ((buffer=strdup(p)) == NULL)
		{
			perror("malloc");
			exit(1);
		}

		if (initproxy(buffer, newproxy))
		{
			fprintf(stderr,
				"SOCKS_PROXY: Invalid proxy setting.\n");
			free(newproxy);
		}
		else
		{
			for (ptr= &configfile; *ptr; )
			{
				if ( (*ptr)->server == NULL)
				{
					/* Keep noproxy settings */

					ptr= &(*ptr)->next;
					continue;
				}

				s=*ptr;

				*ptr=s->next;

				free(s->server);
				if (s->uid)
					free(s->uid);
				if (s->pwd)
					free(s->pwd);
				free(s);
			}

			newproxy->next=configfile;
			configfile=newproxy;
		}
		free(buffer);
	}
}

/*
** Stubs that invoke the real system functions we've intercepted.
*/

static int sys_connect(int fd, const struct sockaddr *addr, SOCKLEN_T addrlen)
{
	if (!initflag) init();
	return (*ptr_connect)(fd, addr, addrlen);
}

static int sys_bind(int fd, const struct sockaddr *addr, SOCKLEN_T addrlen)
{
	if (!initflag) init();
	return (*ptr_bind)(fd, addr, addrlen);
}

static int sys_select(int n, fd_set *r, fd_set *w, fd_set *e,
		      struct timeval *tv)
{
	if (!initflag) init();
	return (*ptr_select)(n, r, w, e, tv);
}

static int sys_getsockopt(int  s, int level, int optname, void *optval,
			  SOCKLEN_T *optlen)
{
	if (!initflag) init();
	return (*ptr_getsockopt)(s, level, optname, optval, optlen);
}

static int sys_close(int fd)
{
	if (!initflag) init();
	return (*ptr_close)(fd);
}

static int sys_fclose(FILE *fp)
{
	if (!initflag) init();
	return (*ptr_fclose)(fp);
}

static int sys_listen(int s, int backlog)
{
	if (!initflag) init();
	return (*ptr_listen)(s, backlog);
}

static int sys_accept(int s, struct sockaddr *addr, SOCKLEN_T *addrlen)
{
	if (!initflag) init();
	return (*ptr_accept)(s, addr, addrlen);
}

static int sys_getsockname(int s, struct sockaddr *name, SOCKLEN_T *namelen)
{
	if (!initflag) init();
	return (*ptr_getsockname)(s, name, namelen);
}

static int sys_getpeername(int s, struct sockaddr *name, SOCKLEN_T *namelen)
{
	if (!initflag) init();
	return (*ptr_getsockname)(s, name, namelen);
}

static int sys_dup(int fd)
{
	if (!initflag) init();
	return (*ptr_dup)(fd);
}

static int sys_dup2(int fd, int fdto)
{
	if (!initflag) init();
	return (*ptr_dup2)(fd, fdto);
}

#if HAVE_POLL
static int sys_poll(struct pollfd *ufds, unsigned int nfds, int timeout)
{
	if (!initflag) init();
	return (*ptr_poll)(ufds, nfds, timeout);
}
#endif

/*
** Deallocate memory used by the socket monitoring structure.
*/

static void reset_ptr(struct sox_info *p)
{

	/* Flush connection information */

	while (p->connect_info)
	{
		const struct sockaddr *addr;
		SOCKLEN_T addrlen;

		if (connect_sox_next(p->connect_info, &addr, &addrlen))
		{
			free(p->connect_info);
			p->connect_info=NULL;
		}
	}
	p->server=NULL;
	if (p->redirect)
		free(p->redirect);
	p->redirect=NULL;
	if (p->misc_buffer)
		free(p->misc_buffer);
	p->misc_buffer=NULL;
}

/* Destroy a socket information structure */

static int free_ptr(struct sox_info **ptr)
{
	struct sox_info *p;

	if (!(p=*ptr))
		return 0;

	*ptr=p->next;

	reset_ptr(p);
	free(p);
	return 1;
}

/*
** Locate a structure that monitors socket #fd.
**
** Returns a pointer to a pointer.  If *returnvalue is NULL, the socket
** structure was not found.
*/

static struct sox_info **find_ptr(int fd)
{
	struct sox_info **ptr;

	ptr=&soxtab[fd % HASHSIZE];

	while (*ptr)
	{
		if ( (*ptr)->fd == fd)
			break;
		ptr= &(*ptr)->next;
	}
	return ptr;
}

/*
** Monitor file descriptor #fd.  Returns a pointer to a pointer to the
** monitoring structure.
*/

static struct sox_info **reset_fd(int fd)
{
	struct sox_info **ptr;

	/*
	** Check if we already have something on the file handle.
	** In either case, this is stale stuff that can be thrown away.
	*/

	ptr=find_ptr(fd);

	/*
	** Initialize the monitoring structure for this filedesc.
	*/

	if (*ptr)
	{
		reset_ptr( *ptr );
	}
	else
	{
		if ( ((*ptr)=(struct sox_info *)malloc(sizeof(**ptr))) == NULL)
		{
			perror("malloc");
			return NULL;
		}

		(*ptr)->next=NULL;
		(*ptr)->fd=fd;
	}

	(*ptr)->misc_buffer=NULL;
	(*ptr)->redirect=NULL;
	(*ptr)->connect_info=NULL;
	return ptr;
}

/*
** Search the parsed configuration file for the proxy server for a given
** address.
*/

static struct config *find_server(int fd, const struct sockaddr *addr,
				  enum sox_state forState)
{
	struct config *c, *p;

	D(DEBUG_CONFIG)
		{
			fprintf(stderr,
				"socks(%d): Intercepted %s() for ", fd,
				forState == listening ? "bind":"connect");
			printaddr(stderr, (const SOCKADDR_STORAGE *)addr);
			fprintf(stderr, " - ");
		}

	/* Find the matching record with the longest prefix. */

	p=NULL;
	for (c=configfile; c; c=c->next)
	{
		if (incidr(&c->dest, (const SOCKADDR_STORAGE *)addr) == 0)
		{
			if (p == NULL || p->dest.pfix < c->dest.pfix)
				p=c;
		}
	}

	if (!p || !p->server)
	{
		D(DEBUG_CONFIG) fprintf(stderr, "no proxy.\n");
		return NULL;
	}

	D(DEBUG_CONFIG)
		{
			fprintf(stderr, "proxy via %s", p->server);
			if (p->uid)
				fprintf(stderr, ", userid=%s", p->uid);
			if (p->pwd)
				fprintf(stderr, ", password=%s", p->pwd);
			fprintf(stderr, "\n");
		}

	return p;
}

/*
** For blocking sockets, we temporary turn on O_NONBLOCK then repeatedly call
** the connecting function until the socket connection goes through.
*/

static int manual_connect(struct sox_info **p)
{
	int rc;

	while ((rc= (*(*p)->connect_func)(*p)) > 0)
	{
		fd_set r, w;

		FD_ZERO(&r);
		FD_ZERO(&w);

		if ((*p)->connect_want_read)
			FD_SET((*p)->fd, &r);
		if ((*p)->connect_want_write)
			FD_SET((*p)->fd, &w);

		if (sys_select((*p)->fd+1, &r, &w, NULL, NULL) < 0)
		{
			if (errno != EINTR)
			{
				free_ptr(p);
				return -1;
			}
		}
	}
	return rc;
}

static int connect_or_bind(int fd,
			   const struct sockaddr *addr, SOCKLEN_T addrlen,
			   enum sox_state state,
			   int (*sys_func)(int, const struct sockaddr *,
					   SOCKLEN_T));

#if HAVE_PTHREADS

static pthread_mutex_t libmutex =
#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP

		PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
#else
		PTHREAD_MUTEX_INITIALIZER
#endif
					;

static void lock_failed()
{
	perror("socks: Internal error - mutex lock failed");
	exit(1);
}

static void unlock_failed()
{
	perror("socks: Internal error - mutex unlock failed");
	exit(1);
}

#define liblock() \
	do {if (pthread_mutex_lock(&libmutex) < 0) lock_failed(); } while(0)

#define libunlock() \
	do {if (pthread_mutex_unlock(&libmutex) < 0) unlock_failed();} while(0)

#else

#define liblock() do { ; } while (0)
#define libunlock() do { ; } while (0)
#endif

int Rconnect(int fd, const struct sockaddr *addr, SOCKLEN_T addrlen)
{
	int rc;

	liblock();

	rc=connect_or_bind(fd, addr, addrlen,
			   connecting,
			   sys_connect);

	libunlock();
	return rc;
}

int Rbind(int fd, const struct sockaddr *addr, SOCKLEN_T addrlen)
{
	struct sox_info **p;
	int rc;

	liblock();

	p=find_ptr(fd);

	if (*p && (*p)->state == listening)
	{
		/* Already bound, perhaps by resvport */
		libunlock();

		return 0;
	}

	rc=connect_or_bind(fd, addr, addrlen,
			   listening,
			   sys_bind);
	libunlock();
	return rc;
}

static int try_next_server(struct sox_info *);

/*
** Connect to a proxy server for an outgoing or an incoming socket connection.
*/

static int connect_or_bind(int fd,
			   const struct sockaddr *addr, SOCKLEN_T addrlen,
			   enum sox_state state,
			   int (*sys_func)(int, const struct sockaddr *,
					   SOCKLEN_T))
{
	struct sox_info **p;
	int sock_type;
	SOCKLEN_T sock_size;
	struct config *c;

	/*
	** Sanity check on the address to connect/bind to.
	*/

	sock_size=sizeof(sock_type);
	if (addrlen > sizeof( (*p)->remote_addr) ||
	    (
	     ((const struct sockaddr_in *)addr)->sin_family != AF_INET
#if HAVE_IPV6
	     && ((const struct sockaddr_in6 *)addr)->sin6_family != AF_INET6
#endif
	     ) ||
	    sys_getsockopt(fd, SOL_SOCKET, SO_TYPE, &sock_type, &sock_size))
	{
		/*
		** fd is not a socket
		*/

		return (*sys_func)(fd, addr, addrlen);
	}

	if (sock_type != SOCK_STREAM)
	{
		/*
		** SOCK_DGRAM not implemented
		*/
		return (*sys_func)(fd, addr, addrlen);
	}

	if ((c=find_server(fd, addr, state)) == NULL)
	{
		return (*sys_func)(fd, addr, addrlen);
	}

	sock_type=fcntl(fd, F_GETFL);

	if (sock_type == -1)
	{
		/* Something's wrong */

		D(DEBUG_ERRORS)
			{
				fprintf(stderr,
					"socks(%d): fcntl(F_GETFL) failed, ",
					fd);
				perror("errno");
			}
		return (*sys_func)(fd, addr, addrlen);
	}

	if (state == connecting)
	{
		p=find_ptr(fd);
		if (*p)
			switch ( (*p)->state ) {
			case connected:
				errno=EISCONN;
				return -1;
			case connecting:
				errno=EINPROGRESS;
				return -1;
			default:
				break;
			}
	}

	if ((p=reset_fd(fd)) == NULL)
		return -1;

	memcpy( &(*p)->remote_addr, addr, addrlen);

	if ( ((*p)->misc_buffer=strdup(c->server)) == NULL)
	{
		free_ptr(p);
		return -1;
	}
	(*p)->misc_ptr= (*p)->misc_buffer;
	(*p)->connect_func= &try_next_server;
	(*p)->state=state;
	(*p)->server=c;

	/*
	** If the socket is a nonblocking socket, temporarily make it blocking
	** until we connect to the sox server.
	**
	** Or: if we're binding, we need to connect to the sox server
	** even in non-blocking mode.
	*/

	if ( (sock_type & O_NONBLOCK) == 0 || state == listening)
	{
		int rc;

		D(DEBUG_CONNECT)
			fprintf(stderr,
				"socks(%d): Connecting a blocking socket\n",
				fd);

		if (fcntl(fd, F_SETFL, sock_type | O_NONBLOCK) < 0)
		{
			free_ptr(p);

			D(DEBUG_ERRORS)
				{
					fprintf(stderr,
						"socks(%d): fcntl(F_SETFL) failed, ",
						fd);
					perror("errno");
				}
			return -1;
		}

		rc=manual_connect(p);

		fcntl(fd, F_SETFL, sock_type);
		return (rc);
	}

	D(DEBUG_CONNECT)
		fprintf(stderr,
			"socks(%d): Connecting a non-blocking socket\n",
			fd);

	errno=EINPROGRESS;
	return -1;
}

/*
** Connection aborted, save error code so that the getsockopt interceptor
** can fetch it out.
*/

static void failed(struct sox_info *si, int errnum)
{
	if (si->misc_buffer)
	{
		free(si->misc_buffer);
		si->misc_buffer=NULL;
	}

	si->state=connectfailed;
	errno=si->errnumber=errnum;

	D(DEBUG_ERRORS|DEBUG_CONNECT)
		{
			fprintf(stderr,
				"socks(%d): socket operation failed, ",
				si->fd);
			perror("errno");
		}
}

/*
** Try to connect to the next SOCKS server
*/

static int try_next_server_ip(struct sox_info *si);

static int try_next_server(struct sox_info *si)
{
	char *r;
	char *port;

	do
	{
		while (si->misc_ptr &&
		       (*si->misc_ptr == ',' ||
			isspace((int)(unsigned char)*si->misc_ptr)))
			++si->misc_ptr;


		if (si->misc_ptr == NULL || *si->misc_ptr == 0)
		{
			/* No more servers available */

			failed(si, ECONNREFUSED);
			return -1;
		}

		/* Look for the separator between the next socks server. */

		r=si->misc_ptr;

		while (*si->misc_ptr)
		{
			if (*si->misc_ptr == ',' ||
			    isspace((int)(unsigned char)*si->misc_ptr))
			{
				*si->misc_ptr++=0;
				break;
			}
			++si->misc_ptr;
		}

		si->connect_flags=0;
		if (si->redirect)
			free(si->redirect);
		si->redirect=NULL;

		while ((port=strrchr(r, '/')) != NULL)
		{
			*port++=0;

			if (strcmp(port, "ipv4") == 0)
				si->connect_flags |= FLAG_IPV4;
			if (strncmp(port, "redirect=", 9) == 0 &&
			    si->redirect == NULL)
			{
				if ((si->redirect=strdup(port+9)) == NULL)
				{
					perror("malloc");
					exit(1);
				}
			}
		}

		port=strchr(r, ':');
		if (port)
			*port++=0;
		else port="1080";

		/* Clean up the previous connection attempt. */

		if (si->connect_info)
		{
			const struct sockaddr *dummy;
			SOCKLEN_T dummylen;

			while (connect_sox_next(si->connect_info, &dummy, &dummylen)
			       == 0)
				;
			free(si->connect_info);
		}

		if ((si->connect_info=malloc(sizeof(*si->connect_info))) == NULL)
		{
			failed(si, errno);
			return -1;
		}

		/*
		** If the resolution fails, go back to step 1
		*/

		D(DEBUG_CONNECT)
			fprintf(stderr, "socks(%d): looking up proxy server"
				" \"%s\"\n", si->fd, r);

	} while (connect_sox_init(si->connect_info,
				  ((const struct sockaddr_in *)
				   &si->remote_addr)
				  ->sin_family, SOCK_STREAM, r, port) < 0);

	return try_next_server_ip(si);
}

static int server_connected(struct sox_info *si);
static int check_connection(struct sox_info *si);


static int getPortNum(SOCKADDR_STORAGE *sa)
{
	int portnum;

	switch (sa->SS_family)
	{
#ifdef HAVE_IPV6
	case AF_INET6:
		{
			struct sockaddr_in6 sin6;

			memcpy(&sin6, sa, sizeof(sin6));

			portnum=sin6.sin6_port;
		}
		break;
#endif

	case AF_INET:
		{
			struct sockaddr_in sin;

			memcpy(&sin, sa, sizeof(sin));

			portnum=sin.sin_port;
		}
		break;

	default:
		portnum=0;
	}

	return portnum;
}

static void setPortNum(int fd, SOCKADDR_STORAGE *addr_type, int portnum)
{
	if (portnum <= 0)
		return;

	switch (((const struct sockaddr_in *)addr_type)->sin_family)
	{
#if HAVE_IPV6
	case AF_INET6:
		{
			struct sockaddr_in6 sin6;

			memset(&sin6, 0, sizeof(sin6));
			sin6.sin6_family=AF_INET6;
			sin6.sin6_port=portnum;

			sys_bind(fd, (const struct sockaddr *)&sin6,
				 sizeof(sin6));
		}
		break;
#endif
	case AF_INET:
		{
			struct sockaddr_in sin;

			memset(&sin, 0, sizeof(sin));
			sin.sin_family=AF_INET;
			sin.sin_port=portnum;
			sys_bind(fd, (const struct sockaddr *)&sin,
				 sizeof(sin));
		}
		break;
	}
}


/*
** Connection attempt to the next IP address for the current socks server.
*/

static int try_next_server_ip(struct sox_info *si)
{
	const struct sockaddr *addr;
	SOCKLEN_T addrlen;
	SOCKLEN_T addrlen_cpy;
	SOCKADDR_STORAGE addr_cpy;
	int fd2;
	int flags;

	if (connect_sox_next(si->connect_info, &addr, &addrlen))
	{
		/* No more IP addresses, go to the next socks server */

		free(si->connect_info);
		si->connect_info=NULL;
		return try_next_server(si);
	}

	D(DEBUG_CONNECT)
		{
			fprintf(stderr, "socks(%d): Connecting to ", si->fd);
			printaddr(stderr, (const SOCKADDR_STORAGE *)addr);
			fprintf(stderr, "\n");
		}

	/*
	** To clear any leftover error conditions on the original
	** socket, create a new socket and dup it over the old one.
	*/

	flags=fcntl(si->fd, F_GETFL);

	if (flags < 0 ||
	    (fd2=socket(((const struct sockaddr_in *)addr)->sin_family,
			SOCK_STREAM, 0)) < 0)
	{
		failed(si, errno);
		return -1;
	}

	if (si->state == listening)
	{
		int dummy=1;

		setsockopt(fd2, SOL_SOCKET, SO_REUSEADDR,
			   (const char *)&dummy, sizeof(dummy));

		setPortNum(fd2, (SOCKADDR_STORAGE *)addr,
			   getPortNum(&si->remote_addr));
	}

	addrlen_cpy=sizeof(addr_cpy);
	if (sys_getsockname(si->fd, (struct sockaddr *)&addr_cpy,
			    &addrlen_cpy) < 0)
		addrlen=0;

	if (fcntl(fd2, F_SETFL, flags) < 0 ||
	    sys_dup2(fd2, si->fd) < 0)
	{
		sys_close(fd2);
		failed(si, errno);
		return -1;
	}

	if (addrlen)
		setPortNum(si->fd, (SOCKADDR_STORAGE *)addr,
			   getPortNum(&addr_cpy));

	sys_close(fd2);

	if (sys_connect(si->fd, addr, addrlen) == 0)
	{
		/*
		** The socket is non-blocking, but if connect says we're all
		** set, who are we to question?
		*/

		return server_connected(si);
	}

	if (errno != EINPROGRESS)
	{
		failed(si, errno);
		return -1;
	}

	/* Wait for the connection to go through */

	si->connect_want_write=1;
	si->connect_want_read=0;
	si->connect_func=check_connection;
	return 1;
}

/*
** Waiting for the connection to the socks server to go through.
*/

static int check_connection(struct sox_info *si)
{
	fd_set w;
	int rc;
	int errcode;
	SOCKLEN_T errcodesize;

	/*
	** Is the socket now writable?
	*/

	FD_ZERO(&w);
	FD_SET(si->fd, &w);

	while ((rc=sys_select(si->fd+1, NULL, &w, NULL, NULL)) < 0)
	{
		if (rc != EINTR)
		{
			failed(si, errno);
			return -1;
		}
	}

	errcodesize=sizeof(errcode);
	if (!FD_ISSET(si->fd, &w))
	{
		/* Not yet writable, wait some more. */

		si->connect_want_write=1;
		si->connect_want_read=0;
		si->connect_func=check_connection;
		return 1;
	}

	if (sys_getsockopt(si->fd, SOL_SOCKET, SO_ERROR, &errcode,
			   &errcodesize) < 0)
	{
		failed(si, errno);
		return -1;
	}

	if (errcode == 0)
	{
		return server_connected(si);
	}

	D(DEBUG_CONNECT)
		{
			fprintf(stderr,
				"socks(%d): Connection attempt failed, ",
				si->fd);
		}

	/* Connection failed, try the next IP address */

	return try_next_server_ip(si);
}

static int write_init_packet(struct sox_info *si);

/*
** Connected to a socks server.
*/

static int server_connected(struct sox_info *si)
{
	D(DEBUG_CONNECT)
		{
			fprintf(stderr,
				"socks(%d): Proxy connection established.\n",
				si->fd);
		}

	/* Clean up connection stuff */

	if (si->connect_info)
	{
		const struct sockaddr *dummy;
		SOCKLEN_T dummylen;

		while (connect_sox_next(si->connect_info, &dummy, &dummylen)
		       == 0)
			;
		free(si->connect_info);
		si->connect_info=NULL;
	}

	if (si->misc_buffer)
	{
		free(si->misc_buffer);
		si->misc_buffer=NULL;
	}

	/*
	** Prepare the initial packet.
	*/

	if ((si->misc_buffer=malloc(1024)) == NULL)
	{
		failed(si, errno);
		return -1;
	}

	si->misc_buffer[0]=5;
	si->misc_buffer[1]=1;
	si->misc_buffer[2]=0;

	si->misc_left=3;

	if (si->server->uid && *si->server->uid &&
	    si->server->pwd && *si->server->pwd)
	{
		/* Can do uid/pwd */

		si->misc_buffer[1]=2;
		si->misc_buffer[3]=2;
		si->misc_left=4;
	}

	si->misc_ptr=si->misc_buffer;

	return write_init_packet(si);
}


static int async_read(int fd, char *buf, size_t cnt)
{
	int n;

#if HAVE_POLL
	struct pollfd pfd;

	pfd.fd=fd;
	pfd.events=POLLIN;
	sys_poll(&pfd, 1, 0);
	if (!(pfd.revents & POLLIN))
		return 0;
#else
	fd_set r;
	struct timeval t;

	FD_ZERO(&r);
	FD_SET(fd, &r);
	t.tv_sec=0;
	t.tv_usec=0;

	if (sys_select(fd+1, &r, NULL, NULL, &t) < 0 || !FD_ISSET(fd, &r))
		return 0;
#endif
	n=read(fd, buf, cnt);
	if (n == 0)
	{
#ifdef ECONNRESET
		errno=ECONNRESET;
#else
		errno=EPIPE;
#endif
		n= -1;
	}
	return n;
}

static int async_write(int fd, const char *buf, size_t cnt)
{
	int n;

#if HAVE_POLL
	struct pollfd pfd;

	pfd.fd=fd;
	pfd.events=POLLOUT;
	sys_poll(&pfd, 1, 0);
	if (!(pfd.revents & POLLOUT))
		return 0;
#else
	fd_set w;
	struct timeval t;

	FD_ZERO(&w);
	FD_SET(fd, &w);
	t.tv_sec=0;
	t.tv_usec=0;

	if (sys_select(fd+1, NULL, &w, NULL, &t) < 0 || !FD_ISSET(fd, &w))
		return 0;
#endif
	n=write(fd, buf, cnt);
	if (n == 0)
	{
#ifdef ECONNRESET
		errno=ECONNRESET;
#else
		errno=EPIPE;
#endif
		n= -1;
	}
	return n;
}

static int read_init_packet(struct sox_info *si);

/*
** Send the initial packet.
*/

static int write_init_packet(struct sox_info *si)
{
	int n=async_write(si->fd, si->misc_ptr, si->misc_left);

	if (n <= 0)
	{
		failed(si, errno);
		return -1;
	}

	D(DEBUG_CONNECT)
		{
			fprintf(stderr,
				"socks(%d): Initial greeting: wrote %d bytes.\n",
				si->fd, n);
		}

	si->connect_func=write_init_packet;
	si->misc_ptr += n;
	si->misc_left -= n;

	if (si->misc_left)
	{
		si->connect_want_read=0;
		si->connect_want_write=1;
		return 1;
	}

	/* Prepare to read the server's response. */

	si->connect_want_read=1;
	si->connect_want_write=0;
	si->connect_func=read_init_packet;
	si->misc_ptr=si->misc_buffer;
	si->misc_left=2;
	return 1;
}

static int prep_connect_packet(struct sox_info *si);
static int prep_uidpwd_packet(struct sox_info *si);
static int send_connect_packet(struct sox_info *si);

static int read_init_packet(struct sox_info *si)
{
	int n=async_read(si->fd, si->misc_ptr, si->misc_left);

	if (n <= 0)
	{
		failed(si, errno);
		return -1;
	}

	D(DEBUG_CONNECT)
		{
			fprintf(stderr,
				"socks(%d): Initial greeting: read %d bytes.\n",
				si->fd, n);
		}

	si->connect_func=read_init_packet;
	si->misc_ptr += n;
	si->misc_left -= n;

	if (si->misc_left)
	{
		/* The full packet is yet to come in. */

		si->connect_want_read=1;
		si->connect_want_write=0;
		return 1;
	}

	if (si->misc_buffer[0] == 5)
		switch (si->misc_buffer[1]) {
		case 0:
			return prep_connect_packet(si);
		case 2:
			return prep_uidpwd_packet(si);
		}
	failed(si, ECONNREFUSED);
	return -1;
}

static int send_uidpwd_packet(struct sox_info *si);

static int prep_uidpwd_packet(struct sox_info *si)
{
	/* Send the userid/password logon packet */

	const char *u=si->server->uid ? si->server->uid:"";
	const char *p=si->server->pwd ? si->server->pwd:"";
	size_t ul=strlen(u);
	size_t pl=strlen(p);

	if (ul > 255) ul=255;
	if (pl > 255) pl=255;

	si->misc_buffer[0]=1;
	si->misc_buffer[1]=(char)ul;

	memcpy(&si->misc_buffer[2], u, ul);
	si->misc_buffer[2+ul]=(char)pl;
	memcpy(&si->misc_buffer[3+ul], p, pl);
	si->misc_ptr=si->misc_buffer;
	si->misc_left=3+ul+pl;
	si->connect_want_read=0;
	si->connect_want_write=1;
	si->connect_func=send_uidpwd_packet;
	return 1;
}

static int read_uidpwd_packet(struct sox_info *si);

static int send_uidpwd_packet(struct sox_info *si)
{
	int n=async_write(si->fd, si->misc_ptr, si->misc_left);

	if (n <= 0)
	{
		failed(si, errno);
		return -1;
	}

	D(DEBUG_CONNECT)
		{
			fprintf(stderr,
				"socks(%d): Sending userid and password: sent %d bytes.\n",
				si->fd, n);
		}


	si->connect_func=send_uidpwd_packet;
	si->misc_ptr += n;
	si->misc_left -= n;

	if (si->misc_left)
	{
		si->connect_want_read=0;
		si->connect_want_write=1;
		return 1;
	}

	si->connect_want_read=1;
	si->connect_want_write=0;
	si->connect_func=read_uidpwd_packet;
	si->misc_ptr=si->misc_buffer;
	si->misc_left=2;
	return 1;
}

static int read_uidpwd_packet(struct sox_info *si)
{
	int n=async_read(si->fd, si->misc_ptr, si->misc_left);

	/* read uid/pwd login result */

	if (n <= 0)
	{
		failed(si, errno);
		return -1;
	}

	D(DEBUG_CONNECT)
		{
			fprintf(stderr,
				"socks(%d): Userid and password response: read %d bytes.\n",
				si->fd, n);
		}

	si->misc_ptr += n;
	si->misc_left -= n;

	if (si->misc_left)
	{
		si->connect_want_read=1;
		si->connect_want_write=0;
		return 1;
	}

	if (si->misc_buffer[0] == 1 && si->misc_buffer[1] == 0)
		return prep_connect_packet(si);

	D(DEBUG_CONNECT)
		{
			fprintf(stderr,
				"socks(%d): Userid and password rejected.\n",
				si->fd);
		}

	failed(si, EPERM);
	return -1;
}


static int prep_connect_packet(struct sox_info *si)
{
	SOCKADDR_STORAGE tempbuf;
	SOCKADDR_STORAGE *addrptr;
	const char *redirect;
	int portn;
	char portnbuf[2];

	/* Send the connection request */

	si->misc_buffer[0]=5;
	si->misc_buffer[1]=1; /* CONNECT */

	if (si->state == listening)
		si->misc_buffer[1]=2; /* BIND */

	D(DEBUG_CONNECT)
		{
			fprintf(stderr,
				"socks(%d): Sending proxy request 0x%02x.\n",
				si->fd, (int)si->misc_buffer[1]);
		}

	si->misc_buffer[2]=0;
	si->misc_ptr=si->misc_buffer;

	addrptr=&si->remote_addr;

#if HAVE_IPV6
	if (si->connect_flags & FLAG_IPV4)
	{
		struct sockaddr_in tempbuf_sin;

		if (toipv4((struct sockaddr_in *)&tempbuf_sin, addrptr) == 0)
		{
			memcpy(&tempbuf, &tempbuf_sin, sizeof(tempbuf_sin));
			addrptr=&tempbuf;
		}
	}
#endif

	redirect=si->redirect;
	portn=0;

	if (redirect)
	{
		D(DEBUG_CONNECT)
			fprintf(stderr,
				"socks(%d): Redirecting to %s\n",
				si->fd,	redirect);
	}

	if (redirect && *redirect == ':')  /* Only a port redirect */
	{
		portn=atoi(redirect+1);
		redirect=NULL;
	}
	portnbuf[0]=(char)(portn/256);
	portnbuf[1]=(char)portn;

	if (redirect)
	{
		const char *hostname=redirect;
		const char *port=strrchr(hostname, ':');
		int l;

		l=strlen(hostname)-(port ? strlen(port):0);

		if (l > 255)
			l=255;

		if (port)
			portn=atoi(port+1);
		else switch (((const struct sockaddr_in *)addrptr)->sin_family)
		{
		case AF_INET:
			portn=ntohs(((const struct sockaddr_in *)addrptr)
				    ->sin_port);
			break;
#if HAVE_IPV6
		case AF_INET6:
			portn=ntohs(((const struct sockaddr_in6 *)addrptr)
				    ->sin6_port);
			break;
#endif
		default:
			failed(si, EPROTO); /* Shouldn't happen */
			return -1;
		}

		si->misc_buffer[3]=(char)3;
		si->misc_buffer[4]=(char)l;
		memcpy(si->misc_buffer+5, hostname, l);
		l += 5;
		si->misc_buffer[l++]= (char)(portn/256);
		si->misc_buffer[l++]= (char)portn;
		si->misc_left=l;
	}
	else switch (((const struct sockaddr_in *)addrptr)->sin_family) {
	case AF_INET:
		si->misc_buffer[3]=1;

		{
			struct in_addr sin;

			sin=((const struct sockaddr_in *)addrptr)
				->sin_addr;

#if 0
			if (si->state == listening)
				sin.s_addr=INADDR_ANY;
#endif

			memcpy(&si->misc_buffer[4], &sin, 4);
		}

		memcpy(&si->misc_buffer[8],
		       &((const struct sockaddr_in *)addrptr)
		       ->sin_port, 2);
		if (portn > 0)
			memcpy(&si->misc_buffer[8], portnbuf, 2);

		si->misc_left=10;
		break;
#if HAVE_IPV6
	case AF_INET6:
		si->misc_buffer[3]=4;
		{
			struct in6_addr sin6;

			sin6=((const struct sockaddr_in6 *)addrptr)
				->sin6_addr;

#if 0
			if (si->state == listening)
				sin6=in6addr_any;
#endif
			memcpy(&si->misc_buffer[4], &sin6, 16);
		}
		memcpy(&si->misc_buffer[20],
		       &((const struct sockaddr_in6 *)addrptr)
		       ->sin6_port, 2);
		if (portn > 0)
			memcpy(&si->misc_buffer[20], portnbuf, 2);
		si->misc_left=22;
		break;
#endif
	default:
		failed(si, EPROTO); /* Shouldn't happen */
		return -1;
	}
	si->connect_want_read=0;
	si->connect_want_write=1;
	si->connect_func=send_connect_packet;
	return 1;
}

static int read_connect_packet(struct sox_info *si);

/*
** Sending the connection request.
*/

static int send_connect_packet(struct sox_info *si)
{
	int n=async_write(si->fd, si->misc_ptr, si->misc_left);

	if (n <= 0)
	{
		failed(si, errno);
		return -1;
	}

	D(DEBUG_CONNECT)
		{
			fprintf(stderr,
				"socks(%d): Request: wrote %d bytes.\n",
				si->fd, n);
		}
	si->connect_func=send_connect_packet;
	si->misc_ptr += n;
	si->misc_left -= n;

	if (si->misc_left)
	{
		si->connect_want_read=0;
		si->connect_want_write=1;
		return 1;
	}

	si->connect_want_read=1;
	si->connect_want_write=0;
	si->connect_func=read_connect_packet;
	si->misc_ptr=si->misc_buffer;
	si->misc_left=4;
	return 1;
}

/*
** Common reply packet processing
*/

static int read_reply_packet(struct sox_info *si)
{
	int n=async_read(si->fd, si->misc_ptr, si->misc_left);
	int errnumber;

	if (n <= 0)
	{
		failed(si, errno);
		return -1;
	}

	si->misc_ptr += n;
	si->misc_left -= n;

	D(DEBUG_CONNECT)
		{
			fprintf(stderr,
				"socks(%d): Request: received %d bytes.\n",
				si->fd, n);
		}

	/*
	** After receiving the address type we know the actual length
	** of the response.
	*/

	if (si->misc_left == 0 && si->misc_ptr == si->misc_buffer + 4)
		switch (si->misc_buffer[3]) {
		case 1:
			si->misc_left=6;
			break;
		case 4:
			si->misc_left=18;
			break;
		default:
			failed(si, errno);
			return -1;
		}

	if (si->misc_left)
	{
		si->connect_want_read=1;
		si->connect_want_write=0;
		return 1;
	}
	switch (si->misc_buffer[0] == 5 ?
		si->misc_buffer[1]:1) {
	case 0:
		errnumber=0;
		break;
	case 2:
		errnumber=EACCES;
		break;
#ifdef EHOSTUNREACH
	case 3:
		errnumber=EHOSTUNREACH;
		break;
#endif
	case 4:
		errnumber=ENETUNREACH;
		break;
	case 5:
		errnumber=ECONNREFUSED;
		break;
	case 6:
		errnumber=ETIMEDOUT;
		break;
	case 8:
		errnumber=EAFNOSUPPORT;
		break;
	default:
		errnumber=EPROTO;
		break;
	}

	D(DEBUG_CONNECT)
		{
			fprintf(stderr,
				"socks(%d): Request status: %d.\n",
				si->fd, errnumber);
		}

	if (errnumber)
	{
		failed(si, errnumber);
		return -1;
	}

	return 0;
}

/*
** Save address from reply packet into local_addr or remote_addr
*/

static void set_reply_address(struct sox_info *si,
			      SOCKADDR_STORAGE *addr)
{

	memset(addr, 0, sizeof(*addr));

	switch (si->misc_buffer[3]) {
	case 1:
		{
			struct sockaddr_in *sin=(struct sockaddr_in *)addr;

			sin->sin_family=AF_INET;
			memcpy(&sin->sin_addr, si->misc_buffer+4, 4);
			memcpy(&sin->sin_port, si->misc_buffer+8, 2);
		}
		break;
#if HAVE_IPV6
	case 4:
		{
			struct sockaddr_in6 *sin6=
				(struct sockaddr_in6 *)addr;

			sin6->sin6_family=AF_INET6;
			memcpy(&sin6->sin6_addr, si->misc_buffer+4, 16);
			memcpy(&sin6->sin6_port, si->misc_buffer+20, 2);
		}
		break;
#endif
	}
}

static int read_accept_packet(struct sox_info *si);

/*
** Reading the response to the connection request.
*/

static int read_connect_packet(struct sox_info *si)
{
	int errnumber;

	si->connect_func=read_connect_packet;

	errnumber=read_reply_packet(si);

	if (errnumber)
		return errnumber;

	set_reply_address(si, &si->local_addr);

	switch (si->state) {
	case listening:
		si->connect_func=read_accept_packet;
		si->misc_ptr=si->misc_buffer;
		si->misc_left=4;
		break;
	default:
		si->state=connected;
		if (si->misc_buffer)
		{
			free(si->misc_buffer);
			si->misc_buffer=NULL;
		}
		break;
	}
	return 0;
}
/*
** Second reply to the BIND command comes in when a connection is received.
*/

static int read_accept_packet(struct sox_info *si)
{
	int errnumber;

	errnumber=read_reply_packet(si);

	if (errnumber)
		return errnumber;

	set_reply_address(si, &si->remote_addr);
	si->state=accepted;

	if (si->misc_buffer)
	{
		free(si->misc_buffer);
		si->misc_buffer=NULL;
	}
	return 0;
}

int Rclose(int fd)
{
	int rc;

	liblock();

	if (free_ptr(find_ptr(fd)))
		D(DEBUG_MISC)
			fprintf(stderr,
				"socks(%d): Intercepted close().\n",
				fd);
	rc=sys_close(fd);
	libunlock();
	return rc;
}

int Rfclose(FILE *fp)
{
	int rc;

	liblock();

	if (free_ptr(find_ptr(fileno(fp))))
		D(DEBUG_MISC)
			fprintf(stderr,
				"socks(%d): Intercepted fclose().\n",
				fileno(fp));
	rc=sys_fclose(fp);
	libunlock();
	return rc;
}

static void fakeselect(int n, fd_set *r, fd_set *w, fd_set *e, int fd)
{
	D(DEBUG_SELECT)
		fprintf(stderr,
			"socks(%d): Synthesizing select(%s%s%s) completion.\n",
			fd, r ? "r":"", w ? "w":"", e ? "e":"");

	while (n)
	{
		--n;

		if (r) FD_CLR(n, r);
		if (w) FD_CLR(n, w);
		if (e) FD_CLR(n, e);

		if (fd == n)
		{
			if (r) FD_SET(n, r);
			if (w) FD_SET(n, w);
			if (e) FD_SET(n, e);
		}
	}
}

/*
** Intersept select and poll system calls for sockets that are still
** connecting.  Similar logic for both select and poll calls:
**
** If the application is waiting for the connecting socket, invoke the
** current connection handler.  If the socket gets connected return the
** connected indicator to the application, otherwise turn off and do not
** check the socket.
*/

static void sel_clear(const char *state, int fd,
		      fd_set *r, fd_set *w, fd_set *e)
{
	/* Dead listening socket */

	if (r)
	{
		if (FD_ISSET(fd, r))
			D(DEBUG_SELECT)
				fprintf(stderr,
					"socks(%d): clearing %s socket from"
					" read select set.\n", fd,
					state);
		FD_CLR(fd, r);
	}

	if (w)
	{
		if (FD_ISSET(fd, w))
			D(DEBUG_SELECT)
				fprintf(stderr,
					"socks(%d): clearing %s socket from"
					" write select set.\n", fd, state);
		FD_CLR(fd, w);
	}

	if (e)
	{
		if (FD_ISSET(fd, e))
			D(DEBUG_SELECT)
				fprintf(stderr,
					"socks(%d): clearing %s socket from"
					" exception select set.\n", fd,
					state);
		FD_CLR(fd, e);
	}
}

static int real_select(int n, fd_set *r, fd_set *w, fd_set *e,
		       struct timeval *tv);

int Rselect(int n, fd_set *r, fd_set *w, fd_set *e,
	    struct timeval *tv)
{
	int rc;

	liblock();
	rc=real_select(n, r, w, e, tv);
	libunlock();
	return rc;
}

static int real_select(int n, fd_set *r, fd_set *w, fd_set *e,
		       struct timeval *tv)
{
	int f;
	fd_set rd, wd;
	int cnt;
	fd_set rcopy, wcopy, ecopy, *rsaveptr, *wsaveptr, *esaveptr;
	int flag=0;
	struct timeval tvcopy, tvsave;
	time_t tbefore=0, tafter;

	memset(&tvsave, 0, sizeof(tvsave));
#if 0
	D(DEBUG_SELECT)
		{
			fprintf(stderr, "socks: Intercepted a select() call.\n");
			if (r)
			{
				fprintf(stderr, "socks: read:");
				for (f=0; f<n; f++)
					if (FD_ISSET(f, r))
						fprintf(stderr, " %d", f);
				fprintf(stderr, "\n");
			}

			if (w)
			{
				fprintf(stderr, "socks: write:");
				for (f=0; f<n; f++)
					if (FD_ISSET(f, w))
						fprintf(stderr, " %d", f);
				fprintf(stderr, "\n");
			}

			if (e)
			{
				fprintf(stderr, "socks: except:");
				for (f=0; f<n; f++)
					if (FD_ISSET(f, e))
						fprintf(stderr, " %d", f);
				fprintf(stderr, "\n");
			}
		}
#endif

	/* Make a copy of the original file descriptor sets */

	if (r)
	{
		memcpy(&rcopy, r, sizeof(rcopy));
		rsaveptr= &rcopy;
	}
	else rsaveptr=NULL;

	if (w)
	{
		memcpy(&wcopy, w, sizeof(wcopy));
		wsaveptr= &wcopy;
	}
	else	wsaveptr=NULL;

	if (e)
	{
		memcpy(&ecopy, e, sizeof(ecopy));
		esaveptr= &ecopy;
	}
	else	esaveptr=NULL;

	if (tv)
	{
		tvcopy= *tv;
		tv=&tvcopy;
	}

	do
	{
		if (flag) /* Restore original sets */
		{
			if (rsaveptr)
				memcpy(r, rsaveptr, sizeof(*rsaveptr));
			else
				r=NULL;

			if (wsaveptr)
				memcpy(w, wsaveptr, sizeof(*wsaveptr));
			else
				w=NULL;

			if (esaveptr)
				memcpy(e, esaveptr, sizeof(*esaveptr));
			else
				e=NULL;
		}
		flag=1;

		for (f=0; f<HASHSIZE; ++f)
		{
			struct sox_info *si;

			for (si=soxtab[f]; si; si=si->next)
			{
				if (si->fd < n && si->state == dead)
				{
					sel_clear("dead", si->fd, r, w, e);
				}

				/* Check if this is a connecting socket */
				if (si->fd < n &&
				    (si->state == connecting ||
				     si->state == listening) &&
				    ((r && FD_ISSET(si->fd, r)) ||
				     (w && FD_ISSET(si->fd, w)) ||
				     (e && FD_ISSET(si->fd, e))))
				{
					int rc;

					D(DEBUG_SELECT)
						fprintf(stderr,
							"socks(%d): "
							"Intercepted connecting socket, invoking handler.\n",
							si->fd);

					/* Call the connect func instead */

					rc=(*si->connect_func)(si);

					if (rc > 0)
					{
						/*
						** Still connecting,
						** substitute the event the
						** connection function
						** wants, instead.
						*/

						/*
						** Clear the requested events.
						*/

						sel_clear("connecting",
							  si->fd,
							  r, w, e);

						/*
						** Now, set the connection
						** function's event, creating
						** the event set if needed.
						*/

						if (si->connect_want_read)
						{
							D(DEBUG_SELECT)
								fprintf(stderr,
									"socks(%d): "
									"Intercepted socket wants to read.\n",
									si->fd);

							if (!r)
							{
								FD_ZERO(&rd);
								r= &rd;
							}
							FD_SET(si->fd, r);
						}

						if (si->connect_want_write)
						{
							D(DEBUG_SELECT)
								fprintf(stderr,
									"socks(%d): "
									"Intercepted socket wants to write.\n",
									si->fd);

							if (!w)
							{
								FD_ZERO(&wd);
								w= &wd;
							}
							FD_SET(si->fd, w);
						}
						continue;
					}

					D(DEBUG_SELECT)
						fprintf(stderr,
							"socks(%d): "
							"Socket connected.\n",
							si->fd);

					/*
					** The socket got connected (or failed)
					** so pretend the event happened.
					*/

					fakeselect(n, r, w, e, si->fd);
					return 1;
				}

				/*
				** The socket is in an error state, so pretend
				** the event has happened.
				*/

				if (si->state == connectfailed)
				{
					D(DEBUG_SELECT)
						fprintf(stderr,
							"socks(%d): "
							"Socket connection failed.\n",
							si->fd);

					fakeselect(n, r, w, e, si->fd);
					return 1;
				}
			}
		}

		if (tv)
		{
			tvsave=*tv;
			tbefore=time(NULL);
		}

#if 0
		D(DEBUG_SELECT)
			fprintf(stderr,
				"socks: select() %s timeout\n",
				tv ? "with":"without");
#endif

		cnt=sys_select(n, r, w, e, tv);

		if (tv)
		{
			tafter=time(NULL);
			if (tafter < tbefore)
				tafter=tbefore; /* Sanity check */
			tafter -= tbefore;

			tvsave.tv_sec= tvsave.tv_sec > tafter ?
				tvsave.tv_sec - tafter:(tvsave.tv_usec=0);
			*tv=tvsave;
		}

		D(DEBUG_SELECT)
			fprintf(stderr,
				"socks: select() completed with status %d\n",
				(int)cnt);

		/*
		** After the select turn off any events for connecting
		** sockets, since those events are really for the connection
		** function's benefit.
		*/

		for (f=0; f<HASHSIZE; ++f)
		{
			struct sox_info *si;

			for (si=soxtab[f]; si; si=si->next)
			{
				if (si->fd < n &&
				    (si->state == connecting ||
				     si->state == listening) &&
				    ((r && FD_ISSET(si->fd, r)) ||
				     (w && FD_ISSET(si->fd, w)) ||
				     (e && FD_ISSET(si->fd, e))))
				{
					sel_clear("connecting",
						  si->fd,
						  r, w, e);
					--cnt;
					D(DEBUG_SELECT)
						fprintf(stderr,
							"socks(%d): Reducing select() return count by 1.\n",
							si->fd);
				}
			}
		}
		D(DEBUG_SELECT)
			fprintf(stderr,
				"socks: Final select() status: %d%s\n",
				(int)cnt, cnt == 0 && tv == NULL
				? ", trying again!":"");
	} while (cnt == 0 && (tv == NULL || tv->tv_sec || tv->tv_usec));

	return cnt;
}

/*
** After a socket's sox connection failed, intercept getsockopt() and return
** the error code logged earlier.
*/

int Rgetsockopt(int  s, int level, int optname, void *optval,
		SOCKLEN_T *optlen)
{
	if (level == SOL_SOCKET && optname == SO_ERROR)
	{
		struct sox_info **si;

		liblock();
		si=find_ptr(s);

		if (*si && (*si)->state == connectfailed)
		{
			if ( *optlen < sizeof(int))
			{
				libunlock();
				errno=ENOMEM;
				return -1;
			}

			memcpy(optval, &(*si)->errnumber, sizeof(int));
			*optlen=sizeof(int);
			libunlock();
			return 0;
		}
		libunlock();
	}

	return sys_getsockopt(s, level, optname, optval, optlen);
}

/*
** The poll() version of the select() logic.
*/

#if HAVE_POLL

static int Rpoll_cpy(struct pollfd *ufds, NFDS_T nfds, int timeout,
		     struct pollfd *origFds);

int Rpoll(struct pollfd *ufds, NFDS_T nfds, int timeout)
{
	struct pollfd *cpy=(struct pollfd *)malloc(nfds * sizeof(*ufds));
	int rc;

	if (!cpy)
		return -1;

	/*
	** Make a backup copy of the original file descriptors being polled.
	*/

	liblock();
	memcpy(cpy, ufds, nfds * sizeof(*ufds));
	rc=Rpoll_cpy(ufds, nfds, timeout, cpy);
	free(cpy);
	libunlock();
	return rc;
}

static int Rpoll_cpy(struct pollfd *ufds, NFDS_T nfds, int timeout,
		     struct pollfd *origFds)
{
	int flag=0;
	int n;
	int rc;
	int cnt;
	time_t tbefore, tafter;

	D(DEBUG_SELECT)
		fprintf(stderr, "socks: Intercepted a poll() call.\n");

	do
	{
		if (flag) /* Restore original masks */
		{
			memcpy(ufds, origFds, nfds * sizeof(*ufds));
		}
		flag=1;

		for (n=0; n<nfds; n++)
		{
			struct sox_info **si;

			if ((ufds[n].events & (POLLIN | POLLOUT | POLLPRI))
			    == 0)
				continue;

			si=find_ptr(ufds[n].fd);

			if (!*si)
				continue;

			if ((*si)->state == dead)
			{
				D(DEBUG_SELECT)
					fprintf(stderr, "socks(%d): Clearing dead socket from event list.\n",
						(*si)->fd);

				ufds[n].events=0;
			}

			if ( (*si)->state == connecting ||
			     (*si)->state == listening)
			{
				D(DEBUG_SELECT)
					fprintf(stderr,
						"socks(%d): "
						"Intercepted connecting socket, invoking handler.\n",
						(*si)->fd);

				rc=(*(*si)->connect_func)(*si);

				if (rc > 0)
				{
					ufds[n].events=0;

					if ((*si)->connect_want_read)
					{
						D(DEBUG_SELECT)
							fprintf(stderr,
								"socks(%d): "
								"Intercepted socket wants to read.\n",
								ufds[n].fd);

						ufds[n].events |= POLLIN;
					}

					if ((*si)->connect_want_write)
					{
						D(DEBUG_SELECT)
							fprintf(stderr,
								"socks(%d): "
								"Intercepted socket wants to write.\n",
								ufds[n].fd);
						ufds[n].events |= POLLOUT;
					}
					continue;
				}
				D(DEBUG_SELECT)
					fprintf(stderr,
						"socks(%d): "
						"Socket connected.\n",
						ufds[n].fd);

				for (rc=0; rc<nfds; ++rc)
					ufds[rc].revents=0;
				ufds[n].revents=ufds[n].events;
				return 1;
			}

			if ((*si)->state == connectfailed)
			{
				D(DEBUG_SELECT)
					fprintf(stderr,
						"socks(%d): "
						"Socket connection failed.\n",
						ufds[n].fd);

				for (rc=0; rc<nfds; ++rc)
					ufds[rc].revents=0;
				ufds[n].revents=ufds[n].events;
				return 1;
			}
		}

		tbefore=time(NULL);

		cnt=sys_poll(ufds, nfds, timeout);

		if (timeout >= 0)
		{
			tafter=time(NULL);
			if (tafter < tbefore)
				tafter=tbefore; /* Sanity check */
			tafter -= tbefore;

			tafter *= 1000;

			if (timeout > tafter)
				timeout -= tafter;
			else
				timeout=0;
		}

		D(DEBUG_SELECT)
			fprintf(stderr,
				"socks: select() completed with status %d\n",
				(int)cnt);

		for (n=0; n<nfds; n++)
		{
			struct sox_info **si;

			if (ufds[n].revents == 0)
				continue;

			si=find_ptr(ufds[n].fd);

			if (!*si)
				continue;

			if ( (*si)->state == connecting ||
			     (*si)->state == listening)
			{
				D(DEBUG_SELECT)
					{
						fprintf(stderr, "socks(%d): Clearing connecting socket from event list.\n",
							ufds[n].fd);

						fprintf(stderr,
							"socks(%d): Reducing poll() return count by 1.\n",
							ufds[n].fd);
					}
				ufds[n].revents=0;
				--cnt;
			}
		}
		D(DEBUG_SELECT)
			fprintf(stderr,
				"socks: Final poll() status: %d%s\n",
				(int)cnt, cnt == 0 && timeout < 0
				? ", trying again!":"");

	} while (cnt == 0 && timeout);

	return cnt;
}
#endif

static int listen_cb(int fd, const struct sockaddr *sa, SOCKLEN_T sal)
{
	return 0;
}

/*
** If we intercept a listen() on a monitored socket, it's a noop.
**
** If we intercept a listen() on some other socket, play it safe and monitor
** it.  Get its local name, via getsockname() and pretend do the bind() logic.
** to set things up.
*/

static int real_listen(int s, int backlog);

int Rlisten(int s, int backlog)
{
	int rc;

	liblock();
	rc=real_listen(s, backlog);
	libunlock();
	return rc;
}

static int real_listen(int s, int backlog)
{
	struct sox_info **p=find_ptr(s);
	SOCKADDR_STORAGE tempAddr;
	SOCKLEN_T tempAddrLen;
	int sock_type;
	SOCKLEN_T sock_size;

	D(DEBUG_MISC)
		fprintf(stderr,
			"socks(%d): Intercepted listen().\n", s);

	if (*p)
	{
		D(DEBUG_MISC)
			fprintf(stderr, "socks(%d): socket state is %s\n",
				(*p)->fd, sox_state_str( (*p)->state));

		switch ( (*p)->state ) {
		case listening:
			D(DEBUG_MISC)
				fprintf(stderr,
					"socks(%d): already proxied, returning 0.\n", s);
			return 0; /* Punt */
		default:
			break;
		}
		free_ptr(p);
	}

	if (sys_listen(s, backlog))
	{
		D(DEBUG_MISC)
			{
				fprintf(stderr,
					"socks(%d): listen() failed, ", s);
				perror("errno");
			}

		return -1;
	}

	sock_size=sizeof(sock_type);

	if (sys_getsockopt(s, SOL_SOCKET, SO_TYPE, &sock_type, &sock_size)
	    || sock_type != SOCK_STREAM)
	{
		D(DEBUG_MISC)
			fprintf(stderr,
				"socks(%d): Is not a stream socket.\n", s);
		return 0; /* We won't meddle */
	}

	tempAddrLen=sizeof(tempAddr);

	if (sys_getsockname(s, (struct sockaddr *)&tempAddr, &tempAddrLen) < 0)
		return -1;

	switch (tempAddr.SS_family) {
	case AF_INET:
#if HAVE_IPV6
	case AF_INET6:
#endif
		break;
	default:
		D(DEBUG_MISC)
			fprintf(stderr,
				"socks(%d): unknown address family.\n",
				s);
		return 0; /* We won't meddle */
	}

	D(DEBUG_MISC)
		{
			fprintf(stderr,
				"socks(%d): Socket is listening on ", s);
			printaddr(stderr, &tempAddr);
			fprintf(stderr, ", proxying.\n");
		}

	return connect_or_bind(s, (struct sockaddr *)&tempAddr,
			       tempAddrLen, listening, listen_cb);

}

static SOCKLEN_T getAddrLen(SOCKADDR_STORAGE *a)
{
	switch ( ((struct sockaddr_in *)a)->sin_family ) {
#if HAVE_IPV6
	case AF_INET6:
		return sizeof(struct sockaddr_in6);
#endif
	default:
		break;
	}
	return sizeof(struct sockaddr_in);
}

static void copy_addr(SOCKADDR_STORAGE *s, struct sockaddr *addr,
		      SOCKLEN_T *addrlen)
{
	SOCKLEN_T l;

	l=getAddrLen(s);

	if (addrlen && l > *addrlen)
		l= *addrlen;

	if (addrlen)
		*addrlen=l;
	if (addr)
		memcpy(addr, s, l);
}
/*
** Monitor file descriptors across dupes.
*/

static int dup_common(struct sox_info **siOld, int fdNew);

int Rdup(int fd)
{
	struct sox_info **siOld;
	int fdNew;
	int rc;

	liblock();

	siOld=find_ptr(fd);
	if (!*siOld)
	{
		rc=sys_dup(fd);
	}
	else
	{
		fdNew=sys_dup(fd);
		if (fdNew < 0)
		{
			libunlock();
			return -1;
		}

		rc=dup_common(siOld, fdNew);
	}
	libunlock();
	return rc;
}

int Rdup2(int fd, int fdNew)
{
	struct sox_info **siOld;
	int rc;

	liblock();

	siOld=find_ptr(fd);

	if (!*siOld)
	{
		rc=sys_dup2(fd, fdNew);
		libunlock();
		return rc;
	}

	fdNew=sys_dup2(fd, fdNew);
	if (fdNew < 0)
	{
		libunlock();
		return -1;
	}

	rc=dup_common(siOld, fdNew);
	libunlock();
	return rc;
}

static int dup_common(struct sox_info **siOld, int fdNew)
{
	struct sox_info **siNew=reset_fd(fdNew);

	D(DEBUG_MISC)
		fprintf(stderr,
			"socks(%d): Intercepted dup -> %d.\n", (*siOld)->fd,
			fdNew);

	if (!siNew)
	{
		sys_close(fdNew);
		return -1;
	}

	(*siNew)->state=dupe;
	(*siNew)->remote_addr=(*siOld)->remote_addr;
	(*siNew)->local_addr=(*siOld)->local_addr;
	(*siNew)->server=(*siOld)->server;
	return fdNew;
}

/*
** Intercept accept().
**
** For blocking sockets, wait until the monitored socket goes into
** accepted state.
**
** For non-blocking sockets return EWOULDBLOCK unless the monitored socket
** is in the accepted state.
**
** Punt the accept() simply by dup-ing the socket, and set the original's
** state to dead, to mark the socket as untouchable.
*/
static int real_accept(int s, struct sockaddr *addr, SOCKLEN_T *addrlen);

int Raccept(int s, struct sockaddr *addr, SOCKLEN_T *addrlen)
{
	int rc;

	liblock();
	rc=real_accept(s, addr, addrlen);
	libunlock();
	return rc;
}

static int real_accept(int s, struct sockaddr *addr, SOCKLEN_T *addrlen)
{
	struct sox_info **p=find_ptr(s);
	int flags;

	if (!*p)
		return sys_accept(s, addr, addrlen);

	D(DEBUG_CONNECT)
		fprintf(stderr,
			"socks(%d): Intercepted accept() in state %s.\n", s,
			sox_state_str((*p)->state));

	switch ( (*p)->state ) {
	case listening:
	case accepted:
		break;
	default:
		D(DEBUG_CONNECT)
			fprintf(stderr,
				"socks(%d): accept(): invalid socket state, returning error indication.\n", s);
		errno=EWOULDBLOCK;
		return -1; /* No need to hang forever */
	}

	flags=fcntl(s, F_GETFL);

	if (flags < 0)
	{
		D(DEBUG_ERRORS)
			{
				fprintf(stderr,
					"socks(%d): fcntl(F_GETFL) failed, ",
					s);
				perror("errno");
			}
		return -1;
	}

	if ( (flags & O_NONBLOCK) == 0 && (*p)->state == listening)
	{
		D(DEBUG_CONNECT)
			fprintf(stderr,
				"socks(%d): Blocking socket, waiting for connection.\n", s);
		if (manual_connect(p))
			return -1;
	}

	if ( (*p)->state != accepted)
	{
		D(DEBUG_CONNECT)
			fprintf(stderr,
				"socks(%d): accept(): non-blocking socket not ready, returning error indication.\n", s);

#ifdef EWOULDBLOCK
		errno=EWOULDBLOCK;
#else
#ifdef EAGAIN
		errno=EAGAIN;
#else
		errno=EINPROGRESS;
#endif
#endif
		return -1;
	}

	D(DEBUG_CONNECT)
		fprintf(stderr,
			"socks(%d): accept(): socket ready, duping, marking original as dead.\n", s);

	copy_addr(&(*p)->remote_addr, addr, addrlen);
	(*p)->state=dead;
	return Rdup( (*p)->fd);
}


/*
** Intercept getsockname and getpeername for monitored sockets.
*/

static int real_getsockname(int s, struct sockaddr *name, SOCKLEN_T *namelen);

int Rgetsockname(int s, struct sockaddr *name, SOCKLEN_T *namelen)
{
	int rc;

	liblock();
	rc=real_getsockname(s, name, namelen);
	libunlock();
	return rc;
}

static int real_getsockname(int s, struct sockaddr *name, SOCKLEN_T *namelen)
{
	struct sox_info **p=find_ptr(s);

	if (!*p)
		return sys_getsockname(s, name, namelen);

	D(DEBUG_MISC)
		fprintf(stderr,
			"socks(%d): Intercepted getsockname() in state %s.\n",
			s, sox_state_str( (*p)->state ));

	switch ( (*p)->state ) {
	case listening:
	case connected:
	case accepted:
	case dead:
	case dupe:
		D(DEBUG_MISC)
			{
				fprintf(stderr,
					"socks(%d): Returning address ", s);
				printaddr(stderr, &(*p)->local_addr);
				fprintf(stderr, "\n");
			}

		copy_addr(&(*p)->local_addr, name, namelen);
		return 0;
	default:
		break;
	}

	D(DEBUG_MISC)
		fprintf(stderr,
			"socks(%d): Socket is not in a valid state.\n", s);
	errno=ENOTCONN;
	return -1;
}

static int real_getpeername(int s, struct sockaddr *name, SOCKLEN_T *namelen);

int Rgetpeername(int s, struct sockaddr *name, SOCKLEN_T *namelen)
{
	int rc;

	liblock();
	rc=real_getpeername(s, name, namelen);
	libunlock();
	return rc;
}

static int real_getpeername(int s, struct sockaddr *name, SOCKLEN_T *namelen)
{
	struct sox_info **p=find_ptr(s);

	if (!*p)
		return sys_getpeername(s, name, namelen);

	D(DEBUG_MISC)
		fprintf(stderr,
			"socks(%d): Intercepted getpeername() in state %s.\n",
			s, sox_state_str((*p)->state));

	switch ( (*p)->state ) {
	case connected:
	case accepted:
	case dead:
	case dupe:
		D(DEBUG_MISC)
			{
				fprintf(stderr,
					"socks(%d): Returning address ", s);
				printaddr(stderr, &(*p)->remote_addr);
				fprintf(stderr, "\n");
			}
		copy_addr(&(*p)->remote_addr, name, namelen);
		return 0;
	default:
		break;
	}

	D(DEBUG_MISC)
		fprintf(stderr,
			"socks(%d): Socket is not in a valid state.\n", s);
	errno=ENOTCONN;
	return -1;
}
