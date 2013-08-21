/*
** Copyright 2000-2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"config.h"
#include	"cmlm.h"
#include	"cmlmcmdmisc.h"
#include	"cmlmsubunsub.h"
#include	"dbobj.h"
#include	"cmlmsublist.h"
#include	<iostream>
#include	<fstream>
#include	<sstream>
#include	<stdio.h>
#include	<string.h>
#include	<stdlib.h>
#include	<ctype.h>
#include	<time.h>
#include	"mydirent.h"
#include	<fcntl.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sysexits.h>
#if HAVE_LANGINFO_H
#include	<langinfo.h>
#endif
#if	HAVE_LOCALE_H
#include	<locale.h>
#endif


int updatelistheaders();

int cmdupdate(const std::vector<std::string> &argv)
{
	DIR	*dirp;
	struct	dirent *de;
	std::string addr=cmdget_s("ADDRESS");

	std::string lang_suffix;

	std::string templatedir=TEMPLATEDIR;

	std::vector<std::string>::const_iterator ab=argv.begin(),
		ae=argv.end();

	while (ab != ae)
	{
		std::string s= *ab++;

		if (s.substr(0, 7) == "--lang=")
			lang_suffix=s.substr(7);

		if (s.substr(0, 6) == "--dir=")
			templatedir=s.substr(6);
	}

	// Copy all the template files

	dirp=opendir(templatedir.c_str());
	while (dirp && (de=readdir(dirp)) != 0)
	{
		const char *p=strrchr(de->d_name, '.');
		std::string n;
		std::string line;
		std::string s;

		if (!p)
			continue;

		if (strcmp(p, ".tmpl") && strcmp(p, ".html"))
			continue;

		n= templatedir + "/" + de->d_name;

		if (lang_suffix.size())
		{
			// Install localized template, if available

			std::string nn=n + "." + lang_suffix;

			if (access(nn.c_str(), R_OK) == 0)
				n=nn;
		}

		std::ifstream	ifs(n.c_str());

		if (ifs.bad())
		{
			perror(n.c_str());
			return (1);
		}

		std::ofstream	ofs(de->d_name);

		if (ofs.bad())
		{
			perror(de->d_name);
			return (1);
		}

		// Copy line at a time, expanding the address macro

		while (std::getline(ifs, line).good())
		{
			size_t i;

			for (i=0; i+1<line.size(); i++)
			{
				if (line[i] != '#' || line[i+1] != '#')
					continue;

				s=line.substr(i+2);

				size_t j=s.find('#');

				if (j == s.npos || j+1 >= s.size() ||
				    s[j+1] != '#')
					continue;

				s=s.substr(0, j);

				bool add_mailto=true;

				if (s.substr(0, 1) == "-")
				{
					s=s.substr(1);
					add_mailto=false;
				}

				if (s.size() > 0)	s= "-" + s;

				size_t k=addr.rfind('@');

				if (k == addr.npos)	k=0;

				s= (add_mailto ? "mailto:":"")
					+ addr.substr(0, k)
					+ s + addr.substr(k);
			
				line=line.substr(0, i) +s+ line.substr(i+j+4);
				i += j+3;	
			}
			ofs << line << std::endl;
		}
		ofs.flush();
		if (ofs.fail())
		{
			perror(de->d_name);
			return (1);
		}
		ofs.close();
		if (ofs.fail())
		{
			perror(de->d_name);
			return (1);
		}
	}
	if (dirp)	closedir(dirp);

	return updatelistheaders();
}

// Create a mailing list

int cmdcreate(const std::vector<std::string> &argv)
{
	std::ofstream ofs(OPTIONS);

	if (!ofs.is_open())
	{
		perror(OPTIONS);
		return (1);
	}

	std::vector<std::string>::const_iterator
		b=argv.begin(),
		e=argv.end();

	while (b != e)
	{
		if (b->substr(0, 1) != "-")	// Option
			ofs << *b << std::endl;
		++b;
	}
	ofs << std::flush;
	if (ofs.bad())
	{
		perror(OPTIONS);
		return (1);
	}

	if (mkdir(TMP, 0755))
	{
		perror(TMP);
		return (1);
	}

	if (mkdir(DOMAINS, 0755))
	{
		perror(DOMAINS);
		return (1);
	}

	if (mkdir(COMMANDS, 0755))
	{
		perror(COMMANDS);
		return (1);
	}
	if (mkdir(ARCHIVE, 0755))
	{
		perror(ARCHIVE);
		return (1);
	}
	if (mkdir(UNSUBLIST, 0755))
	{
		perror(UNSUBLIST);
		return (1);
	}
	if (mkdir(MODQUEUE, 0755))
	{
		perror(MODQUEUE);
		return (1);
	}
	if (mkdir(BOUNCES, 0755))
	{
		perror(BOUNCES);
		return (1);
	}

	return cmdupdate(argv);
}

static int insertalias(std::string addr, std::string a);

int docmdalias(const char *addr, std::string subbuf)
{
	return insertalias(addr, header_s(subbuf, "x-alias"));
}

static int insertalias(std::string addr, std::string a)
{
	int	rc;

	rc=getinfo(addr, isfound);
	if (rc)
	{
		std::cerr << "<" << addr
			<< "> is not subscribed to this mailing list." << std::endl;
		return (9);
	}

	TrimLeft(a);
	TrimRight(a);

	std::string::iterator domain_p=std::find(a.begin(), a.end(), '@');


	std::transform(domain_p, a.end(), domain_p, std::ptr_fun(::tolower));

SubExclusiveLock subscription_lock;

DbObj	aliases;

	aliases.Open(ALIASES, "C");

	if (a.size() > 0)
	{
		std::string	key;

		key="1";
		key += a;

		if (aliases.Store(key, addr, "I"))
		{
			std::cerr << "<" << a << "> is an existing write-only alias."
				<< std::endl;
			return (9);
		}

		key="0";
		key += addr;

		std::string p=aliases.Fetch(key, "");

		if (p.size())
		{
			std::string s="1" + p;
			aliases.Delete(s);
		}

		aliases.Store(key, a, "R");
	}
	else
	{
		std::string key;

		key="0";
		key += addr;

		std::string p=aliases.Fetch(key, "");

		if (p.size())
		{
			std::string s="1"+p;

			aliases.Delete(s);
			aliases.Delete(key);
		}
	}
	return (0);
}

// List subscribed addresses

int cmdlsub(const std::vector<std::string> &args)
{
	std::string buf;
	SubscriberList subs;

	while (subs.Next(buf))
	{
		const char *p=subs.sub_info.c_str();
		time_t t=0;

		if (strncmp(p, "x-couriermlm-date:", 18) == 0)
		{
			p += 18;
			while (*p && isspace((int)(unsigned char)*p))
				++p;
			while (isdigit((int)(unsigned char)*p))
			{
				t=t * 10 + (*p-'0');
				++p;
			}
			if (t)
			{
				struct tm *tmptr=localtime(&t);
				char tbuf[100];

				if (strftime(tbuf, sizeof(tbuf),
					     "%d-%b-%Y", tmptr) > 0)
				{
					buf += " (";
					buf += tbuf;
					buf += ")";
				}
			}
		}

		std::cout << buf << std::endl;
	}
	return (0);
}

static void doexport(std::ostream &o)
{
	SubExclusiveLock subscription_lock;

	std::string buf;
	SubscriberList subs;

	while (subs.Next(buf))
	{
		o << buf << std::endl;

		std::istringstream i(subs.sub_info);

		std::string line;

		while (std::getline(i, line).good())
		{
			if (line.substr(0, 1) == ".")
				line="." + line;

			o << line << std::endl;
		}
		o << "." << std::endl;
	}

	SubscriberList aliaslist(true);

	while (aliaslist.Next(buf))
	{
		std::cout << "@" << buf << std::endl;
		std::cout << aliaslist.posting_alias << std::endl;
	}
}

int cmdexport(const std::vector<std::string> &args)
{
	doexport(std::cout);
	return (0);
}

static int doimport(std::istream &i)
{
	std::string	address;
	std::string	subinfo;
	std::string	linebuf;
	int	rc;

	while (std::getline(i, address).good())
	{
		subinfo="";

		if (address.substr(0, 1) == "@") // ALIAS
		{
			if (!std::getline(i, subinfo).good())
				break;

			address=address.substr(1);
			insertalias(address, subinfo);
			continue;
		}

		while (std::getline(i, linebuf).good())
		{
			if (linebuf == ".")	break;
			if (*linebuf.c_str() == '.')
				linebuf=linebuf.substr(1);
			subinfo += linebuf;
			subinfo += '\n';
		}

		rc=docmdsub(address.c_str(), subinfo, false);
		if (rc == 9)
		{
			std::cout << address << ": already subscribed." << std::endl;
			rc=0;
		}
		if (rc)	return (rc);
	}
	return (0);
}

int cmdimport(const std::vector<std::string> &args)
{
	return (doimport(std::cin));
}

int cmdlaliases(const std::vector<std::string> &args)
{
	std::string buf;
	SubscriberList subs(true);

	while (subs.Next(buf))
		std::cout << buf << ' ' << subs.posting_alias << std::endl;
	return (0);
}

// Display subscription information for this subscriber


static int print_info(std::string s)
{
	std::cout << s;
	return (0);
}

int cmdinfo(const std::vector<std::string> &args)
{
	int	rc=getinfo(args.empty() ? "":args[0], print_info);

	if (rc == EX_NOUSER)
		std::cerr << "No subscription information found." << std::endl;
	return (rc);
}
