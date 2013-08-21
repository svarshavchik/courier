/*
** Copyright 1998 - 2001 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"config.h"
#include	<stdio.h>
#include	<string.h>
#include	<stdlib.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	"msghash.h"
#include	"duphash.h"
#include	"comctlfile.h"
#include	"filtersocketdir.h"
#include	"threadlib/threadlib.h"

#include	<sys/types.h>
#if	HAVE_SYS_STAT_H
#include	<sys/stat.h>
#endif
#include	"commsgcancel.h"
#include	"courier.h"

#include	"libfilter/libfilter.h"


struct duphash	top_hash, bottom_hash;

struct dupinfo {
	int	fd;
	void	(*cancelfunc)(const char *);
	struct msghashinfo hi;
	FILE	*cmdfp;
	int	doclosecmdfp;
	} ;


static int hashclient(struct dupinfo *di)
{
int	i;
char	buf[BUFSIZ];
char	*filename;
FILE	*f;

	if (di->cmdfp == 0 && (di->cmdfp=fdopen(di->fd, "w+")) == 0)
	{
		perror("fopen");
		close(di->fd);
		return (-1);
	}

	if (fgets(buf, sizeof(buf), di->cmdfp) == 0 ||
		(filename=strtok(buf, "\n")) == 0)
	{
		fclose(di->cmdfp);
		close(di->fd);
		return (-1);
	}

	if ((f=fopen(filename, "r")) == 0)
	{
		/* Eat the remainder of the request */

		for (;;)
		{
			if (fgets(buf, sizeof(buf), di->cmdfp) == 0)
				break;
			if (strtok(buf, "\r\n") == 0)
				break;
		}

		fprintf(di->cmdfp, "400 dupfilter - cannot open message\n");
		fclose(di->cmdfp);
		close(di->fd);
		return (-1);
	}

	msghash_init(&di->hi);

	for (;;)
	{
	int	c;

		i=0;
		while ((c=getc(f)) != EOF && c != '\n')
		{
			if (i < sizeof(buf)-1)
				buf[i++]=c;
		}
		buf[i]=0;
		if (i == 0 && c == EOF)	break;
		msghash_line(&di->hi, buf);
	}
	msghash_finish(&di->hi);
	fclose(f);
	return (0);
}

static void checkclient(struct dupinfo *di)
{
char	*msgid;
int	isdupe;
int	rejected;
char	buf[BUFSIZ];

	if (di->cmdfp == 0)	return;

	isdupe=0;
	rejected=0;

	for (;;)
	{
		FILE *fp;

		if (fgets(buf, sizeof(buf), di->cmdfp) == 0)
			break;

		if ((msgid=strtok(buf, "\r\n")) == 0)
			break;

		/* It's really the filename */

		if ((fp=fopen(msgid, "r")) == NULL)
		{
			perror(msgid);
			continue;
		}

		for (;;)
		{
			if (fgets(buf, sizeof(buf), fp) == 0)
			{
				msgid=NULL;
				break;
			}

			if ((msgid=strtok(buf, "\r\n")) == 0)
				continue;

			if (*msgid == COMCTLFILE_MSGID)
			{
				++msgid;
				break;
			}
		}
		fclose(fp);
		if (!msgid)
			continue;

		if (duphash_check( &top_hash, &di->hi.md1, msgid,
				isdupe, di->cancelfunc))
			rejected=1;

		if (duphash_check( &bottom_hash, &di->hi.md2, msgid,
				isdupe, di->cancelfunc))
			rejected=1;
		isdupe=1;
	}
	if (rejected)
		fprintf(di->cmdfp, "500 Duplicate message rejected.\n");
	else
		fprintf(di->cmdfp, "200 Ok.\n");

	if (di->doclosecmdfp)
	{
		fclose(di->cmdfp);
		close(di->fd);
	}
}

static void realcancel(const char *p)
{
static const char cancelmsg[]="Message cancelled as a duplicate.";
static const char *cancelmsgp[1]={ cancelmsg };

	(void)msgcancel(p, cancelmsgp, 1, 0);
}

static int hashclient_wrapper(struct dupinfo *di)
{
	if (hashclient(di) < 0)
	{
		di->cmdfp=0;
		return (-1);
	}
	return (0);
}

static void initdupinfo(struct dupinfo *a, struct dupinfo *b)
{
	a->cmdfp=0;
	a->doclosecmdfp=1;
	a->fd=b->fd;
	a->cancelfunc=b->cancelfunc;
}

static int realmode(unsigned nthreads)
{
int	listensock;
struct	cthreadinfo *threads;
struct	dupinfo di;

	listensock=lf_init("filters/dupfilter-mode",
		ALLFILTERSOCKETDIR "/dupfilter",
		ALLFILTERSOCKETDIR "/.dupfilter",
		FILTERSOCKETDIR "/dupfilter",
		FILTERSOCKETDIR "/.dupfilter");

	if (listensock < 0)
		return (1);

	threads=cthread_init(nthreads, sizeof(struct dupinfo),
		(void (*)(void *))&hashclient_wrapper,
		(void (*)(void *))&checkclient);
	if (!threads)
	{
		perror("cthread_init");
		return (1);
	}

	lf_init_completed(listensock);

	for (;;)
	{
		if ((di.fd=lf_accept(listensock)) < 0)	break;

		di.cancelfunc= &realcancel;

		if ( cthread_go(threads,
			(void (*)(void *, void *))&initdupinfo, &di))
		{
			perror("cthread_go");
			break;
		}
	}
	cthread_wait(threads);
	return (0);
}

static void testcancel(const char *p)
{
	printf("Cancel: %s\n", p);
}

static int testmode()
{
int	fd;
struct	dupinfo di;

	for (;;)
	{
		printf("Ready.\n");
		if ((fd=dup(0)) < 0)
		{
			perror("dup");
			return (1);
		}

		di.fd=fd;
		di.cancelfunc= &testcancel;

		di.cmdfp=stdin;
		di.doclosecmdfp=0;
		if (hashclient(&di) < 0)	break;
		checkclient(&di);
	}
	return (0);
}

int main(int argc, char **argv)
{
	if (argc > 1 && strcmp(argv[1], "test") == 0)
	{
		duphash_init(&top_hash, 500, 4);
		duphash_init(&bottom_hash, 500, 4);
		return (testmode());
	}
	else
	{
	char	*fn, *f;
	unsigned hashsize=500;
	unsigned duplevel=4;
	unsigned nthreads=4;

		fn=config_localfilename("filters/dupfilter-hashsize");
		if ( (f=config_read1l(fn)) != 0)
		{
			sscanf(f, "%u", &hashsize);
			free(f);
		}
		free(fn);

		fn=config_localfilename("filters/dupfilter-duplevel");
		if ( (f=config_read1l(fn)) != 0)
		{
			sscanf(f, "%u", &duplevel);
			free(f);
		}
		free(fn);

		fn=config_localfilename("filters/dupfilter-nthreads");
		if ( (f=config_read1l(fn)) != 0)
		{
			sscanf(f, "%u", &nthreads);
			free(f);
		}
		free(fn);

		duphash_init(&top_hash, hashsize, duplevel);
		duphash_init(&bottom_hash, hashsize, duplevel);
		return (realmode(nthreads));
	}
}
