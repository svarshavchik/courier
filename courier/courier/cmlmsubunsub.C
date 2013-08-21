/*
** Copyright 2000-2008 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"config.h"
#include	"cmlm.h"
#include	"cmlmsubunsub.h"
#include	"cmlmsublist.h"
#include	"numlib/numlib.h"
#include	"dbobj.h"
#include	"sys/types.h"
#include	"sys/stat.h"
#include	<time.h>
#include	<string.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<iostream>
#include	<fstream>
#include	<list>

#include	<ctype.h>
#include	<sysexits.h>


// subscribe an address

void addrlower(char *p)
{
	if (cmdget_s("CASESENSITIVE") == "1")
		p=strchr(p, '@');

	while (p && *p)
	{
		*p=tolower((int)(unsigned char)*p);
		++p;
	}
}

void addrlower(std::string &s)
{
	std::string::iterator b=s.begin(), e=s.end();

	if (cmdget_s("CASESENSITIVE") == "1")
		b=std::find(b, e, '@');

	std::transform(b, e, b, std::ptr_fun(::tolower));
}

static int cmdsubunsub(const std::vector<std::string> &args,
		       int (*)(const char *, std::string),
		       const char *);

int cmdsub(const std::vector<std::string> &args)
{
	return (cmdsubunsub(args, docmdsub, "subscribed"));
}

int cmdunsub(const std::vector<std::string> &args)
{
	return (cmdsubunsub(args, docmdunsub, "unsubscribed"));
}

static int cmdsubunsub(const std::vector<std::string> &args,
		       int (*func)(const char *, std::string),
		       const char *msg)
{
	const char	*p;

	if (args.size() == 0 || (p=strchr(args[0].c_str(), '@')) == NULL
	    || strchr(p+1, '@'))
	{
		std::cerr << "Invalid address." << std::endl;
		return (1);
	}

	std::string	buf;
	std::string	subbuf;

	{
	time_t	t;

		time(&t);
		subbuf=ctime(&t);
		subbuf += '\n';
	}

#if	HAVE_ISATTY
	if (isatty(2))
#endif
	{
		std::cout << "Enter supporting information, EOF to terminate (usually CTRL-D):" << std::endl;
	}

	while (std::getline(std::cin, buf).good())
	{
		subbuf += buf;
		subbuf += '\n';
	}

	int rc=(*func)(args[0].c_str(), subbuf);

	if (rc == 9)
		std::cerr << args[0] << ": already " << msg << "." << std::endl;
	return (rc);
}

int docmdsub(const char *aptr, std::string subbuf)
{
	return docmdsub(aptr, subbuf, true);
}

int docmdsub(const char *aptr, std::string subbuf, bool addflag)
{
	std::string addr=(aptr ? aptr:"");
	std::string origaddr=addr;

	struct	stat	stat_buf;
	std::string	buf;
	time_t	timestamp;

	addrlower(addr);

	std::string::iterator b=addr.begin(), e=addr.end(),
		p=std::find(b, e, '@');

	if (p == e || std::find(++p, e, '@') != e ||
	    std::find(p, e, '.') == e ||
	    std::find(p, e, '/') != e)
	{
		std::cerr << "Invalid address." << std::endl;
		return (1);
	}

	// Save subscription date in a header.

	if (addflag)
	{
		char buf[NUMBUFSIZE];

		time(&timestamp);
		libmail_str_time_t(timestamp, buf);
		subbuf= std::string("x-couriermlm-date: ") + buf
			+ " " + origaddr + "\n" + subbuf;
	}
	else
	{
		if (subbuf == "")
			subbuf="\n";
	}

	SubExclusiveLock subscription_lock;

	buf=DOMAINS "/";
	buf += std::string(p, e);

	//
	// First, check for a dedicated db file
	//

	DbObj	domain_file;

	if (stat(buf.c_str(), &stat_buf) == 0 &&
	    domain_file.Open(buf, "W") == 0)
	{
		if (domain_file.Store(addr, subbuf, "I"))
		{
			domain_file.Close();
			return (9);
		}
		return (0);
	}

	//
	// No, check the misc file
	//

	DbObj	misc_file;

	if (misc_file.Open(MISC, "C"))
	{
		perror(MISC);
		return (1);
	}

	std::string r;

	std::list<std::string> misclist;
	std::list<std::string>::iterator mb, me;

	if ((r=misc_file.Fetch(std::string(p, e), "")) != "")
	{
		SubscriberList::splitmisc(r, misclist);

		mb=misclist.begin();
		me=misclist.end();

		while (mb != me)
		{
			if (*mb++ == addr)
				return (9);

			if (mb != me)
				++mb;
		}

		if (misclist.size() >= MISCSIZE*2)
			// Time to create a new domain file
		{
			buf=DOMAINS "/";
			buf += std::string(p, e);
			if (domain_file.Open(buf, "C"))
			{
				perror(buf.c_str());
				misc_file.Close();
				return (1);
			}

			misclist.push_back(addr);
			misclist.push_back(subbuf);

			mb=misclist.begin(),
			me=misclist.end();

			while (mb != me)
			{
				std::string k= *mb++;

				if (mb != me)
				{
					if (domain_file.Store(k, *mb++, "I"))
					{
						perror(buf.c_str());
						domain_file.Close();
						misc_file.Close();
						unlink(buf.c_str());
						return (1);
					}
				}
			}

			domain_file.Close();
			misc_file.Delete(std::string(p, e));
			misc_file.Close();

			std::ofstream(SUBLOGFILE, std::ios::out|std::ios::app)
				<< timestamp << " SUBSCRIBE " << addr
				<< std::endl;

			return (0);
		}
	}

	misclist.push_back(addr);
	misclist.push_back(subbuf);

	r=SubscriberList::joinmisc(misclist);

	if (misc_file.Store(std::string(p, e), r, "R"))
	{
		perror(MISC);
		misc_file.Close();
		return (EX_SOFTWARE);
	}
	misc_file.Close();

	std::ofstream(SUBLOGFILE, std::ios::out|std::ios::app)
		<< timestamp << " SUBSCRIBE " << addr << std::endl;

	return (0);
}

static void saveunsub(std::string, std::string, const char *);
static void unsubalias(std::string);

static int unsub_common(std::string, std::string, const char *);

int docmdunsub(const char *aptr, std::string reason)
{
	return (unsub_common(aptr, reason, "UNSUBSCRIBE"));
}

int docmdunsub_bounce(std::string aptr, std::string reason)
{
	return (unsub_common(aptr, reason, "BOUNCE"));
}

static int unsub_common(std::string addr, std::string reason, const char *log)
{
	struct	stat	stat_buf;

	addrlower(addr);

	std::string::iterator b=addr.begin(), e=addr.end(),
		p=std::find(b, e, '@');

	if (p == e || std::find(++p, e, '@') != e ||
	    std::find(p, e, '.') == e ||
	    std::find(p, e, '/') != e)
	{
		std::cerr << "Invalid address." << std::endl;
		return (1);
	}

	SubExclusiveLock subscription_lock;

	std::string	buf;

	buf=DOMAINS "/";
	buf += std::string(p, e);

	DbObj		domain_file;
	std::string	sub_request;

	std::list<std::string> misclist;

	if (stat(buf.c_str(), &stat_buf) == 0 &&
	    domain_file.Open(buf, "W") == 0)
	{
		std::string key, val;

		if ((val=domain_file.Fetch(addr, "")) == "")
		{
			return (9);
		}

		sub_request=val;
		sub_request += "==========================================\n";
		sub_request += reason;

		domain_file.Delete(addr);

		size_t cnt=0;

		for (key=domain_file.FetchFirstKeyVal(val); key != "";
		     key=domain_file.FetchNextKeyVal(val))
		{
			if (cnt >= MISCSIZE)
			{
				domain_file.Close();
				saveunsub(sub_request, addr, log);
				unsubalias(addr);
				return (0);
			}

			misclist.push_back(key);
			misclist.push_back(val);
			++cnt;
		}

		DbObj	misc_file;

		if (misc_file.Open(MISC, "W") ||
		    misc_file.Store(std::string(p, e),
				    SubscriberList::joinmisc(misclist), "R"))
		{
			perror(MISC);
			misc_file.Close();
			domain_file.Close();
			return (1);
		}
		misc_file.Close();
		domain_file.Close();
		unlink(buf.c_str());
		saveunsub(sub_request, addr, log);
		unsubalias(addr);
		return (0);
	}

	std::string r;

	if (domain_file.Open(MISC, "W"))
	{
		return (0);
	}

	if ((r=domain_file.Fetch(std::string(p, e), "")) == "")
		return (9);

	int	rc=9;

	misclist.clear();

	SubscriberList::splitmisc(r, misclist);

	std::list<std::string>::iterator mb, me;

	mb=misclist.begin();
	me=misclist.end();

	while (mb != me)
	{
		std::list<std::string>::iterator p;

		if (*mb == addr)
		{
			p=mb++;

			misclist.erase(p);

			if (mb != me)
			{
				rc=0;

				sub_request=*mb;

				p=mb++;
				misclist.erase(p);

				sub_request += "==========================================\n";
				sub_request += reason;
			}
			continue;
		}
		++mb;

		if (mb != me)
			++mb;
	}
	if (domain_file.Store(std::string(p, e),
			      SubscriberList::joinmisc(misclist), "R") == 0)
	{
		domain_file.Close();
		if (rc == 0)
		{
			saveunsub(sub_request, addr, log);
			unsubalias(addr);
		}
		return (rc);
	}
	perror(MISC);
	domain_file.Close();
	return (1);
}

static void saveunsub(std::string sub_request,
		      std::string addr, const char *log)
{
	std::string	f=mktmpfilename();
	std::string	tfilename= TMP "/" + f;
	std::string	ufilename= UNSUBLIST "/" + f;

	std::ofstream	ofs(tfilename.c_str());

	ofs << "Address: " << addr << std::endl << sub_request;
	ofs.flush();
	if (ofs.bad())
	{
		perror(tfilename.c_str());
		exit(EX_OSERR);
	}
	ofs.close();
	rename (tfilename.c_str(), ufilename.c_str());

	time_t	timestamp;

	{
	char buf[NUMBUFSIZE];

		time(&timestamp);
		libmail_str_time_t(timestamp, buf);
		std::ofstream(SUBLOGFILE, std::ios::out|std::ios::app)
			<< timestamp << " " << log << " "
			<< addr << std::endl;
	}
}

static void unsubalias(std::string addr)
{
	DbObj	aliases;
	std::string buf;

	if (aliases.Open(ALIASES, "W"))	return;

	buf="0";
	buf += addr;

	std::string p=aliases.Fetch(buf, "");

	if (p == "") return;

	aliases.Delete(buf);

	p="1" + p;

	aliases.Delete(p);
}
