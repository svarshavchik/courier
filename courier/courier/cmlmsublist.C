/*
** Copyright 2000-2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"config.h"
#include	"cmlmsublist.h"
#include	<string.h>


SubscriberList::SubscriberList( bool wantalias)
	: dirp(0), next_func( wantalias ? &SubscriberList::openalias:
			      &SubscriberList::domain )
{
}

SubscriberList::~SubscriberList()
{
	if (dirp)	closedir(dirp);
}

//
// Read addresses from dedicated domain files.
//

bool SubscriberList::domain(std::string &ret)
{
	struct	dirent *de;
	std::string key;
	std::string val;

	if (!dirp)	dirp=opendir(DOMAINS);

	if (domain_file.IsOpen())	// A domain file is currently open
	{
		key=domain_file.FetchNextKeyVal(val);

		if (key.size())
		{
			ret=key;
			sub_info=val;
			return (true);
		}

		domain_file.Close();
	}

	// Searching for the next domain file.

	while (dirp && (de=readdir(dirp)) != 0)
	{
		if (de->d_name[0] == '.')	continue;
		if (strchr(de->d_name, '.') == 0)	continue;

		std::string filename;

		filename=DOMAINS "/";
		filename += de->d_name;

		if (domain_file.Open(filename, "R") == 0)
		{
			key=domain_file.FetchFirstKeyVal(val);
			if (key.size())
			{
				ret=key;
				sub_info=val;
				return (true);
			}
			domain_file.Close();
		}
	}
	closedir(dirp);
	dirp=0;

	// Now, read the miscellaneous database

	if (domain_file.Open(MISC, "R"))	return (false);

	key=domain_file.FetchFirstKeyVal(val);
	if (key.size() == 0)
	{
		domain_file.Close();
		return (false);
	}

	splitmisc(val, misclist);
	next_func= &SubscriberList::shared;
	return (shared(ret));
}

void SubscriberList::splitmisc(std::string s,
			       std::list<std::string> &misclist)
{
	misclist.clear();

	std::string::iterator b=s.begin(), e=s.end(), p=b;

	while (b != e)
	{
		if (*b == 0)
		{
			misclist.push_back(std::string(p, b));
			p= ++b;
			continue;
		}
		++b;
		continue;
	}

	if (p != b)
		misclist.push_back(std::string(p, e));
}

std::string SubscriberList::joinmisc(const std::list<std::string> &misclist)
{
	std::list<std::string>::const_iterator mb, me;
	std::string r;

	mb=misclist.begin();
	me=misclist.end();

	while (mb != me)
	{
		r += *mb++;
		r.push_back((char)0);
	}

	return r;
}

// Read the miscellaneous database file

bool SubscriberList::shared(std::string &ret)
{
	while (misclist.empty())
	{
		if (!domain_file.IsOpen())
			return false;

		std::string key;
		std::string val;

		key=domain_file.FetchNextKeyVal(val);

		if (key.size() == 0)
		{
			domain_file.Close();
			return false;
		}

		splitmisc(val, misclist);
	}

	ret=misclist.front();
	misclist.pop_front();
	sub_info="";

	if (!misclist.empty())
	{
		sub_info=misclist.front();
		misclist.pop_front();
	}

	return true;
}

// Read the alias file, instead of the subscriber database.

bool SubscriberList::openalias(std::string &ret)
{
	if (domain_file.Open( ALIASES, "R" ))	return false;

	next_func= &SubscriberList::nextalias;

	std::string keys, vals;

	keys=domain_file.FetchFirstKeyVal(vals);

	if (keys.size() == 0)
		return false;

	ret=keys;
	posting_alias=vals;

	if (ret.substr(0, 1) == "1")
		return nextalias(ret);
	ret=ret.substr(1);

	return true;
}

bool SubscriberList::nextalias(std::string &ret)
{
	do
	{
		std::string keys, vals;

		keys=domain_file.FetchNextKeyVal(vals);

		if (keys.size() == 0)
			return false;

		ret=keys;
		posting_alias=vals;
	} while (ret.substr(0, 1) == "1");

	ret=ret.substr(1);
	return true;
}
