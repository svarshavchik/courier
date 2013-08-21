/*
** Copyright 1998 - 2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"config.h"
#include	"cmlm.h"
#include	"cmlmsublist.h"
#include	"dbobj.h"
#include	"rfc822/rfc822.h"
#include	"numlib/numlib.h"

#include <sstream>

#include	<ctype.h>
#include	<time.h>
#include	<string.h>
#include	<iostream>
#include	<fstream>
#include	<list>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sysexits.h>
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif
#ifndef WIFEXITED
#define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif

/* I also want to encode /s as well, sometimes */

#define	VERPSPECIAL	"@:%!+-[]/"

static const char xdigit[]="0123456789ABCDEFabcdef";

std::string toverp(std::string buf)
{
	std::string::iterator b=buf.begin(), e=buf.end();
	size_t i;

	while (b != e)
	{
		if (*--e == '@')
		{
			*e='=';
			break;
		}
	}

	for (i=0; i<buf.size(); ++i)
	{
		if (strchr(VERPSPECIAL, buf[i]) == 0)	continue;
		buf=buf.substr(0, i)+ '+' + xdigit[ (buf[i] >> 4) & 15] +
			xdigit[buf[i] & 15] + buf.substr(i+1);
		i += 2;
	}
	return (buf);
}

std::string fromverp(std::string buf)
{
	std::vector<char> obuf;
	std::string::iterator b=buf.begin(), e=buf.end();
	const char *r;

	obuf.reserve(buf.size());

	while (b != e)
	{
		if (*b != '+' || e-b < 3) 
		{
			obuf.push_back(*b++);
			continue;
		}

		++b;

		int h=0, l=0;

		r=strchr(xdigit, *b++);
		if (r)	h= r-xdigit;
		r=strchr(xdigit, *b++);
		if (r)	l= r-xdigit;

		if (h >= 16) h -= 6;
		if (l >= 16) l -= 6;

		obuf.push_back((char)(h*16+l));
	}

	buf=std::string(obuf.begin(), obuf.end());

	b=buf.begin();
	e=buf.end();

	while (b != e)
	{
		if (*--e == '=')
		{
			*e='@';
			break;
		}
	}

	return buf;
}

std::string mkfilename()
{
	char buf[256];
	char *p;

	buf[sizeof(buf)-1]=0;

	if (gethostname(buf, sizeof(buf)-1))
		strcpy(buf, "courier-mlm");

	// Drop any 8-bit characters from the hostname, to avoid creating
	// illegal mail headers.

	for (p=buf; *p; p++)
		*p &= 127;

	time_t	t;
	time(&t);

	std::ostringstream o;

	o << t << "." << getpid() << "." << buf;

	return o.str();
}

std::string mktmpfilename()
{
	std::string f;
	unsigned	n=0;
	struct	stat	stat_buf;

	std::string p;

	do
	{
		if (n)	sleep(n);
		n=3;
		f=mkfilename();

		p= TMP "/" + f;

	} while ( stat(p.c_str(), &stat_buf) == 0);
	return (f);
}


/*
** Check whether an address can post to this list (whether it's listed as
** either a subscriber, or an alias.
*/
static int is_subscriber_1(std::string, std::string);

int is_subscriber(std::string from)
{
	int	rc;

	if (from.size() == 0) return (EX_NOUSER);

	rc=is_subscriber_1(".", from);
	if (rc && rc != EX_NOUSER)
		return (rc);
	if (rc)
	{
		std::string	digestdir=cmdget_s("DIGEST");

		if (digestdir.size() > 0)
			rc=is_subscriber_1(digestdir, from);
	}
	return (rc);
}

static int getinfodir(std::string, std::string, int (*)(std::string));

int isfound(std::string dummy)
{
	return 0;
}

static int is_subscriber_1(std::string dir, std::string from)
{
int	rc;

	rc=getinfodir(dir, from, isfound);

	if (rc == 0)	return (0);

	/* Not on subscription rolls, maybe an alias */

	std::string shared_lock_name=dir;

	shared_lock_name += "/";
	shared_lock_name += SUBLOCKFILE;

	SharedLock alias_lock(shared_lock_name.c_str());
	DbObj	alias;

	std::string	alias_name=dir;

	alias_name += "/" ALIASES;

	if (alias.Open(alias_name, "R") == 0)
	{
		std::string key;

		key="1";
		key += from;

		addrlower(key);

		if (alias.Fetch(key, "").size() != 0)
		{
			rc=0;
		}
	}
	return (rc);
}

int getinfo(std::string a, int (*func)(std::string))
{
	return (getinfodir(".", a, func));
}

int getinfodir(std::string dir, std::string address, int (*func)(std::string))
{
	struct	stat	stat_buf;

	std::string::iterator b=address.begin(), e=address.end(),
		p=std::find(b, e, '@');

	if (p == e || std::find(++p, e, '@') != e ||
	    std::find(p, e, '.') == e ||
	    std::find(p, e, '/') != e)
	{
		std::cerr << "Invalid address." << std::endl;
		return (1);
	}

	addrlower(address);

	std::string shared_lock_name=dir;

	shared_lock_name += "/";
	shared_lock_name += SUBLOCKFILE;

	SharedLock subscription_lock(shared_lock_name.c_str());

	std::string	buf;

	buf=dir;

	buf += "/" DOMAINS "/";
	buf += std::string(p, e);

	DbObj	domain_file;

	if (stat(buf.c_str(), &stat_buf) == 0 &&
	    domain_file.Open(buf, "W") == 0)
	{
		std::string val;

		if ((val=domain_file.Fetch(address, "")).size() > 0)
		{
			int	rc= (*func)(val);

			domain_file.Close();
			return (rc);
		}

		domain_file.Close();
		return (EX_NOUSER);
	}

	buf=dir;
	buf += "/" MISC;
	if (domain_file.Open(buf, "W"))
	{
		return (EX_NOUSER);
	}

	std::string r;

	if ((r=domain_file.Fetch(std::string(p, e), "")).size() == 0)
	{
		domain_file.Close();
		return (EX_NOUSER);
	}

	std::list<std::string> misclist;

	SubscriberList::splitmisc(r, misclist);

	while (!misclist.empty())
	{
		if (misclist.front() == address)
		{
			misclist.pop_front();

			std::string v;

			if (!misclist.empty())
				v=misclist.front();

			return (*func)(v);
		}

		misclist.pop_front();

		if (!misclist.empty())
			misclist.pop_front();
	}

	domain_file.Close();
	return (EX_NOUSER);
}

std::string myname()
{
	std::string	myname_buf;

	myname_buf = cmdget_s("NAME");
	if (myname_buf.size() == 0)
		myname_buf = "Courier Mailing List Manager";

	return myname_buf;
}


// Run sendmail in the background

int sendmail(const char **argv, pid_t &p)
{
int	pipefd[2];

	while (pipe(pipefd) == -1)
	{
		perror("pipe");
		sleep(5);
	}

	while ((p=fork()) == -1)
	{
		perror("fork");
		sleep(5);
	}

	if (p == 0)
	{
	char	*fakeenv=0;

		dup2(pipefd[0], 0);
		close(pipefd[0]);
		close(pipefd[1]);

		execve(SENDMAIL, (char **)argv, &fakeenv);
		perror("exec");
		exit(EX_OSERR);
	}
	close(pipefd[0]);
	return (pipefd[1]);
}

int wait4sendmail(pid_t p)
{
int	waitstat;
pid_t	p2;

	while ((p2=wait(&waitstat)) != p)
		;
	if (WIFEXITED(waitstat))
		return (WEXITSTATUS(waitstat));
	return (EX_TEMPFAIL);
}

//
// Run sendmail with the -bcc flag, requesting only FAIL notices.
// If 'nodsn' is set, request no delivery status notices at all.
//

int sendmail_bcc(pid_t &p, std::string from, int nodsn)
{
const char	*argvec[7];

	argvec[0]="sendmail";
	argvec[1]="-bcc";
	argvec[2]="-f";
	argvec[3]=from.c_str();
	argvec[4]="-N";
	argvec[5]=nodsn?"never":"fail";
	argvec[6]=0;
	return (sendmail(argvec, p));
}

////////////////////////////////////////////////////////////////////////////
//
// Read message, up to 16K bytes of it.
//

std::string readmsg()
{
	char	msgbuf[16384];
	size_t	i, j;

	for (i=0; i<sizeof(msgbuf); )
	{
		std::cin.read(msgbuf+i, sizeof(msgbuf)-i);

		int	n=std::cin.gcount();

		if (n <= 0)	break;
		i += n;
	}

	for (j=0; j<i; j++)
		if (msgbuf[j] == 0)	msgbuf[j]=' ';

	return msgbuf;
}

//
// Extract a specific header.
//

std::string header_s(std::string msgbuf, const char *header)
{
	std::istringstream i(msgbuf);
	std::string line;

	while (std::getline(i, line).good())
	{
		if (line.size() == 0)
			break; // End of headers

		std::string::iterator b=line.begin(), e=line.end();

		std::string::iterator p=std::find(b, e, ':');

		std::string name(b, p);

		std::transform(name.begin(), name.end(), name.begin(),
			       std::ptr_fun(::tolower));

		if (name != header)
			continue;

		if (p != e)
			++p;

		std::string
			buf(std::find_if(p, e,
					 std::not1(std::ptr_fun(::isspace))),
			    e);

		while (std::getline(i, line).good())
		{
			b=line.begin();
			e=line.end();

			p=std::find_if(b, e,
				       std::not1(std::ptr_fun(::isspace)));

			if (b == p)
				break;

			buf=buf + " " + std::string(p, e);
		}
		return buf;
	}
	return "";
}

//
//  Addresses to subscribe/unsubscribe are derived in two ways:
//
//  * From the headers.
//
//  * Explicitly specified in the subscription address:
//                 To:  list-subscribe-luser=host@listdomain
//
//  If we find an explicit address, use that.  Otherwise, grep the headers.
//

std::string returnaddr(std::string msg, const char *explicit_address)
{
	std::string addr;
	int n;

	if (explicit_address && *explicit_address)
	{
		addr=fromverp(explicit_address);
	}
	else
	{
		// Preference:  Reply-To:, then From:, then envelope Sender.

		addr=header_s(msg, "reply-to");

		if (addr == "")	addr=header_s(msg, "from");
		if (addr == "")
		{
			const char *p=getenv("SENDER");

			if (!p)	return (addr);
			addr=p;
		}
		else
		{
//
//  Grabbed the header, must now parse an address out of it.
//
			struct rfc822t *t=rfc822t_alloc_new(addr.c_str(),
							    NULL, NULL);

			if (!t)
			{
				perror("malloc");
				exit(EX_OSERR);
			}

		struct rfc822a *a=rfc822a_alloc(t);

			if (!a)
			{
				perror("malloc");
				exit(EX_OSERR);
			}

			if (a->naddrs <= 0)
			{
				addr="";
				rfc822a_free(a);
				rfc822t_free(t);
				return (addr);
			}

		char	*s=rfc822_getaddr(a, 0);

			rfc822a_free(a);
			rfc822t_free(t);

			if (!s)
			{
				perror("malloc");
				exit(EX_OSERR);
			}

			addr=s;
			free(s);
		}
	}

	// Sanity check: all addresses must have an @

	n=0;

	std::string::iterator b=addr.begin(), e=addr.end();

	while (b != e)
		if (*b++ == '@')
			++n;

	if (n != 1)
		addr="";

	// One more sanity check: domain.

	std::string::iterator p=std::find(addr.begin(), e, '@');

	if (std::find(p, e, '/') != e || std::find(p, e, '.') == e)
		addr="";

	return (addr);
}
