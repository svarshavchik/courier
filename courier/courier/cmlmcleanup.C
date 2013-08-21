/*
** Copyright 2000-2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"config.h"
#include	"cmlm.h"
#include	"cmlmbounce.h"
#include	"cmlmcleanup.h"
#include	"cmlmmoderate.h"
#include	"cmlmarchive.h"
#include	"dbobj.h"
#include	"afx/afx.h"
#include	<stdio.h>
#include	<string.h>
#include	<fcntl.h>
#include	<time.h>
#include	<ctype.h>
#include	<errno.h>
#include	<iostream>
#include	<fstream>
#include	<list>
#include	<utime.h>
#include	"mydirent.h"


static int chkbounces(std::string d, int &counter);

// Delete stuff from TMP that's more than 8 hours old

static void purge_tmp(time_t &current_time)
{
	DIR	*dirp;
	struct	dirent *de;
	struct	stat	stat_buf;
	std::string n;
	std::list<std::string> rmlist;

	dirp=opendir(TMP);
	while (dirp && (de=readdir(dirp)) != 0)
	{
		if (de->d_name[0] == '.')	continue;

		n= TMP "/";
		n += de->d_name;
		if (stat(n.c_str(), &stat_buf) == 0 &&
			stat_buf.st_mtime + 8 * 60 * 60 < current_time)
		{
			rmlist.push_back(n);
		}
	}
	if (dirp)	closedir(dirp);

	while (!rmlist.empty())
	{
		unlink(rmlist.front().c_str());
		rmlist.pop_front();
	}
}

static void purge_commands(time_t &current_time)
{
	DIR	*dirp;
	struct	dirent *de;
	struct	stat	stat_buf;
	std::string n;
	std::list<std::pair<std::string, time_t> > rmlist;


	// Delete stuff from commands that's more than 48 hours old,
	// except keep bounces for 14 days.

	dirp=opendir(COMMANDS);
	while (dirp && (de=readdir(dirp)) != 0)
	{
		if (de->d_name[0] == '.')	continue;

	unsigned h=48 * 60 * 60;

		{
			std::string hs;

			hs=cmdget_s("PURGECMD");
			if (hs.size() > 0)
			{
				unsigned n=atoi(hs.c_str());

				if (n)
					h= n * 60 * 60;
			}
		}

		if (strncmp(de->d_name, "bounce", 6) == 0)
		{
			h= 14 * 24 * 60 * 60;
			{
				std::string hs;

				hs=cmdget_s("PURGEBOUNCE");
				if (hs.size() > 0)
				{
					unsigned n=atoi(hs.c_str());

					if (n)
						h= n * 24 * 60 * 60;
				}
			}
		}

		n= COMMANDS "/";
		n += de->d_name;
		if (stat(n.c_str(), &stat_buf) == 0 &&
			(time_t)(stat_buf.st_mtime + h) < current_time)
		{
			rmlist.push_back(std::make_pair(n, h));
		}
	}

	if (dirp)	closedir(dirp);


	// Once more, with locking

	CommandLock	cmd_lock;

	while (!rmlist.empty())
	{
		if (stat(rmlist.front().first.c_str(), &stat_buf) == 0 &&
		    (time_t)(stat_buf.st_mtime + rmlist.front().second)
		    < current_time)
			unlink(rmlist.front().first.c_str());
		rmlist.pop_front();
	}
}


//////////////////////////////////////////////////////////////////////////////
//
// Cleanup
//
//////////////////////////////////////////////////////////////////////////////

int cmdhourly(const std::vector<std::string> &args)
{
	int counter;
	ExclusiveLock hourly_lock(HOURLYLOCKFILE);
	time_t	current_time;
	DIR	*dirp;
	struct	dirent *de;
	struct	stat	stat_buf;
	std::string n;

	time(&current_time);

	purge_tmp(current_time);
	purge_commands(current_time);

	int	rc=0;

	counter=atoi(cmdget_s("MAXBOUNCES").c_str());
	if (counter <= 0)	counter=20;

	dirp=opendir(BOUNCES);
	while (dirp && (de=readdir(dirp)) != 0)
	{
		if (de->d_name[0] == '.')	continue;
		if (counter == 0)	break;

		n= BOUNCES "/";
		n += de->d_name;
		rc=chkbounces(n, counter);
		if (rc)	break;
	}
	if (dirp)	closedir(dirp);
	if (rc) return (rc);

	// Remoderate

	n=cmdget_s("POST");

	if (n == "mod")
	{
		time_t	t;
		int	nn=atoi(cmdget_s("REMODERATE").c_str());
		ExclusiveLock modqueue_lock(MODQUEUELOCKFILE);

		counter=atoi(cmdget_s("MAXMODNOTICES").c_str());
		if (counter <= 0)	counter=20;

		if (nn == 0)	nn=12;
		time (&t);
		t -= nn * 60 * 60;

		dirp=opendir(MODQUEUE);
		while (dirp && (de=readdir(dirp)) != 0)
		{
			if (de->d_name[0] == '.')	continue;
			if (counter == 0)	break;
			n= MODQUEUE "/";
			n += de->d_name;

			if (stat(n.c_str(), &stat_buf))	continue;
			if (stat_buf.st_mtime >= t)	continue;
			utime(n.c_str(), NULL);
			int i_fd=open(n.c_str(), O_RDONLY);

			if (i_fd < 0)
				continue;

			afxipipestream	f(i_fd);

			rc=sendinitmod(f, de->d_name, "modtext2.tmpl");
			if (rc)
			{
				closedir(dirp);
				return (rc);
			}
			--counter;
		}
		if (dirp)	closedir(dirp);
	}
	return (0);
}

static void expirecommands()
{
	CommandLock cmd_lock;
	DbObj	db;

	std::list<std::string> purgelist;

	if (db.Open(COMMANDSDAT, "W"))	return;

	std::string	k, v;
	std::string	f;
	struct	stat	stat_buf;

	for (k=db.FetchFirstKeyVal(v); k != ""; k=db.FetchNextKeyVal(v))
	{
		f=COMMANDS "/";
		f += v;
		if (stat(f.c_str(), &stat_buf) && errno == ENOENT)
			purgelist.push_back(k);
	}

	while (!purgelist.empty())
	{
		db.Delete(purgelist.front());
		purgelist.pop_front();
	}
}

static void purgebounces()
{
	ExclusiveLock bounce_lock(BOUNCESLOCK);
	DbObj	db;
	std::list<std::string>	purgelist;

	if (db.Open(BOUNCESDAT, "W"))	return;

	std::string	k, v;
	std::string	f;
	struct	stat	stat_buf;

	for (k=db.FetchFirstKeyVal(v); k != ""; k=db.FetchNextKeyVal(v))
	{
		f=BOUNCES "/";
		f += v;
		if (stat(f.c_str(), &stat_buf) && errno == ENOENT)
			purgelist.push_back(k);
	}

	while (!purgelist.empty())
	{
		db.Delete(purgelist.front());
		purgelist.pop_front();
	}
}

static void purgearchive()
{
	struct	stat	stat_buf;
	std::string buf;
	time_t	t;
	unsigned long n;

	buf=cmdget_s("PURGEARCHIVE");
	n=atol(buf.c_str());
	if (n == 0)	return;
	time(&t);

	std::list<std::string> purgelist;

	ArchiveList list;

	while ((buf=list.Next()).size())
	{
		if (stat(buf.c_str(), &stat_buf))	continue;
		if (stat_buf.st_mtime + (time_t)(n * 24 * 60 * 60) < t)
			purgelist.push_back(buf);
	}

	while (!purgelist.empty())
	{
		unlink(purgelist.front().c_str());
		purgelist.pop_front();
	}
}

// Subscription report

class SubReport {
public:
	time_t when;
	std::string address;
} ;

static void read_sublog(std::list<SubReport> &sub_list,
			std::list<SubReport> &unsub_list,
			std::list<SubReport> &bounce_list)
{
	std::ifstream list(SUBLOGFILE);

	if (!list.is_open())
		return;

	std::string buf;
	SubReport new_report;

	while (std::getline(list, buf).good())
	{
		const char *p=buf.c_str();
		time_t t=0;
		std::list<SubReport> *list_ptr;

		while (isdigit((int)(unsigned char)*p))
		{
			t = t*10 + (*p-'0');
			++p;
		}
		
		while (*p && isspace((int)(unsigned char)*p))
			++p;

		if (strncmp(p, "SUBSCRIBE", 9) == 0)
		{
			p += 9;
			list_ptr= &sub_list;
		}
		else if (strncmp(p, "UNSUBSCRIBE", 11) == 0)
		{
			p += 11;
			list_ptr= &unsub_list;
		}
		else if (strncmp(p, "BOUNCE", 6) == 0)
		{
			p += 6;
			list_ptr=&bounce_list;
		}
		else continue;

		while (*p && isspace((int)(unsigned char)*p))
			++p;

		if (*p)
		{
			new_report.when=t;
			new_report.address=p;
			list_ptr->push_back(new_report);
		}
	}
}

static void subreportsection(afxopipestream &o,
			     std::list<SubReport> l,
			     const char *template_name)
{
	if (l.empty())	return;

	copy_report(template_name, o);

	std::list<SubReport>::iterator b=l.begin(), e=l.end();

	while (b != e)
	{
		SubReport &record= *b++;

		o << "    " << record.address << std::endl;
	}
}

static void subreport()
{
	std::list<SubReport> sub_list, unsub_list, bounce_list;

	std::string report_addr=cmdget_s("REPORTADDR");
	SubExclusiveLock sub_lock;

	if (report_addr.size() > 0)
	{
		read_sublog(sub_list, unsub_list, bounce_list);
		if (sub_list.empty() &&
		    unsub_list.empty() &&
		    bounce_list.empty())
			return;

		const char *argv[6];

		argv[0]="sendmail";
		argv[1]="-f";

		std::string me=get_verp_return("owner");

		argv[2]=me.c_str();
		argv[3]="-N";
		argv[4]="fail";
		argv[5]=0;

		pid_t p;

		afxopipestream report(sendmail(argv, p));

		report << "From: " << me << std::endl
		       << "To: " << report_addr << std::endl;

		copy_report("subreporthdr.tmpl", report);

		subreportsection(report, sub_list, "subreporthdr1.tmpl");
		subreportsection(report, unsub_list, "subreporthdr2.tmpl");
		subreportsection(report, bounce_list, "subreporthdr3.tmpl");

		copy_report("subreportfooter.tmpl", report);

		report.close();

		if (wait4sendmail(p))
			return;
	}

	std::string	f=mktmpfilename();
	std::string	ufilename= UNSUBLIST "/log." + f;

	rename(SUBLOGFILE, ufilename.c_str());
}

int cmddaily(const std::vector<std::string> &args)
{
ExclusiveLock dailylock(DAILYLOCKFILE);

	expirecommands();
	purgebounces();
	purgearchive();
	subreport();
	return (0);
}

//////////////////////////////////////////////////////////////////////////////
//
// WARNING MESSAGE GENERATION
//
//////////////////////////////////////////////////////////////////////////////

//
//  Ok, poke around in the bounce directory.
//  Returns: 0 - ignore this, hasn't aged enough.
//          -1 - remove immediately, something wrong.
//           1 - generate a warning message.
//

static int chkbouncedir(std::string d, std::string &addr,
			std::string &lastbounce)
{
DIR	*dirp;
struct	dirent *de;

	{
		std::string	addy_name=d + "/.address";
		std::ifstream	addy(addy_name.c_str());

		if (!std::getline(addy, addr).good()) return -1;
	}

	if (getinfo(addr, isfound))
		return (-1);		// Not subscribed any more

	dirp=opendir(d.c_str());
	if ( !dirp )	return (0);

	time_t	oldestbounce=0;
	time_t	last_bounce_time=0;
	std::string n;
	struct	stat	stat_buf;
	time_t	current_time;

	time (&current_time);

	while ((de=readdir(dirp)) != 0)
	{
		if (de->d_name[0] == '.')	continue;

		n=d + "/";
		n += de->d_name;

		if (stat(n.c_str(), &stat_buf))	continue;
		if (oldestbounce == 0)
		{
			last_bounce_time=oldestbounce=stat_buf.st_mtime;
			lastbounce=n;
		}
		if (stat_buf.st_mtime < oldestbounce)
			oldestbounce=stat_buf.st_mtime;
		if (stat_buf.st_mtime > last_bounce_time)
		{
			last_bounce_time=stat_buf.st_mtime;
			lastbounce=n;
		}
	}
	closedir(dirp);
	if (oldestbounce == 0)	return (0);

	std::string grace=cmdget_s("STARTPROBE");

	if (grace.size() == 0)
		grace="3";

	if (oldestbounce + atoi(grace.c_str()) * 24 * 60 * 60 < current_time)
		return (1);
	return (0);
}

static void rmbouncedir(std::string d)	// Well, almost
{
	std::list<std::string> flist;

	// Delete bounce dir after we're done.

	DIR	*dirp;
	struct	dirent *de;

	dirp=opendir(d.c_str());
	while (dirp && (de=readdir(dirp)) != 0)
	{
		if (strcmp(de->d_name, ".") == 0)	continue;
		if (strcmp(de->d_name, "..") == 0)	continue;
		flist.push_back(de->d_name);
	}
	if (dirp)	closedir(dirp);

	while (!flist.empty())
	{
		std::string n=d + "/" + flist.front();
		flist.pop_front();

		unlink(n.c_str());
	}

	rmdir(d.c_str());
}

static int chkbounces(std::string d, int &counter)
{
	std::string	addr;
	std::string	lastbounce;

	if (chkbouncedir(d, addr, lastbounce) == 0)	return (0);

	// Once more, locked.

ExclusiveLock bounce_lock(BOUNCESLOCK);

int	rc=chkbouncedir(d, addr, lastbounce);

	if (rc == 0)	return (0);
	if (rc > 0)
	{
		rc=bouncewarning(addr, lastbounce, d);
		--counter;
	}
	else	rc=0;
	rmbouncedir(d);
	return (rc);
}
