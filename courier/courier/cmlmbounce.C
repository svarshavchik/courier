/*
** Copyright 2000-2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"config.h"
#include	"cmlm.h"
#include	"cmlmbounce.h"
#include	"cmlmarchive.h"
#include	"cmlmsubunsub.h"
#include	"random128/random128.h"
#include	"numlib/numlib.h"
#include	"dbobj.h"

#include	<stdio.h>
#include	<string.h>
#include	<ctype.h>
#include	<fcntl.h>
#include	<sysexits.h>
#include	<iostream>
#include	<fstream>
#include	<vector>
#include	<sstream>

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


static int handlebounce(std::string, std::string, unsigned long);
static std::string mkbouncetoken(std::string addr, std::string pfix);

//
//  Initial bounce.  Determine address(es) that are bouncing.
//

int dobounce(const char *p)
{
	unsigned long n=0;

	while (isdigit((int)(unsigned char) *p))
		n=n*10 + (*p++ - '0');

	if (*p && *p != '-')
	{
		std::cerr << "Invalid address." << std::endl;
		return (EX_SOFTWARE);
	}

	std::string t=mktmpfilename();
	std::string tname= TMP "/" + t;

	int tmpfile_fd=open(tname.c_str(), O_RDWR|O_CREAT|O_TRUNC, 0666);

	if (tmpfile_fd < 0)
	{
		perror(tname.c_str());
		return (-1);
	}

	afxopipestream tmpfile_pipe(tmpfile_fd);

	int cin_fd=dup(0);

	if (cin_fd < 0)
	{
		perror("dup");
		exit(EX_OSERR);
	}

	int rc;

	{
		afxipipestream cin_copy(cin_fd);

		rc=copyio_noseek(cin_copy, tmpfile_pipe);

		if (cin_copy.bad())
			rc= -1;
	}

	if (rc == 0)
	{
		tmpfile_pipe.close();
		if (tmpfile_pipe.bad())
			rc= -1;
	}

	std::ifstream tmpfile(tname.c_str());

	if (rc || !tmpfile.is_open())
	{
		perror(tname.c_str());
		tmpfile.close();
		unlink(tname.c_str());
		return (rc);
	}

	if (*p)
	{
		std::string addr=fromverp(p+1);

		int exitstatus=EX_SOFTWARE;

		tmpfile.close();
		if (addr.size() > 0 && addr.find('@') != addr.npos)
			exitstatus=handlebounce(tname, addr, n);
		else
			std::cerr << "Invalid address" << std::endl;

		unlink(tname.c_str());
		return (exitstatus);
	}

	// No need to reinvent the wheel.  Run reformime.

int	pipefd[2];

	if (pipe(pipefd) < 0)
	{
		perror("pipe");
		return (EX_TEMPFAIL);
	}

pid_t	pid=fork();

	if (pid < 0)
	{
		perror("fork");
		close(pipefd[0]);
		close(pipefd[1]);
		return (EX_TEMPFAIL);
	}

	if (pid == 0)
	{
		close(0);
		if (dup(tmpfile_fd) < 0)
		{
			perror("dup");
			exit(EX_TEMPFAIL);
		}
		tmpfile.close();
		dup2(pipefd[1], 1);
		close(pipefd[0]);
		close(pipefd[1]);
		execl(REFORMIME, "reformime", "-D", (char *)0);
		perror("execl");
		exit(EX_OSERR);
	}
	close(pipefd[1]);
	tmpfile.close();

unsigned	addrcnt=0;

	{
		afxipipestream reformime(pipefd[0]);

		std::string buf;

		while (std::getline(reformime, buf).good())
		{
			size_t i=buf.find(' ');

			if (i == buf.npos)	continue;
			if (strncasecmp(buf.c_str(), "f", 1))	continue;

			// failed

			buf=buf.substr(i);
			TrimLeft(buf);
			TrimRight(buf);
			if (buf.size() == 0)	continue;
			rc=handlebounce(tname, buf, n);
			if (rc)
			{
				unlink (tname.c_str());
				return (rc);
			}
			++addrcnt;
		}
	}

int	exitstatus;

	while ( wait(&exitstatus) != pid)
		;

	if (WIFEXITED(exitstatus))
		exitstatus=WEXITSTATUS(exitstatus);
	else
		exitstatus=EX_SOFTWARE;

	unlink(tname.c_str());
	return (exitstatus);
}

//
//  This is called to handle a bounce to a single address.
//

static int handlebounce(std::string tmpfilename, std::string addr,
			unsigned long n)
{
ExclusiveLock bounce_lock(BOUNCESLOCK);
DbObj	dat;
struct	stat	stat_buf;

char bufn[NUMBUFSIZE];

	libmail_str_size_t(n, bufn);
 
	addrlower(addr);
	if (addr.find('@') == addr.npos)
	{
		std::cerr << "Invalid address." << std::endl;
		return (EX_SOFTWARE);
	}

	// Validate bounce address as much as possible:
	// 1.  Must be a current subscriber address
	// 2.  Bounce must specify a message number that exists

	if (getinfo(addr, isfound))
		return (0);	// Not a subscriber (any more?)

	if (stat( Archive::filename(n).c_str(), &stat_buf))
	{
		std::cerr << "Invalid bounce address." << std::endl;
		return (EX_SOFTWARE);
	}

	// Locate the directory where bounces for this address are kept

	if (dat.Open(BOUNCESDAT, "C"))
	{
		perror(BOUNCESDAT);
		return (EX_OSERR);
	}

	std::string	bouncedir, bouncedirp;

	std::string q;

	if ((q=dat.Fetch(addr, "")) != "")
	{
		bouncedir=q;

		bouncedirp=BOUNCES "/" + q;
		if (stat(bouncedirp.c_str(), &stat_buf))	// Stale entry
			q="";
	}

	if (q == "") // First bounce for this address -- create this directory
	{
		bouncedir=mktmpfilename();
		bouncedirp=BOUNCES "/" + bouncedir;

		if ( mkdir( bouncedirp.c_str(), 0755))
		{
			perror(bouncedirp.c_str());
			return (EX_OSERR);
		}

		std::ofstream	addrfile((bouncedirp + "/.address").c_str());

		addrfile << addr << std::endl << std::flush;
		if (addrfile.fail())
		{
			perror(bouncedirp.c_str());
			return (EX_OSERR);
		}
		if (dat.Store(addr, bouncedir, "R"))
			return (EX_SOFTWARE);
	}

	if (link(tmpfilename.c_str(), (bouncedirp + "/" + bufn).c_str()) < 0)
		; /* ignore */
	return (0);
}

//
//  Return the address associated with a bounce return address.

static std::string getbounceaddr(const char *n)
{
	std::string s;
	std::string buf;
	std::string ns=n;

	if (strchr(n, '/'))	return ("");	// Script kiddies

	size_t i=ns.find('-');

	if (i == ns.npos)
		return "";

	buf=ns.substr(0, i);

	ns=ns.substr(i); // Bounce token

	std::transform(ns.begin(), ns.end(), ns.begin(),
		       std::ptr_fun(::toupper));

	ns=buf + ns;

	s= TMP "/";
	s += ns;

	std::ifstream	ifs(s.c_str());

	if (!std::getline(ifs, buf).good())
		buf="";

	ifs.close();

	if (buf.size() > 0)
		unlink(s.c_str());
	return (buf);
}

//
//  Warning message bounces, send a probe.
//

int dobounce1(const char *n)
{
	std::string addr=getbounceaddr(n);

	if (addr == "")
	{
		std::cerr << "Invalid address." << std::endl;
		return (EX_SOFTWARE);
	}

	std::string token(mkbouncetoken(addr, "bounce2-"));

	if (token == "")
		return (EX_SOFTWARE);

	std::string owner=get_verp_return(token);
	pid_t	p;
	afxopipestream ack(sendmail_bcc(p, owner));

	ack << "From: " << myname() << " <" << owner << ">" << std::endl
	    << "To: " << cmdget_s("ADDRESS") << std::endl
	    << "Bcc: " << addr << std::endl;

	copy_report("warn2msg.tmpl", ack);

	ack.close();
	return (wait4sendmail(p));
}

int dobounce2(const char *n)
{
	std::string addr=getbounceaddr(n);
	std::string message;
	std::string buf;

	if (addr == "")
	{
		std::cerr << "Invalid address." << std::endl;
		return (EX_SOFTWARE);
	}

	while (std::getline(std::cin, buf).good())
	{
		if (message.size() + buf.size() < 50000)
		{
			message += buf;
			message += '\n';
		}
	}
	return (docmdunsub_bounce(addr, message));
}

static std::string mkbouncetoken(std::string addr, std::string pfix)
{
	struct	stat	stat_buf;
	ExclusiveLock tmp_lock(TMPLOCK);

	std::string f, token;

	do
	{
		token=pfix;
		token += random128_alpha();
		f = TMP "/";
		f += token;

	} while (stat(f.c_str(), &stat_buf) == 0);

	{
		std::ofstream	ofs(f.c_str());

		ofs << addr << std::endl << std::flush;
		if (ofs.fail())
		{
			perror(f.c_str());
			token="";
		}
		ofs.close();
		if (ofs.fail())
		{
			perror(f.c_str());
			token="";
		}
	}
	return (token);
}

//
//  Generate a warning bounce message.
//

int bouncewarning(std::string addr, std::string lastbounce, std::string dir)
{
	int b_fd=open(lastbounce.c_str(), O_RDONLY);

	if (b_fd < 0)
	{
		perror(lastbounce.c_str());
		return (EX_SOFTWARE);
	}

	afxipipestream bouncetxt(b_fd);

	std::string boundary=mkboundary_msg_s(bouncetxt);

	bouncetxt.seekg(0);

	std::string token(mkbouncetoken(addr, "bounce1-"));

	if (token == "")
		return (EX_SOFTWARE);

	std::string owner=get_verp_return(token);

	pid_t	p;
	afxopipestream ack(sendmail_bcc(p, owner));

	ack << "From: " << myname() << " <" << owner << ">" << std::endl
		<< "To: " << cmdget_s("ADDRESS") << std::endl
		<< "Bcc: " << addr << std::endl
		<< "Mime-Version: 1.0" << std::endl
		<< "Content-Type: multipart/mixed; boundary=\""
			<< boundary << "\"" << std::endl
		<< "Content-Transfer-Encoding: 8bit" << std::endl;

	copy_report("warn1headers.tmpl", ack);

	ack << std::endl << "This is a MIME formatted message." << std::endl
		<< std::endl << "--" << boundary << std::endl;

	copy_report("warn1text.tmpl", ack);

	std::vector<long> bouncelist;

	DIR	*dirp;
	struct	dirent *de;

	dirp=opendir(dir.c_str());

	while (dirp && (de=readdir(dirp)) != 0)
	{
		if (de->d_name[0] == '.')	continue;

		std::istringstream i(de->d_name);

		long n=0;

		i >> n;

		if (!i.fail())
			bouncelist.push_back(n);
	}
	if (dirp)	closedir(dirp);


	std::vector<long>::iterator b=bouncelist.begin(),
		e=bouncelist.end();

	std::sort(b, e);

	while (b != e)
	{
		ack << *b++ << std::endl;
	}

	copy_report("warn1text2.tmpl", ack);

	ack << std::endl << "--" << boundary << std::endl
		<< "Content-Type: message/rfc822" << std::endl << std::endl;

	if (copyio_noseek(bouncetxt, ack))
		perror(lastbounce.c_str());
	ack << std::endl << "--" << boundary << "--" << std::endl << std::flush;
	ack.close();

	return (wait4sendmail(p));
}
