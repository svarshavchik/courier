/*
** Copyright 2002-2025 S. Varshavchik.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif

#include	"courier.h"
#include	"moduledel.h"
#include	"comqueuename.h"
#include	"comfax.h"
#include	"rfc822/rfc822.h"
#include	"waitlib/waitlib.h"
#include	"numlib/numlib.h"
#include	"comuidgid.h"
#include	"comstrtotime.h"
#include	<stdlib.h>
#include	<locale.h>
#include	<langinfo.h>
#include	<string.h>
#include	<ctype.h>
#include	<signal.h>
#include	<errno.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#if	HAVE_FCNTL_H
#include	<fcntl.h>
#endif
#include	<pwd.h>
#include	<sys/wait.h>
#include	<sys/time.h>
#include	<sys/stat.h>
#include <sys/types.h>
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif
#ifndef WIFEXITED
#define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif
#include	"faxconvert.h"
#include	"sendfax.h"
#ifndef FAXTMPDIR
#include	"faxtmpdir.h"
#endif
#include	"comctlfile.h"

struct faxconv_err_args {
	struct ctlfile *ctf;
	unsigned nreceip;

	std::vector<std::string> *file_list;
	int is_locked;
	unsigned n_cover_pages;
} ;

static int faxsend_cleanup(int, char *, void *);

extern int faxconvert(const char *, int, unsigned *);

static int faxconvert_cleanup(int dummy, char *errmsg, void *vp);

/*
**  All righty - pick up a message to deliver.
*/

static int read_childerrmsg(pid_t child_proc,
			    int errfd,
			    int (*errhandler)(int, char *, void *),
			    void *errhandler_arg)
{
	char errbuf[BUFSIZ];
	int errptr=0;
	int waitstat;
	pid_t p;

	for (;;)
	{
		int n, i;

		if (errptr >= BUFSIZ/4 * 3)
		{
			/* Read errmsg too big, strip it */

			i=BUFSIZ/2;

			for (n=BUFSIZ/2; n<errptr; n++)
				if (errbuf[n] == '\n')
				{
					i= n+1;
					break;
				}

			strcpy(errbuf, "...\n");
			n=4;
			while (i < errptr)
				errbuf[n++] = errbuf[i++];
			errptr=n;
		}

		n=read(errfd, errbuf+errptr, BUFSIZ-1-errptr);
		if (n <= 0)
			break;
		errptr += n;
	}
	errbuf[errptr]=0;
	close(errfd);

	while ((p=wait(&waitstat)) != child_proc)
	{
		if (p < 0 && errno != EINTR)
			break;
	}

	if (p == child_proc && WIFEXITED(waitstat) &&
	    WEXITSTATUS(waitstat) == 0)
		return (0);

	if (errptr == 0)
		strcpy(errbuf, "courierfax: child process crashed.");

	return (*errhandler)(p == child_proc && WIFEXITED(waitstat)
			     ? WEXITSTATUS(waitstat):1,
			     errbuf, errhandler_arg);
}

void courierfax(struct moduledel *p)
{
	struct	ctlfile ctf;
	unsigned nreceipients=p->nreceipients;
	const char *host=p->host;
	const char *receipient;
	unsigned nrecipient=(unsigned)atol(p->receipients[0]);
	int pipefd[2];
	pid_t child_proc;
	int faxopts;
	unsigned n_cover_pages;
	FILE *fp;

	struct faxconv_err_args err_args;

	if (!comgetfaxopts(host, &faxopts))
	{
		clog_msg_start_err();
		clog_msg_str("courierfax: FATAL: invalid host");
		clog_msg_send();
		exit(1);
	}

	setenv("FAXRES", (faxopts & FAX_LOWRES ? "lo":"hi"), 1);

	if (nreceipients != 1)
	{
		clog_msg_start_err();
		clog_msg_str("courierfax: FATAL: # receipients must be 1");
		clog_msg_send();
		exit(1);
	}

	receipient=p->receipients[1];
	nrecipient=(unsigned)atol(p->receipients[0]);

	if (ctlfile_openi(p->inum, &ctf, 0))
		clog_msg_errno();

	/* Convert message to fax format, use a child process */

	if (pipe(pipefd) < 0)
	{
		clog_msg_errno();
	}

	child_proc=fork();
	if (child_proc < 0)
	{
		clog_msg_errno();
		return;
	}

	if (child_proc == 0)
	{
		const char *fn;

		close(0);
		if (open("/dev/null", O_RDONLY) != 0)
			clog_msg_errno();

		close(pipefd[0]);
		close(1);
		if (dup(pipefd[1]) != 1)
		{
			perror("dup");
			exit(1);
		}
		close(2);
		if (dup(pipefd[1]) != 2)
		{
			fprintf(stderr, "ERROR: dup failed\n");
			exit(1);
		}
		close(pipefd[1]);

		libmail_changeuidgid(MAILUID, MAILGID);

		fn=qmsgsdatname(p->inum);

		if (faxconvert(fn, faxopts, &n_cover_pages))
			exit (1);

		if ((fp=fopen(FAXTMPDIR "/.ncoverpages", "w")) == NULL ||
		    fprintf(fp, "%d\n", n_cover_pages) < 0 ||
		    fflush(fp) < 0)
			exit(1);
		fclose(fp);
		exit(0);
	}

	close(pipefd[1]);

	err_args.ctf= &ctf;
	err_args.nreceip=nrecipient;

	if (read_childerrmsg(child_proc, pipefd[0],
			     &faxconvert_cleanup, &err_args))
	{
		ctlfile_close(&ctf);
		return;
	}

	/* Hit it */

	if ((fp=fopen(FAXTMPDIR "/.ncoverpages", "r")) == NULL ||
	    fscanf(fp, "%d", &n_cover_pages) != 1)
	{
		if (fp)
			fclose(fp);

		ctlfile_append_reply(err_args.ctf,
				     err_args.nreceip,
				     "Internal error: cannot read number of cover pages",
				     COMCTLFILE_DELFAIL, 0);
		ctlfile_close(&ctf);
		exit(0);
	}

	auto page_list=read_dir_sort_filenames(FAXTMPDIR, FAXTMPDIR "/");

	/* Keep trying until the modem line is unlocked */

	do
	{
		if (pipe(pipefd) < 0)
		{
			clog_msg_errno();
		}

		child_proc=fork();
		if (child_proc < 0)
		{
			clog_msg_errno();
			return;
		}

		if (child_proc == 0)
		{
			unsigned page_cnt=0;

			close(pipefd[0]);
			close(0);
			if (open("/dev/null", O_RDONLY) != 0)
			{
				perror("/dev/null");
				exit(1);
			}
			close(1);
			if (dup(pipefd[1]) != 1)
			{
				perror("dup");
				exit(1);
			}
			close(2);
			if (dup(pipefd[1]) != 2)
			{
				fprintf(stderr, "ERROR: dup failed\n");
				exit(1);
			}
			close(pipefd[1]);

			page_cnt += page_list.size();

			std::vector<char *> argvec;

			argvec.reserve(page_cnt*10);

			static char sendfax_str[]=SENDFAX;
			static char v_opt[]="-v";
			static char r_opt[]="-r";
			argvec.push_back(sendfax_str);
			argvec.push_back(v_opt);
			argvec.push_back(r_opt);

			static char n_str[]="-n";
			if (faxopts & FAX_LOWRES)
				argvec.push_back(n_str);

			argvec.push_back(const_cast<char *>(receipient));

			for (auto &filename:page_list)
				argvec.push_back(filename.data());
			argvec.push_back(nullptr);
			invoke_sendfax(argvec.data());
		}
		close(pipefd[1]);

		err_args.ctf= &ctf;
		err_args.nreceip=nrecipient;
		err_args.file_list=&page_list;
		err_args.is_locked=0;
		err_args.n_cover_pages=n_cover_pages;

		if (read_childerrmsg(child_proc, pipefd[0],
				     &faxsend_cleanup, &err_args) == 0)
		{
			size_t npages=0;
			char fmtbuf[NUMBUFSIZE];
			char fmtbuf1[NUMBUFSIZE];
			char fmtbuf2[NUMBUFSIZE*2+100];

			npages += page_list.size();

			libmail_str_size_t(npages, fmtbuf);
			libmail_str_size_t(n_cover_pages, fmtbuf1);

			sprintf(fmtbuf2,
				"%s pages - including %s cover page(s)"
				" - sent by fax.", fmtbuf, fmtbuf1);

			ctlfile_append_reply(&ctf, (unsigned)
					     nrecipient,
					     fmtbuf2,
					     COMCTLFILE_DELSUCCESS, " l");
			break;
		}
	} while (err_args.is_locked);

	ctlfile_close(&ctf);
	rmdir_contents(FAXTMPDIR);
}

static int faxconvert_cleanup(int dummy, char *errmsg, void *vp)
{
	struct faxconv_err_args *args=(struct faxconv_err_args *)vp;

	ctlfile_append_reply(args->ctf, args->nreceip,
			     errmsg,
			     COMCTLFILE_DELFAIL_NOTRACK, 0);
	return (-1);
}

extern int faxconvert(const char *, int, unsigned *);

static int faxsend_cleanup(int errcode, char *errmsg, void *vp)
{
	struct faxconv_err_args *args=(struct faxconv_err_args *)vp;
	unsigned pages_sent=0;
	char *p, *q;

	unsigned coverpage_cnt=0;
	unsigned page_cnt=0;

	/* Check how many files sendfax renamed (were succesfully sent) */

	for (auto &file:*args->file_list)
	{
		if (access(file.c_str(), 0) == 0)
			break;

		if (coverpage_cnt < args->n_cover_pages)
			++coverpage_cnt;
		else
			++pages_sent;
	}

	/* Strip out any blank lines in captured output from sendfax */

	for (p=q=errmsg; *p; p++)
	{
		if (*p == '\n' && (p[1] == '\n' || p[1] == 0))
			continue;

		*q++=*p;
	}
	*q=0;

	/* Find the last message from sendfax */

	for (p=q=errmsg; *p; p++)
	{
		if (*p != '\n')
			continue;

		*p=0;

		/* Dump sendfax's output to the log */

		if (*q)
		{
			clog_msg_start_info();
			clog_msg_str("courierfax: " SENDFAX ": ");
			clog_msg_str(q);
			clog_msg_send();
		}
		q=p+1;
	}

	if (*q)	/* Last line of the error message */
	{
		clog_msg_start_info();
		clog_msg_str("courierfax: " SENDFAX ": ");
		clog_msg_str(q);
		clog_msg_send();
	}
	else	/* Default message */
	{
		static char completed_str[]=SENDFAX ": completed.";
		q=completed_str;
	}

	/*
	** Ugly hack: capture the following message from sendfax:
	**
	** /usr/sbin/sendfax: cannot access fax device(s) (locked?)
	*/

#if 0
	lockflag=0;
	p=strchr(q, ':');
	if (p)
	{
		static const char msg1[]="cannot access fax device";
		static const char msg2[]="locked";

		++p;
		while (*p && isspace((int)(unsigned char)*p))
			p++;

		if (*p && strncasecmp(p, msg1, sizeof(msg1)-1) == 0)
		{
			p += sizeof(msg1);
			while (*p && !isspace((int)(unsigned char)*p))
				++p;

			p=strchr(p, '(');

			if (p && strncmp(p+1, msg2, sizeof(msg2)-1) == 0)
			{
				args->is_locked=1;
				clog_msg_start_info();
				clog_msg_str("courierfax: detected locked"
					     " modem line, sleeping...");
				clog_msg_send();
				sleep(60);
				return (-1);
			}
		}
	}
#else

	if (errcode == 2)
	{
		args->is_locked=1;
		clog_msg_start_info();
		clog_msg_str("courierfax: detected locked"
			     " modem line, sleeping...");
		clog_msg_send();
		sleep(60);
		return (-1);
	}
#endif

	ctlfile_append_connectioninfo(args->ctf, args->nreceip,
				      COMCTLFILE_DELINFO_REPLY, q);

	sprintf(errmsg, "%u cover pages, %u document pages sent.",
		coverpage_cnt, page_cnt);

	courierfax_failed(pages_sent, args->ctf, args->nreceip,
			  errcode, errmsg);
	return (-1);
}
