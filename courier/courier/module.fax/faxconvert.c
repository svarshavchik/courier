/*
** Copyright 2002-2009 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "config.h"
#include "courier.h"
#include "faxconvert.h"
#include "comfax.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include "rfc822/rfc822.h"
#include "rfc822/rfc822hdr.h"
#include "rfc822/rfc2047.h"
#include "rfc2045/rfc2045.h"
#include "rfc2045/rfc2045charset.h"
#include "numlib/numlib.h"
#include "gpglib/gpglib.h"
#include <courier-unicode.h>

#include <sys/types.h>
#include <sys/stat.h>
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif
#ifndef WIFEXITED
#define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif

#if HAVE_DIRENT_H
#include <dirent.h>
#define NAMLEN(dirent) strlen((dirent)->d_name)
#else
#define dirent direct
#define NAMLEN(dirent) (dirent)->d_namlen
#if HAVE_SYS_NDIR_H
#include <sys/ndir.h>
#endif
#if HAVE_SYS_DIR_H
#include <sys/dir.h>
#endif
#if HAVE_NDIR_H
#include <ndir.h>
#endif
#endif


#include "filterbindir.h"
#include "faxtmpdir.h"

#define SUBTMPDIR FAXTMPDIR "/.tmp"

void rfc2045_error(const char *errmsg)
{
	fprintf(stderr, "faxconvert: %s\n", errmsg);
	exit(1);
}

struct faxconvert_info {
	int fd;
	unsigned partnum;
	struct rfc2045 *topp;
	int npages;
	int flags;

	struct rfc2045 *cover_page_part;
	int cover_page_fd;

	struct faxconvert_unknown_list *unknown_list;
} ;

struct faxconvert_unknown_list {
	struct faxconvert_unknown_list *next;
	char *content_type;
} ;

/*
** Remove a directory's contents.
*/

void rmdir_contents(const char *dir)
{
	DIR *p=opendir(dir);
	struct dirent *de;

	if (!p)
		return;

	while ((de=readdir(p)) != 0)
	{
		char *b;
		const char *c=de->d_name;

		if (strcmp(c, ".") == 0 || strcmp(c, "..") == 0)
			continue;

		b=courier_malloc(strlen(dir) + strlen(c) + 2);

		strcat(strcat(strcpy(b, dir), "/"), c);
		unlink(b);
		free(b);
	}
	closedir(p);
}

static int faxconvert_recursive(struct rfc2045 *, struct faxconvert_info *);
static int convert_cover_page(struct rfc2045 *, struct faxconvert_info *,
			      int, int *);

int faxconvert(const char *filename, int flags, int *ncoverpages)
{
	int fd=open(filename, O_RDONLY);
	struct rfc2045 *rfc2045p;
	int rc;
	struct faxconvert_info info;

	/* Open the MIME message, and parse it */

	if (fd < 0)
	{
		perror(filename);
		return (-1);
	}

	if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0)	/* I'm a lazy SOB */
	{
		perror(filename);
		close(fd);
		return (-1);
	}

	rfc2045p=rfc2045_fromfd(fd);

	if (!rfc2045p)
	{
		perror(filename);
		return (-1);
	}

	/*
	** Ok, clean out SUBTMPDIR and FAXTMPDIR.  We run each filter and tell
	** it to spit its output into SUBTMPDIR, then, we scan SUBTMPDIR,
	** pick up whatever we find there, and move it to FAXTMPDIR.
	*/

	rmdir_contents( SUBTMPDIR );
	rmdir_contents( FAXTMPDIR);

	info.topp=rfc2045p;
	info.fd=fd;
	info.partnum=0;
	info.cover_page_part=NULL;
	info.cover_page_fd= -1;
	info.npages=0;
	info.unknown_list=0;
	info.flags=flags;
	rc=faxconvert_recursive(rfc2045p, &info);

	/* Generate a cover page, now */

	if (rc == 0)
	{
		if (info.cover_page_part == NULL)
		{
			fprintf(stderr, "Error: no cover page (empty document)?\n");
			rc= -1;
		}
		else
		{
			rc= convert_cover_page(info.cover_page_part,
					       &info,
					       info.cover_page_fd,
					       ncoverpages);
		}
	}

	rfc2045_free(rfc2045p);
	close(fd);
	rmdir_contents(SUBTMPDIR);
	return (rc);
}

/*
** Recursively parse the message's MIME structure, and convert each MIME
** section to FAX format.
*/

static int start_filter(const char *, pid_t *);
static int end_filter(const char *, pid_t);

static int decode_filter(const char *, size_t, void *);

static int do_filter(struct rfc2045 *, const char *, int,
		     struct faxconvert_info *, int);

/* Determine if we have a filter for the given content type */

static char *get_filter_prog(const char *content_type)
{
	char *filterprog, *p;
	const char *dummy;

	/*
	** The name of each filter is just its MIME type, with / and .
	** replaced by -, and with '.filter' appended.
	*/

	filterprog=courier_malloc(strlen(content_type)
				  +sizeof(FILTERBINDIR)+20);

	strcpy(filterprog, FILTERBINDIR "/");

	p=filterprog+sizeof(FILTERBINDIR);
	for (dummy=content_type; *dummy; dummy++)
	{
		*p++ = *dummy == '.' || *dummy == '/' ? '-':*dummy;
	}
	*p=0;
	strcat(filterprog, ".filter");

	if (access(filterprog, X_OK) < 0)
	{
		free(filterprog);
		filterprog=NULL;
	}
	return (filterprog);
}

static int faxconvert_recursive(struct rfc2045 *m,
				 struct faxconvert_info *info)
{
	const char *content_type, *dummy;
	char *filterprog;
	int rc;

	rfc2045_mimeinfo(m, &content_type, &dummy, &dummy);

	if (strcmp(content_type, "multipart/mixed") == 0)
	{
		/* Spit out all attachments, one after another */

		for (m=m->firstpart; m; m=m->next)
		{
			if (m->isdummy)
				continue;

			rc=faxconvert_recursive(m, info);
			if (rc)
				return (rc);
		}
		return (0);
	}

	if (strcmp(content_type, "multipart/alternative") == 0)
	{
		struct rfc2045 *bestm=NULL;

		/* Search for MIME content we understand */

		for (m=m->firstpart; m; m=m->next)
		{
			const char *content_type, *dummy;

			if (m->isdummy)
				continue;

			rfc2045_mimeinfo(m, &content_type, &dummy, &dummy);

			if (strncasecmp(content_type, "multipart/", 10) ||
			    get_filter_prog(content_type))
				bestm=m;
		}

		if (bestm)
			return ( faxconvert_recursive(bestm, info));
	}

	if (strcmp(content_type, "multipart/related") == 0)
	{
		char *sid=rfc2045_related_start(m);
		struct rfc2045 *q;

		/* Punt - find the main content, and hope for the best */

		for (q=m->firstpart; q; q=q->next)
		{
			const char *cid;

			if (q->isdummy)	continue;

			cid=rfc2045_content_id(q);

			if (sid && *sid && strcmp(sid, cid))
			{
				struct rfc2045 *qq;

				qq=libmail_gpgmime_is_multipart_signed(q);

				if (!qq) continue;

				/* Don't give up just yet */

				cid=rfc2045_content_id(qq);

				if (sid && *sid && strcmp(sid, cid))
					continue;
			}
			break;
		}
		if (sid)	free(sid);

		if (q)
			return ( faxconvert_recursive(q, info));
	}

	if (strcmp(content_type, "multipart/signed") == 0)
	{
		struct rfc2045 *q;

		/* Punt - ignore the signature */

		for (q=m->firstpart; q; q=q->next)
		{
			if (q->isdummy)	continue;

			break;
		}

		if (q)
			return ( faxconvert_recursive(q, info));
	}

	if (info->partnum == 0)	/* Cover page? */
	{
		if (strcmp(content_type, "text/plain") == 0)
		{
			/* Yes - we have text/plain content */

			++info->partnum;
			info->cover_page_part=m;
			info->cover_page_fd=info->fd;
			return (0);
		}

		/* No - too bad */

		info->cover_page_part=m;
		info->cover_page_fd= -1;
		++info->partnum;
	}
	else if (info->flags & FAX_COVERONLY)
		return (0);	/* Ignore the rest of the message */

	filterprog=get_filter_prog(content_type);

	if (!filterprog && !(info->flags & FAX_OKUNKNOWN))
	{

		fprintf(stderr,
			"This message contains an attachment with the document type of\n"
			"\"%s\".  This document type cannot be faxed.\n",
			content_type);
		return (-1);
	}

	if (!filterprog)
	{
		struct faxconvert_unknown_list *l, **p;

		if ((l=malloc(sizeof(*l))) == NULL ||
		    (l->content_type=strdup(content_type)) == NULL)
		{
			perror("malloc");
			exit(1);
		}

		for (p= &info->unknown_list; *p; p=&(*p)->next)
			;

		*p=l;
		l->next=NULL;
		rc=0;
	}
	else
	{
		rc=do_filter(m, filterprog, info->fd, info, 0);
		free(filterprog);
	}
	++info->partnum;
	return (rc);
}

static int cmp_flist(const void *a, const void *b)
{
	return (strcmp( (*(struct sort_file_list **)a)->filename,
			(*(struct sort_file_list **)b)->filename));
}

/*
** Read all non-hidden entries in a directory.  Return the list in
** sorted order.
*/

struct sort_file_list *read_dir_sort_filenames(const char *dirname,
					       const char *fnpfix)
{
	struct sort_file_list *l=NULL, **ll, *lll;
	unsigned cnt=0, i;
	DIR *dirp;

	if ((dirp=opendir(dirname)) != NULL)
	{
		struct dirent *de;

		while ((de=readdir(dirp)) != NULL)
		{
			if (de->d_name[0] == '.')
				continue;

			lll=(struct sort_file_list *)
				courier_malloc(sizeof(struct sort_file_list)
					       + 1 + strlen(fnpfix)
					       + strlen(de->d_name));

			strcat(strcpy(lll->filename=(char *)(lll+1),
				      fnpfix), de->d_name);
			lll->next=l;
			l=lll;
			++cnt;
		}
		closedir(dirp);
	}
	if (!cnt)
		return (NULL);

	ll=(struct sort_file_list **)courier_malloc(sizeof(l) * cnt);
	cnt=0;
	for (lll=l; lll; lll=lll->next)
		ll[cnt++]=lll;
	qsort(ll, cnt, sizeof(*ll), cmp_flist);

	l=ll[0];
	l->next=NULL;

	for (i=1; i<cnt; i++)
	{
		ll[i-1]->next=ll[i];
		ll[i]->next=NULL;
	}
	free(ll);
	return (l);
}

/*
** Convert a single MIME section into FAX format, by running the appropriate
** filter.
*/

static int do_filter(struct rfc2045 *m,		/* The MIME section */
		     const char *filterprog,	/* The filter */
		     int fd,			/* The message */
		     struct faxconvert_info *info,	/* Housekeeping */
		     int is_cover_page)	/* We're generated text/plain cover */
{
	pid_t pidnum=0;
	int filter_fd;
	off_t start_pos, start_body, end_pos, dummy2;
	int n, rc;
	struct sort_file_list *l, *ll;

	char filenamebuf[sizeof(FAXTMPDIR)+NUMBUFSIZE*2+60];

	filter_fd=start_filter(filterprog, &pidnum);
	if (filter_fd < 0)
		return (-1);

	if (is_cover_page)
		while (info->unknown_list != 0)
		{
			char *buf;

			static const char msg[]="Document type \"%s\" cannot be faxed\n"
				"----------------------------------------------------------------\n\n";
			struct faxconvert_unknown_list *u=info->unknown_list;

			info->unknown_list=u->next;

			buf=malloc(sizeof(msg) + strlen(u->content_type));
			if (!buf)
			{
				perror("malloc");
				return (-1);
			}
			sprintf(buf, msg, u->content_type);
			free(u->content_type);
			free(u);

			decode_filter(buf, strlen(buf), &filter_fd);
		}

	if (fd >= 0)	/* See convert_cover_page() */
	{
		/*
		** Decode the MIME section, push the decoded data to the
		** filtering script.
		*/
		rfc2045_mimepos(m, &start_pos, &end_pos, &start_body, &dummy2,
				&dummy2);

		rfc2045_cdecode_start(m, decode_filter, &filter_fd);

		if (lseek(fd, start_body, SEEK_SET) < 0)
		{
			perror("seek");
			close(filter_fd);
			end_filter(filterprog, pidnum);
			return (-1);
		}

		while (start_body < end_pos)
		{
			char buffer[BUFSIZ];
			int n=read(fd, buffer,
				   end_pos - start_body < BUFSIZ
				   ? end_pos - start_body:BUFSIZ);

			if (n == 0)
				errno=EIO;
			if (n <= 0)
			{
				perror("read");
				close(filter_fd);
				end_filter(filterprog, pidnum);
				return (-1);
			}

			start_body += n;
			n=rfc2045_cdecode(m, buffer, n);
			if (n)
			{
				close(filter_fd);
				end_filter(filterprog, pidnum);
				return (n);
			}
		}

		n=rfc2045_cdecode_end(m);
		if (n)
		{
			close(filter_fd);
			end_filter(filterprog, pidnum);
			return (n);
		}
	}

	close(filter_fd);
	n=end_filter(filterprog, pidnum);
	if (n)
		return (n);

	/*
	** Read the filter's results, sort them, compile them.
	*/

	l=read_dir_sort_filenames(SUBTMPDIR, "");

	if (!l)
	{
		fprintf(stderr, "%s: failed to convert input data.\n",
			filterprog);
		return (-1);
	}

	n=0;

	rc=0;
	while (l)
	{
		char *p;

		sprintf(filenamebuf, "%s/f%04d-%04d.g3", FAXTMPDIR,
			info->partnum, ++n);

		p=courier_malloc(sizeof(SUBTMPDIR) + 1 + strlen(l->filename));
		strcat(strcpy(p, SUBTMPDIR "/"), l->filename);

		if (rename(p, filenamebuf))
		{
			perror(p);
			rc= -1;
		}
		free(p);
		ll=l->next;
		free(l);
		l=ll;
		++info->npages;
	}
	return (rc);
}

/*
** Start the filter process as a child process.
*/

static int start_filter(const char *prog, pid_t *pidptr)
{
	pid_t p;
	int pipefd[2];

	/* Prepare an empty directory for the filter. */

	rmdir_contents(SUBTMPDIR);
	mkdir(SUBTMPDIR, 0700);

	if (pipe(pipefd) < 0)
	{
		perror("pipe");
		return (-1);
	}

	p=fork();
	if (p < 0)
	{
		close(pipefd[0]);
		close(pipefd[1]);
		perror("fork");
		return (-1);
	}

	if (p == 0)
	{
		close(0);
		if (dup(pipefd[0]) < 0)
		{
			perror("dup");
			exit(1);
		}
		close(pipefd[0]);
		close(pipefd[1]);
		execl(prog, prog, SUBTMPDIR, (char *)NULL);
		perror(prog);
		exit(1);
	}

	*pidptr=p;
	close(pipefd[0]);
	return (pipefd[1]);
}


/*
** Helper function writes MIME-decoded content to the filter process.
*/

static int decode_filter(const char *p, size_t n, void *vp)
{
	int fd=*(int *)vp;

	while (n)
	{
		int k=write(fd, p, n);

		if (k == 0)
			errno=EIO;
		if (k <= 0)
		{
			perror("write");
			return (-1);
		}

		p += k;
		n -= k;
	}
	return (0);
}

/*
** Wait for the filter process to finish, return its exit status.
*/

static int end_filter(const char *filterprog, pid_t p)
{
	int waitstat;
	pid_t p2;

	while ((p2=wait(&waitstat)) != p)
	{
		if (p2 >= 0)
			continue;
		if (errno != EINTR)
		{
			perror("wait");
			return (-1);
		}
	}

	if (WIFEXITED(waitstat))
	{
		waitstat=WEXITSTATUS(waitstat);

		if (waitstat == 0)
			return (0);
	}

	fprintf(stderr, "%s: terminated with exit status %d\n",
		filterprog, waitstat);
	return (-1);
}

/*
** Generate cover page.
*/

static char *convnames(const char *);

static int convert_cover_page(struct rfc2045 *rfcp,
			      struct faxconvert_info *info,
			      int fd, int *ncoverpages)
     /*
     ** fd >= 0 if there's initial text/plain content that can go on the
     ** cover page.  fd < 0 if there is no text/plain content to go on the
     ** cover page.
     */
{
	int fd2;
	FILE *fp;
	struct rfc822hdr hbuf;
	off_t start_pos, start_body, end_pos, dummy2;
	char *hfrom=NULL, *hto=NULL, *hdate=NULL, *hsubject=NULL, *p;
	size_t save_pages;
	int rc;

	char npages[NUMBUFSIZE];

	if ((fd2=dup(info->fd)) < 0)
	{
		perror("dup");
		return (-1);
	}

	rfc2045_mimepos(info->topp, &start_pos, &end_pos, &start_body, &dummy2,
			&dummy2);

	if ((fp=fdopen(fd2, "r")) == NULL
	    || fseek(fp, start_pos, SEEK_SET) < 0)
	{
		if (fp)
			fclose(fp);
		close(fd2);
		perror("fdopen");
		return (-1);
	}

	/*
	** Read the message's headers, use it to create the cover page.
	*/

	rfc822hdr_init(&hbuf, BUFSIZ);
	while (rfc822hdr_read(&hbuf, fp, &start_pos, start_body) == 0)
	{
		rfc822hdr_fixname(&hbuf);
		rfc822hdr_collapse(&hbuf);

		if (strcmp(hbuf.header, "from") == 0)
		{
			if (hfrom)
				free(hfrom);
			hfrom=strcpy(courier_malloc(strlen(hbuf.value)+1),
				     hbuf.value);
		}

		if (strcmp(hbuf.header, "to") == 0 ||
		    strcmp(hbuf.header, "cc") == 0)
		{
			if (hto)
			{
				p=courier_malloc(strlen(hto)+
						 strlen(hbuf.value)+2);
				strcat(strcat(strcpy(p, hto), ","),
				       hbuf.value);
				free(hto);
				hto=p;
			}
			else
			{
				hto=strcpy(courier_malloc(strlen(hbuf.value)
							  +1), hbuf.value);
			}
		}

		if (strcmp(hbuf.header, "subject") == 0)
		{
			if (hsubject)
				free(hsubject);

			hsubject=rfc822_display_hdrvalue_tobuf(hbuf.header,
							       hbuf.value,
							       unicode_default_chset(),
							       NULL, NULL);

			if (!hsubject)
				hsubject=strdup(hbuf.value);
		}

		if (strcmp(hbuf.header, "date") == 0)
		{
			if (hdate)
				free(hdate);
			hdate=strcpy(courier_malloc(strlen(hbuf.value)+1),
				     hbuf.value);
		}

	}
	rfc822hdr_free(&hbuf);

	/*
	** The cover page scripts read the cover page information from the
	** environment.
	*/

	p=convnames(hfrom);
	hfrom=courier_malloc(strlen(p)+sizeof("FROM="));
	strcat(strcpy(hfrom, "FROM="), p);
	free(p);
	putenv(hfrom);

	p=convnames(hto);
	hto=courier_malloc(strlen(p)+sizeof("TO="));
	strcat(strcpy(hto, "TO="), p);
	free(p);
	putenv(hto);

	p=courier_malloc(sizeof("SUBJECT=")+strlen(hsubject ? hsubject:""));
	strcat(strcpy(p, "SUBJECT="), hsubject ? hsubject:"");
	if (hsubject)
		free(hsubject);

	putenv(p);

	p=courier_malloc(sizeof("DATE=")+strlen(hdate ? hdate:""));
	strcat(strcpy(p, "DATE="), hdate ? hdate:"");
	if (hdate)
		free(hdate);
	putenv(p);

	libmail_str_size_t(info->npages, npages);

	save_pages = info->npages;

	p=courier_malloc(sizeof("PAGES=")+strlen(npages));
	strcat(strcpy(p, "PAGES="), npages);
	putenv(p);

	info->partnum=0;
	rc=do_filter(rfcp, FILTERBINDIR "/coverpage", fd, info, 1);

	/* Ass-backwards way to figure out # of cover pages */
	*ncoverpages = info->npages - save_pages;
	return (rc);
}

/*
** Grab the names from the RFC 822/2045/2047-formatted header */

static void cnt_names(const char *ptr, size_t cnt, void *vp)
{
	int *ip=(int *)vp;
	size_t i;

	for (i=0; i<cnt; i++)
	{
		if (ptr[i] == '\n')
			++*ip;
		++*ip;
	}
}

static void save_names(const char *ptr, size_t cnt, void *vp)
{
	char **ip=(char **)vp;
	size_t i;

	for (i=0; i<cnt; i++)
	{
		if (ptr[i] == '\n')
		{
			*(*ip)++ = ',';
			*(*ip)++ = ' ';
		}
		else
		{
			*(*ip)++ = ptr[i];
		}
	}
}

static char *convnames(const char *pp)
{
	struct rfc822t *t;
	struct rfc822a *a;
	int al;
	char *q, *p=NULL;

	t=rfc822t_alloc_new(pp ? pp:"", NULL, NULL);
	if (!t)
	{
		perror("malloc");
		exit(1);
	}
	a=rfc822a_alloc(t);
	if (!a)
	{
		perror("malloc");
		exit(1);
	}

	al=1;

	if (rfc822_display_namelist(a, unicode_default_chset(),
				    cnt_names, &al) == 0)
	{
		q=p=courier_malloc(al+1);

		if (rfc822_display_namelist(a, unicode_default_chset(),
					    save_names, &q) == 0)
		{
			if (q - p >= 2 && q[-1] == ' ' && q[-2] == ',')
				/* Trailing comma */
				q -= 2;

			*q=0;
		}
	}
	rfc822a_free(a);
	rfc822t_free(t);
	return (p);
}

#if 0
int main(int argc, char **argv)
{
	if (argc < 2)
		exit(0);
	signal(SIGCHLD, SIG_DFL);
	signal(SIGPIPE, SIG_IGN);
	faxconvert(argv[1]);
	exit(0);
	return (0);
}
#endif
