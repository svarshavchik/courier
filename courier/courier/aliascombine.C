/*
** Copyright 1998 - 2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"rw.h"
#include	"comcargs.h"
#include	"rfc822/rfc822.h"
#include	<string.h>
#include	<signal.h>
#include	<stdlib.h>
#include	<stdio.h>

#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<string>
#include	<algorithm>
#include	<sstream>
#include	<iostream>
#include	"dbobj.h"
#include	"maxlongsize.h"
#include	"numlib/numlib.h"


static int readalias(DbObj &obj, std::string aliasname)
{
	std::string key=aliasname + "\n0";
	std::string lenptr=obj.Fetch(key, "");

	size_t	num;
	std::string name;
	std::string namelist;
	unsigned cnt;

	if (!lenptr.size())
	{
		num=1;
	}
	else
	{
		num=atol(lenptr.c_str())+1;
	}

	cnt=0;
	namelist="";
	for (;;)
	{
		if (!std::getline(std::cin, name).good())	return (-1);
		if (name.size() == 0)	break;
		if (cnt >= 100)
		{
			std::ostringstream o;

			o << aliasname << "\n" << num++;

			key=o.str();
			
			if (obj.Store(key, namelist, "R"))
				return (-1);
			namelist="";
			cnt=0;
		}
		namelist += name.substr(1, name.size()-2) + "\n";
		++cnt;
	}

	if (cnt)
	{
		std::ostringstream o;

		o << aliasname << "\n" << num;

		key=o.str();
		
		if (obj.Store(key, namelist, "R"))
			return (-1);
		key=aliasname + "\n0";

		std::ostringstream oo;

		oo << num;

		if (obj.Store(key, oo.str(), "R"))
			return (-1);
	}
	return (0);
}

static int dumpdb(DbObj &obj)
{
	std::string key, val;
	size_t	nblocks, i;

	for (key=obj.FetchFirstKeyVal(val); key.size();
	     key=obj.FetchNextKeyVal(val))
	{
		if (key.size() < 2 || key.substr(key.size()-2)
		    != "\n0")
			continue;
		std::cout << "*" << key.substr(0, key.size()-2) << std::endl;
	}

	for (key=obj.FetchFirstKeyVal(val); key.size();
	     key=obj.FetchNextKeyVal(val))
	{
		if (key.size() < 2 || key.substr(key.size()-2)
		    != "\n0")
			continue;

		std::string keystr=key.substr(0, key.size()-2);

		std::istringstream ii(val);

		nblocks=0;

		ii >> nblocks;
		std::cout << '<' << keystr << '>' << std::endl;
		for (i=0; i<nblocks; )
		{
			++i;

			std::ostringstream o;

			o << keystr << "\n" << i;

			if ((val=obj.Fetch(o.str(), "")).size())
			{
				std::string::iterator
					b=val.begin(),
					e=val.end(),
					p;

				while ((p=std::find(b, e, '\n')) != e)
				{
					std::cout << "<"
						  << std::string(b, p)
						  << ">" << std::endl;
					b= ++p;
				}
			}
		}
		std::cout << std::endl;
	}
	return (0);
}

int cppmain(int argc, char **argv)
{
	char *filename=mktmpfilename();
	DbObj tempdb;
	std::string	aliasname;

	clog_open_stderr("aliascombine");
	if (!filename || tempdb.Open(filename, "N"))
		clog_msg_errno();
	unlink(filename);
	free(filename);

	while (std::getline(std::cin, aliasname).good())
	{
		if (aliasname == ".")
		{
			if (dumpdb(tempdb))
			{
				clog_msg_prerrno();
				break;
			}

			std::cout << "." << std::endl;
			break;
		}
		aliasname=aliasname.substr(1, aliasname.size()-2);
		if (readalias(tempdb, aliasname))
		{
			clog_msg_prerrno();
			break;
		}
	}
	return (0);
}
