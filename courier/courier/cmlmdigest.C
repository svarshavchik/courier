/*
** Copyright 2000-2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"config.h"
#include	"cmlm.h"
#include	"dbobj.h"
#include	"afx/afx.h"
#include	"numlib/numlib.h"
#include	"cmlmarchive.h"

#include	<stdio.h>
#include	<string.h>
#include	<ctype.h>
#include	<fcntl.h>
#include	<sysexits.h>
#include	<iostream>
#include	<fstream>
#include	<list>

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


// --------------------------------------------------------------------------
//
// Send a digest.
//
// --------------------------------------------------------------------------

static int senddigest(std::list<std::string> &list)
{
	unsigned long nextseqno;
	char	buf[NUMBUFSIZE+100];
	char	buf2[NUMBUFSIZE+100];
	int	rc;
	Archive	archive;
	std::string filename_str;

	if ( (rc=archive.get_seq_no(nextseqno)) != 0)
		return (rc);

	libmail_str_off_t(nextseqno, buf);

	filename_str=archive.filename(nextseqno);

	std::string subj;

	{
		std::ifstream ifs("digestsubj.tmpl");

		if (!std::getline(ifs, subj).good())
		{
			perror("digestsubj.tmpl");
			return (EX_SOFTWARE);
		}

		size_t	n=subj.find('@');

		if (n != subj.npos)
			subj=subj.substr(0, n) + buf + subj.substr(n+1);
	}

int	pipefd[2];

	if (pipe(pipefd) < 0)
	{
		perror("pipe");
		return (EX_TEMPFAIL);
	}

	trapsigs(filename_str.c_str());

	int f_fd=open(filename_str.c_str(), O_RDWR|O_CREAT|O_TRUNC, 0666);

	if (f_fd < 0)
	{
		perror(filename_str.c_str());
		return (EX_TEMPFAIL);
	}

	afxopipestream	f(f_fd);

	{
		int h_fd=open(HEADERADD, O_RDONLY);

		if (h_fd >= 0)
		{
			afxipipestream ifs(h_fd);
			copyio_noseek(ifs, f);
		}
	}

	std::string	addr=cmdget_s("ADDRESS");

	f << "From: " << myname() << " <" << addr << ">" << std::endl
		<< "To: " << addr << std::endl
		<< subj << std::endl << std::flush;

	if (f.bad())
	{
		perror(filename_str.c_str());
		f.close();

		clearsigs(EX_SOFTWARE);
		return (EX_SOFTWARE);
	}

pid_t	p;
int	waitstat;

	if ((p=fork()) < 0)
	{
		perror("fork");
		f.close();
		clearsigs(EX_TEMPFAIL);
		return (EX_TEMPFAIL);
	}

	if (p == 0)
	{
		clearsigs(0);
		close(0);
		if (dup(pipefd[0]) < 0)
		{
			perror("dup");
			_exit(EX_TEMPFAIL);
		}
		close(pipefd[0]);
		close(pipefd[1]);
		close(1);
		if (dup(f_fd) < 0)
		{
			perror("dup");
			_exit(EX_TEMPFAIL);
		}
		f.close();
		execl(REFORMIME, "reformime", "-m", (char *)0);
		perror(REFORMIME);
		_exit(EX_TEMPFAIL);
	}

	close(pipefd[0]);

	// Reformime gets filename list on stdin, writes digest out to
	// stdout

	{
		afxopipestream	opipe(pipefd[1]);
		std::list<std::string>::iterator b, e;

		b=list.begin();
		e=list.end();

		while (b != e)
			opipe << *b++ << std::endl;

		opipe.flush();
		if (opipe.bad())
		{
			clearsigs(EX_TEMPFAIL);
			return (EX_TEMPFAIL);
		}
	}
	close(pipefd[1]);

	while (wait(&waitstat) != p)
		;

	rc=EX_TEMPFAIL;
	if (WIFEXITED(waitstat))
		rc=WEXITSTATUS(waitstat);

	clearsigs(rc);
	if (rc)	return (rc);

	f.close();
	std::ifstream ff(filename_str.c_str());

	if (ff.bad())
	{
		perror(filename_str.c_str());
		return (-1);
	}

	if ( (rc=archive.save_seq_no()) != 0)
		return (rc);

	if (rename(NEXTSEQNO, SEQNO))
	{
		perror(SEQNO);
		rc=EX_TEMPFAIL;
	}

	strcpy(buf2, "bounce-");
	strcat(buf2, buf);

	post(ff, buf2);

	// Now, remove the messages that were just sent out

	{
		std::list<std::string>::iterator b, e;

		b=list.begin();
		e=list.end();

		while (b != e)
		{
			unlink (b->c_str());
			++b;
		}
	}
	return (0);
}

//
//  Build a list of messages to be sent out in digest form.
//
//  Returns 0 to go ahead and send it, non-zero if not enough msgs for the
//  digest.

static int mkdigest(std::list<std::string> &list, unsigned n, unsigned h)
{
	DIR	*dirp;
	struct	dirent *de;
	struct	stat	stat_buf;
	std::string s;
	int	rc= -1;
	time_t	age_time;
	std::string dummy;

	time (&age_time);

	age_time -= h * 60 * 60;

	dirp=opendir(MODQUEUE);
	while (dirp && (de=readdir(dirp)) != 0)
	{
		if (de->d_name[0] == '.')	continue;

	unsigned long nn= atol(de->d_name);

		s= MODQUEUE "/";
		s += de->d_name;

		if (stat(s.c_str(), &stat_buf))		continue;

		if (n == 0)	rc=0;
		else		--n;

		if (h && stat_buf.st_mtime < age_time)	rc=0;

		std::list<std::string>::iterator b, e;

		b=list.begin();
		e=list.end();

		while (b != e)
		{
			dummy= *--e;

			unsigned long oo=atol(strrchr(dummy.c_str(), '/')+1);

			if (oo < nn)
			{
				++e;
				break;
			}
		}

		list.insert(e, s);
	}
	if (dirp)	closedir(dirp);
	return (rc);
}

int cmddigest(const std::vector<std::string> &args)
{
	std::vector<std::string>::const_iterator b=args.begin(), e=args.end();

	unsigned n=0, h=0;

	if (b != e)
	{
		n=atoi(b->c_str());
		++b;
	}

	if (b != e)
	{
		h=atoi(b->c_str());
		++b;
	}

	std::list<std::string> msglist;

	ExclusiveLock	digest_lock(DIGESTLOCKFILE);

	if (mkdigest(msglist, n, h))	return (0);

	return (senddigest(msglist));
}
