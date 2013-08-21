/*
** Copyright 1998 - 2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"localstatedir.h"
#include	"mydirent.h"
#include	"comctlfile.h"
#include	"comstrinode.h"
#include	"comcargs.h"
#include	"maxlongsize.h"
#include	<sys/types.h>
#if	HAVE_SYS_STAT_H
#include	<sys/stat.h>
#endif
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<time.h>
#include	<pwd.h>
#include	<stdlib.h>
#include	<string.h>


static const char *sortflag=0;
static const char *batchflag=0;

static struct courier_args arginfo[]={
	{"sort", &sortflag},
	{"batch",&batchflag},
	{0, 0}};

/*

Size  Qid                          Date
User   Sender
Status Recipient

*/

static void showqline(const char *a, const char *b, const char *c)
{
	if(batchflag) {
		if(c)
			printf("%s;%s;%s;", a, b, c);
		else
			printf("%s;%s;", a, b);
	}
	else {
		if (c)
			printf("%16s %-*s %s\n", a,
				(int)(sizeof(ino_t)+sizeof(time_t)*2+sizeof(pid_t))*2+3,
				b, c);
		else
			printf("%-16s %s\n", a, b);
	}
}

static void showq(struct ctlfile *ctf, struct stat *stat_buf)
{
char size[MAXLONGSIZE+10], size2[17];
char buf[sizeof("Mar 12 xx:xx")+40];
struct tm *tmptr;
int	n;
struct	passwd *pw;
const char *fld1;
const char *qid="";

	n=ctlfile_searchfirst(ctf, COMCTLFILE_MSGID);
	if (n >= 0)	qid=ctf->lines[n]+1;

	if (stat_buf->st_size < 1024)
		sprintf(size, "%d", (int)stat_buf->st_size);
	else if (stat_buf->st_size < 1024 * 1024)
		sprintf(size, "%d.%dK", (int)stat_buf->st_size / 1024,
			((int)stat_buf->st_size % 1024) * 10 / 1024);
	else
		sprintf(size, "%ld.%02ldM",
			(int)stat_buf->st_size / (1024*1024L),
			((long)stat_buf->st_size % (1024*1024L)) * 100
				/ (1024 * 1024L));
	sprintf(size2, "%-*.*s", (int)sizeof(size2)-1, (int)sizeof(size2)-1,
		size);

	tmptr=localtime(&stat_buf->st_mtime);
	strftime(buf, sizeof(buf)-1, "%b %d %H:%M", tmptr);

	showqline(size2, qid, buf);

	pw=getpwuid(stat_buf->st_uid);
	fld1=pw ? pw->pw_name:"";
	n=ctlfile_searchfirst(ctf, COMCTLFILE_SENDER);
	qid= n >= 0 ? ctf->lines[n]+1:"";
	showqline(fld1, qid, 0);

	for (n=0; (unsigned)n < ctf->nreceipients; n++)
	{
	unsigned i;
	const char *sf="";

		for (i=0; ctf->lines[i]; i++)
		{
			switch (ctf->lines[i][0])	{
			case COMCTLFILE_DELSUCCESS:
				if (atoi(ctf->lines[i]+1) == n)
				{
					sf="done";
					break;
				}
				continue;
			case COMCTLFILE_DELFAIL:
				if (atoi(ctf->lines[i]+1) == n)
				{
					sf="fail";
					break;
				}
				continue;
			default:
				continue;
			}
			break;
		}

		showqline(sf, ctf->receipients[n], 0);
	}
	printf("\n");
}

static struct sortlist {
	struct sortlist *next;
	char *filename;
	time_t timestamp;
	} *sortlistp=0;
static unsigned sortcnt;

static int sortcmp(struct sortlist **a, struct sortlist **b)
{
	return ( (*a)->timestamp < (*b)->timestamp ? -1:
		(*a)->timestamp > (*b)->timestamp ? 1:0);
}

int main(int argc, char **argv)
{
DIR	*topdirp, *dirp;
struct dirent *topdire, *dire;
struct	ctlfile ctf;
struct stat stat_buf;
unsigned qcount=0;
uid_t	u=getuid();

	(void)cargs(argc, argv, arginfo);
	if (chdir(courierdir()) || chdir(MSGSDIR))
	{
		perror("chdir");
		exit(1);
	}

	if ((topdirp=opendir(".")) == 0)
	{
		perror("topdirp");
		exit(1);
	}
	
	if(!batchflag)
	{
		showqline("Size            ", "Queue ID", "Date");
		showqline("User", "From", 0);
		showqline("Status", "Recipient", 0);
		showqline("----------------",
			"---------------------------------------------------------", 0);
	}

	while ((topdire=readdir(topdirp)) != 0)
	{
	const char *p=topdire->d_name;

		while (*p)
		{
			if (*p < '0' || *p > '9')	break;
			++p;
		}
		if (*p)	continue;

		if ((dirp=opendir(topdire->d_name)) == 0)	continue;
		while ((dire=readdir(dirp)) != 0)
		{
		char	*name;

			p=dire->d_name;
			if (*p != 'C')	continue;
			if ((name=malloc(strlen(topdire->d_name)
					+strlen(p)+2)) == 0)
			{
				perror("malloc");
				exit(1);
			}
			strcat(strcat(strcpy(name, topdire->d_name), "/"), p);

			if (sortflag)
			{
			struct sortlist *sortp;

				*strchr(name, 'C')='D';
				if (stat(name, &stat_buf) || (u &&
					stat_buf.st_uid != u))
				{
					free(name);
					continue;
				}
				if ( (sortp=(struct sortlist *)
					malloc(sizeof(struct sortlist))) == 0)
				{
					perror("malloc");
					exit(1);
				}
				sortp->next=sortlistp;
				sortlistp=sortp;
				sortp->filename=name;
				sortp->timestamp=stat_buf.st_mtime;
				++sortcnt;
				continue;
			}

			if (ctlfile_openfn(name, &ctf, 1, 0) == 0)
			{
				*strchr(name, 'C')='D';

				if (stat(name, &stat_buf) == 0 && (u == 0 ||
					stat_buf.st_uid == u))
				{
					ctf.starttime=stat_buf.st_mtime;
					showq(&ctf, &stat_buf);
					++qcount;
				}
				ctlfile_close(&ctf);
			}
			free(name);
		}
		closedir(dirp);
	}
	closedir(topdirp);

	if (sortflag && sortcnt)
	{
	struct sortlist **sorta=(struct sortlist **)malloc(
			sortcnt*sizeof(struct sortlist *));
	struct sortlist *sortp;
	unsigned i;

		if (!sorta)
		{
			perror("malloc");
			exit(1);
		}
		for (i=0, sortp=sortlistp; sortp; sortp=sortp->next)
			sorta[i++]=sortp;
		qsort(sorta, sortcnt, sizeof(*sorta),
			( int (*)(const void *, const void *))sortcmp);

		for (i=0; i<sortcnt; i++)
		{
			if (stat(sorta[i]->filename, &stat_buf)) continue;
			*strchr(sorta[i]->filename, 'D')='C';
			if (ctlfile_openfn(sorta[i]->filename, &ctf, 1, 0))
				continue;
			ctf.starttime=stat_buf.st_mtime;
			showq(&ctf, &stat_buf);
			ctlfile_close(&ctf);
			++qcount;
		}
	}

	if(batchflag)
		printf("messages: %d\n",qcount);
	else
		printf("%4d messages.\n", qcount);
	return (0);
}
