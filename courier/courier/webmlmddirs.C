/*
**
** Copyright 2007 Double Precision, Inc.
** See COPYING for distribution information.
*/
#include	"webmlmd.H"
#include	"webmlmddirs.H"
#include	"numlib/numlib.h"
#include	<stdio.h>
#include	<errno.h>
#include	<stdlib.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<sys/types.h>
#include	<sys/stat.h>

#include	<iostream>
#include	<algorithm>

// Helper class -- order by last component in a filename path.

class webmlmd::dirs::OrderByLastComponent {

public:
	OrderByLastComponent();
	~OrderByLastComponent();

	bool operator()(std::string, std::string) const;
};

webmlmd::dirs::OrderByLastComponent::OrderByLastComponent()
{
}

webmlmd::dirs::OrderByLastComponent::~OrderByLastComponent()
{
}

bool webmlmd::dirs::OrderByLastComponent::operator()(std::string a,
						     std::string b) const
{
	std::string::iterator ab=a.begin(), ae=a.end(),
		bb=b.begin(), be=b.end();

	while (ab != ae && bb != be)
	{
		if (*--ae != *--be)
			return *ae < *ab;

		if (*ae == '/')
			return false;
	}
	return ab == ae && bb != be;
}

// Helper class -- order list directories by the name of the mailing list.

class webmlmd::dirs::OrderByListName {

public:
	OrderByListName();
	~OrderByListName();

	bool operator()(std::string, std::string) const;
};

webmlmd::dirs::OrderByListName::OrderByListName()
{
}

webmlmd::dirs::OrderByListName::~OrderByListName()
{
}

std::string getlistname(std::string dir, std::string dirbasename);

bool webmlmd::dirs::OrderByListName::operator()(std::string a,
						std::string b) const
{
	return getlistname(a, webmlmd::basename(a))
		< getlistname(b, webmlmd::basename(b));
}

// Helper class -- compare last components of two file paths

class webmlmd::dirs::EqualLastComponent {

public:
	EqualLastComponent();
	~EqualLastComponent();

	bool operator()(std::string, std::string) const;
};

webmlmd::dirs::EqualLastComponent::EqualLastComponent()
{
}

webmlmd::dirs::EqualLastComponent::~EqualLastComponent()
{
}

bool webmlmd::dirs::EqualLastComponent::operator()(std::string a, std::string b) const
{
	std::string::iterator ab=a.begin(), ae=a.end(),
		bb=b.begin(), be=b.end();

	while (ab != ae && bb != be)
	{
		if (*--ae != *--be)
			return false;

		if (*ae == '/')
			return true;
	}

	return true;
}


webmlmd::dirs::dirs()
{
}

webmlmd::dirs::~dirs()
{
}


bool webmlmd::dirs::initialize(std::string colonpath)
{
	// Split the path into an array, first

	std::string::iterator b=colonpath.begin(), e=colonpath.end();
	std::string::iterator q=b;

	while(1)
	{
		if (b == e || *b == ':')
		{
			std::string dirname=std::string(q, b);

			if (dirname.size() > 0)
				push_back(dirname);

			if (b == e)
				break;

			q=++b;
			continue;
		}

		++b;
	}

	if (size() == 0)
	{
		std::cerr << "Missing LISTS setting" << std::endl;
		return false;
	}

	// Check if two directories carry same last component

	std::sort(begin(), end(), webmlmd::dirs::OrderByLastComponent());

	std::vector<std::string>::iterator bb=begin(), ee=end();

	bb=std::adjacent_find(bb, ee, webmlmd::dirs::EqualLastComponent());

	if (bb != ee)
	{
		std::cerr <<
			"Two directories cannot have the same last component:"
			  << std::endl
			  << "  " << *bb << std::endl
			  << "  " << bb[1] << std::endl;
		return false;
	}

	// Verify that all directories exist, and owned by same uid and gid
	// If I'm root, drop root

	std::sort(begin(), end(), webmlmd::dirs::OrderByListName());
	// Final order -- by list name

	bb=begin();

	while (bb != ee)
	{
		struct stat stat_buf;

		if (stat(bb->c_str(), &stat_buf) < 0)
		{
			perror(bb->c_str());
			return false;
		}

		if (stat_buf.st_uid == 0)
		{
			std::cerr << *bb << ": is owned by root!"
				  << std::endl;
			return false;
		}

		if (geteuid() == 0)
			libmail_changeuidgid(stat_buf.st_uid,
					     stat_buf.st_gid);

		if (geteuid() != stat_buf.st_uid ||
		    getegid() != stat_buf.st_gid)
		{
			std::cerr << "My uid/gid does not match "
				  << bb->c_str() << "'s"
				  << std::endl;
			return false;
		}
		++bb;
	}
	return true;
}
