/*
** Copyright 2000-2009 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"config.h"
#include	"cmlm.h"
#include	"cmlmarchive.h"
#include	"afx/afx.h"
#include	"numlib/numlib.h"
#include        "rfc822/rfc822.h"
#include	"rfc822/rfc2047.h"
#include	<courier-unicode.h>
#include	<iostream>
#include	<sstream>
#include	<set>
#include	<list>
#include	<vector>
#include	<iomanip>
#include	<fstream>
#include	<sysexits.h>
#include	<ctype.h>
#include	<stdio.h>
#include	<string.h>
#include	<fcntl.h>

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


////////////////////////////////////////////////////////////////////////////
//
//   Generate an archive index
//
////////////////////////////////////////////////////////////////////////////

struct idxinfo {
	std::string	msgdate;
	std::string	msgsubj;
	std::string	msgsender;
	} ;

static void idxfirstlast(std::string &buf, unsigned long f, unsigned long l)
{
	char	fbuf[NUMBUFSIZE], lbuf[NUMBUFSIZE];
	size_t	i;

	libmail_str_off_t(f, fbuf);
	libmail_str_off_t(l, lbuf);

	for (i=0; i+3<buf.size(); i++)
	{
		if (buf[i] != '@' || buf[i+2] != '@')
			continue;
		switch (buf[i+1])	{
		case 'F':
			buf=buf.substr(0, i)+fbuf+buf.substr(i+3);
			break;
		case 'L':
			buf=buf.substr(0, i)+lbuf+buf.substr(i+3);
			break;
		}
	}
}

static void getmsginfo(unsigned long n, struct idxinfo &info)
{
	info.msgdate="";
	info.msgsubj="";
	info.msgsender="";

	std::string filename(Archive::filename(n));

	int msg_fd=open(filename.c_str(), O_RDONLY);

	if (msg_fd < 0)
		return;

	afxipipestream	msg(msg_fd);
	struct	stat	stat_buf;

	if (fstat(msg_fd, &stat_buf) == 0)
	{
	char	buf[200];
	struct	tm	*tmptr=localtime(&stat_buf.st_mtime);

		strftime(buf, sizeof(buf), "%d-%b-%Y", tmptr);
		info.msgdate=buf;
	}

	std::string headers, line;

	while (std::getline(msg, line).good())
	{
		headers += line;
		headers += '\n';
		if (headers.size() == 0)	break;
	}
	msg.close();
	info.msgsubj=header_s(headers, "subject");
	info.msgsender=header_s(headers, "from");

	{
		char *p=rfc822_display_hdrvalue_tobuf("subject",
						      info.msgsubj.c_str(),
						      "utf-8",
						      NULL, NULL);

		if (p)
		{
			info.msgsubj=p;
			free(p);
		}
	}

	{
		char *p=rfc822_display_hdrvalue_tobuf("from",
						      info.msgsender.c_str(),
						      "utf-8",
						      NULL, NULL);
		if (p)
		{
			info.msgsender=p;
			free(p);
		}
	}

	TrimLeft(info.msgsubj);
	TrimRight(info.msgsubj);
	TrimLeft(info.msgsender);
	TrimRight(info.msgsubj);
}

static void outhtml(std::ostream &o, const char *t)
{
	for ( ; *t; t++)
	{
		switch (*t){
		case ' ':
			o << "&nbsp;";
			break;
		case '&':
			o << "&amp;";
			break;
		case '<':
			o << "&lt;";
			break;
		case '>':
			o << "&gt;";
			break;
		default:
			o << (char)*t;
		}
	}
}

static int checksub(std::string msg)
{
	std::string from=header_s(msg, "from");

	struct rfc822t *t=rfc822t_alloc_new(from.c_str(), 0, 0);

	if (!t)
	{
		perror("malloc");
		return (EX_TEMPFAIL);
	}

struct rfc822a *a=rfc822a_alloc(t);

	if (!a)
	{
		rfc822t_free(t);
		perror("malloc");
		return (EX_TEMPFAIL);
	}

char *nn=0;

	if (a->naddrs > 0)
	{
		nn=rfc822_getaddr(a, 0);
		if (!nn)
		{
			rfc822a_free(a);
			rfc822t_free(t);
			perror("malloc");
			return (EX_TEMPFAIL);
		}
	}
	rfc822a_free(a);
	rfc822t_free(t);
	if (nn)
	{
		from=nn;
		free(nn);
	}
	else	from="";

	int	rc=is_subscriber(from.c_str());

	if (rc == EX_NOUSER)
	{
		std::string postoptions= cmdget_s("POSTARCHIVE");

		if (postoptions.size() == 0 || postoptions == "all")
			rc=0;
	}

	if (rc)
	{
		if (rc == EX_NOUSER)
		{
			std::cout << "You are not subscribed to this mailing list."
				<< std::endl;
		}
		rc=EX_NOPERM;
	}
	return (rc);
}

int doindex(const char *n)
{
	unsigned long nn=n ? atol(n):0;
	std::string	msg(readmsg());
	unsigned long first=0, last=0;
	unsigned long msgs[20];
	struct	idxinfo msginfo[sizeof(msgs)/sizeof(msgs[0])];
	int	nmsgs=0;
	int	i, j;

	static const char nlnl[]="\n\n";

	std::string::iterator b=msg.begin(), e=msg.end();
	msg=std::string(b, std::search(b, e, nlnl, nlnl+2)) + "\n";

	int	rc=checksub(msg);

	if (rc)	return (rc);

	{
	ArchiveList	list;
	unsigned long x;

		while (list.Next(x) == 0)
		{
			if (!x)	continue;
			if (!first || x < first)	first=x;
			if (!last || x > last)		last=x;
			if ( nn && x > nn)	continue;
			if (nmsgs == sizeof(msgs)/sizeof(msgs[0]))
			{
				if (x < msgs[0])	continue;

				for (i=1; i<nmsgs; i++)
					msgs[i-1]=msgs[i];
				--nmsgs;
			}
			for (i=nmsgs; i; )
			{
				--i;
				if (msgs[i] < x)
				{
					++i;
					break;
				}
			}
			for (j=nmsgs; j>i; )
			{
				msgs[j]=msgs[j-1];
				--j;
			}
			msgs[i]=x;
			++nmsgs;
		}
	}

	for (i=0; i<nmsgs; i++)
		getmsginfo(msgs[i], msginfo[i]);

	std::string addr(returnaddr(msg, ""));

	std::string owner=get_verp_return("owner");
	pid_t	p;
	afxopipestream ack(sendmail_bcc(p, owner));

	ack << "From: " << myname() << " <" << owner << ">" << std::endl
		<< "To: " << addr << std::endl
		<< "Bcc: " << addr << std::endl
		<< "Mime-Version: 1.0" << std::endl
		<< "Content-Type: multipart/mixed; boundary=courier1mlm" << std::endl
		<< "Content-Transfer-Encoding: 8bit" << std::endl;

	{
		int i=open("idxsubject.tmpl", O_RDONLY);

		if (i >= 0)
		{
			afxipipestream ifs(i);
			copyio_noseek(ifs, ack);
		}
	}

	ack << std::endl << "This is a MIME message" << std::endl << std::endl
		<< "--courier1mlm" << std::endl
		<< "Content-Type: multipart/alternative; boundary=courier2mlm"
				<< std::endl << std::endl
		<< "This is a MIME message" << std::endl << std::endl
		<< "--courier2mlm" << std::endl;


	{
		std::ifstream ifs("idxheadertxt.tmpl");

		std::ostringstream os;

		os << ifs.rdbuf();

		std::string buf=os.str();

		idxfirstlast(buf, first, last);
		ack << buf;
		for (i=0; i<nmsgs; i++)
		{
			ack << std::setiosflags(std::ios::right)
			    << std::setw(10)
			    << msgs[i]
			    << std::resetiosflags(std::ios::right) << " "
			    << std::setw(0) << msginfo[i].msgsubj << std::endl;
			ack << std::setiosflags(std::ios::right)
			    << std::setw(10)
			    << msginfo[i].msgdate
			    << std::resetiosflags(std::ios::right) << " "
			    << std::setw(0) << msginfo[i].msgsender
			    << std::endl << std::endl;
		}
	}

	ack << std::endl << "--courier2mlm" << std::endl;

	{
		std::ifstream ifs("idxheaderhtml.tmpl");

		std::ostringstream os;

		os << ifs.rdbuf();

		std::string buf=os.str();

		idxfirstlast(buf, first, last);
		ack << buf;

		for (i=0; i<nmsgs; i++)
		{
		char	buf[NUMBUFSIZE];
		char	buf2[10+NUMBUFSIZE];

			strcat(strcpy(buf2, "fetch-"),
				libmail_str_off_t(msgs[i], buf));

			ack << "<tr valign=top><td align=right>"
				"<a href=\"mailto:"
			    << get_verp_return(buf2)
			    << "\">" << msgs[i]
			    << "</a><br />";
			outhtml(ack, msginfo[i].msgdate.c_str());
			ack << "</td><td>";
			outhtml(ack, msginfo[i].msgsubj.c_str());
			ack << "<br />";
			outhtml(ack, msginfo[i].msgsender.c_str());
			ack << "</td></tr>" << std::endl;
		}
	}

	{
		int i=open("idxheader2html.tmpl", O_RDONLY);

		if (i >= 0)
		{
			afxipipestream ifs(i);
			copyio_noseek(ifs, ack);
		}
	}

	ack << std::endl << "--courier2mlm--" << std::endl
		<< std::endl << "--courier1mlm" << std::endl
		<< "Content-Type: text/rfc822-headers"
		<< std::endl << std::endl
		<< msg << std::endl << "--courier1mlm--" << std::endl;
	ack.close();
	return (wait4sendmail(p));
}

////////////////////////////////////////////////////////////////////////////
//
//   Fetch messages from the archive
//
////////////////////////////////////////////////////////////////////////////

static const char *getn(const char *p, unsigned long &un)
{
	un=0;
	if (!isdigit((int)(unsigned char)*p))	return (0);

	while (isdigit((int)(unsigned char)*p))
	{
		un=un * 10 + (*p++ - '0');
	}
	return (p);
}

static int mkfetchlist(const char *fetchlist,
		       std::list<std::string> &filenames)
{
	std::string buf;

	std::set<unsigned long> msgset;

	while (*fetchlist)
	{
	unsigned long	from, to;

		if ((fetchlist=getn(fetchlist, from)) == 0)
		{
			std::cerr << "Invalid message number request." << std::endl;
			return (EX_SOFTWARE);
		}

		if (*fetchlist == '-')
		{
			++fetchlist;
			if ((fetchlist=getn(fetchlist, to)) == 0)
			{
				std::cerr << "Invalid message number request."
					  << std::endl;
				return (EX_SOFTWARE);
			}
		}
		else	to=from;
		if (*fetchlist == '+')	++fetchlist;

		{
		ArchiveList	list;
		unsigned long x;

			while (list.Next(x) == 0)
			{

				if (x < from || x > to)	continue;

				msgset.insert(x);
			}
		}
	}


	std::vector<unsigned long> msgVec;

	msgVec.reserve(msgset.size());

	msgVec.insert(msgVec.end(), msgset.begin(), msgset.end());

	std::sort(msgVec.begin(), msgVec.end());

	std::vector<unsigned long>::iterator b=msgVec.begin(), e=msgVec.end();

	while (b != e)
	{
		std::string buf=Archive::filename(*b++);

		filenames.push_back(buf);
	}

	return (0);
}

static int mkdigest(std::list<std::string> &msglist, int tmpfile_fd,
		    afxopipestream &tfile)
{
	int	pipefd1[2], pipefd0[2];
	pid_t	p;
	std::string	buf;

	if (pipe(pipefd1))
	{
		perror("pipe");
		return (EX_TEMPFAIL);
	}

	if (pipe(pipefd0))
	{
		perror("pipe");
		close(pipefd1[0]);
		close(pipefd1[1]);
		return (EX_TEMPFAIL);
	}

	if ((p=fork()) == -1)
	{
		perror("fork");
		close(pipefd1[0]);
		close(pipefd1[1]);
		close(pipefd0[0]);
		close(pipefd0[1]);
		return (EX_TEMPFAIL);
	}

	if (p == 0)
	{
		dup2(pipefd0[0], 0);
		dup2(pipefd1[1], 1);
		close(pipefd1[0]);
		close(pipefd1[1]);
		close(pipefd0[0]);
		close(pipefd0[1]);
		close(tmpfile_fd);
		execl(REFORMIME, "reformime", "-m", (char *)0);
		perror(REFORMIME);
		_exit(EX_TEMPFAIL);
	}
	close(pipefd0[0]);
	close(pipefd1[1]);

	{
		afxopipestream ofs(pipefd0[1]);

		while (!msglist.empty())
		{
			buf=msglist.front();
			msglist.pop_front();
			ofs << buf << std::endl;
		}
	}
	close(pipefd0[1]);

	afxipipestream	ifs(pipefd1[0]);

	buf=cmdget_s("MAXFETCHSIZE");

	unsigned long	nbytes=atol(buf.c_str());

	if (nbytes == 0)	nbytes=100;
	nbytes *= 1024;

int	rc=copyio_noseek_cnt(ifs, tfile, &nbytes);

	ifs.close();
	close(pipefd1[0]);

int	waitstat;

	while ( wait(&waitstat) != p)
		;

	if (rc == 0)
	{
		rc=EX_SOFTWARE;
		if (WIFEXITED(waitstat))
			rc=WEXITSTATUS(waitstat);
	}
	return (rc);
}

int dofetch(const char *p)
{
	std::string msg(readmsg());

	std::list<std::string> msglist;

	static const char nlnl[]="\n\n";

	std::string::iterator b=msg.begin(), e=msg.end();
	msg=std::string(b, std::search(b, e, nlnl, nlnl+2)) + "\n";

	int	rc=checksub(msg);

	if (rc)	return (rc);

	rc=mkfetchlist(p, msglist);

	if (rc)	return (rc);

	std::string tmpfile(TMP "/");

	tmpfile += mktmpfilename();

	trapsigs(tmpfile.c_str());

	int tmp_fd=open(tmpfile.c_str(), O_RDWR|O_CREAT|O_TRUNC, 0666);

	if (tmp_fd < 0)
	{
		clearsigs(-1);
		return (-1);
	}

	afxopipestream	tmp_pipe(tmp_fd);

	rc=mkdigest(msglist, tmp_fd, tmp_pipe);

	tmp_pipe << std::flush;

	if (rc || tmp_pipe.bad())
	{
		rc= -1;
		tmp_pipe.close();
		clearsigs(rc);
		return (rc);
	}

	tmp_pipe.close();

	tmp_fd=open(tmpfile.c_str(), O_RDONLY, 0666);

	if (tmp_fd < 0)
	{
		perror(tmpfile.c_str());
		clearsigs(-1);
		return (-1);
	}

	afxipipestream tmp(tmp_fd);

	std::string addr;

	addr=header_s(msg, "reply-to");
	if (addr == "")	addr=header_s(msg, "from");

	std::string	boundary=mkboundary_msg_s(tmp);
	std::string	owner=get_verp_return("owner");
	pid_t	pid;
	afxopipestream ack(sendmail_bcc(pid, owner));

	ack << "From: " << myname() << " <" << owner << ">" << std::endl
		<< "To: " << addr << std::endl
		<< "Bcc: " << addr << std::endl
		<< "Mime-Version: 1.0" << std::endl
		<< "Content-Type: multipart/mixed; boundary=\""
			<< boundary << "\"" << std::endl
		<< "Content-Transfer-Encoding: 8bit" << std::endl;

	{
		int i_fd=open("fetchsubj.tmpl", O_RDONLY);

		if (i_fd >= 0)
		{
			afxipipestream ifs(i_fd);

			copyio_noseek(ifs, ack);
		}
	}


	ack << std::endl << "This is a MIME message" << std::endl
		<< std::endl << "--" << boundary << std::endl;

	{
		int i_fd=open("fetch.tmpl", O_RDONLY);

		if (i_fd >= 0)
		{
			afxipipestream ifs(i_fd);

			copyio_noseek(ifs, ack);
		}
	}

	ack << std::endl << "--" << boundary << std::endl
		<< "Content-Type: text/rfc822-headers" << std::endl << std::endl
		<< msg
		<< std::endl << "--" << boundary << std::endl;

	tmp.seekg(0);
	copyio_noseek(tmp, ack);
	ack << std::endl << "--" << boundary << "--" << std::endl;
	ack.close();
	tmp.close();
	unlink(tmpfile.c_str());
	clearsigs(0);
	return (wait4sendmail(pid));
}
