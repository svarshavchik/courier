/*
** Copyright 1998 - 2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"rw.h"
#include	"comcargs.h"
#include	<string.h>
#include	<signal.h>
#include	<stdlib.h>
#include	<stdio.h>

#if	HAVE_LOCALE_H
#include	<locale.h>
#endif

#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<iostream>
#include	<fstream>
#include	<list>
#include	<set>
#include	"dbobj.h"
#include	"aliases.h"


// MAXDUMP is the maximum # of addresses printed per list by the -dump
// flag.  Larger lists are printed in parts.  This allows the output
// of -dump to be refed into makealiases without using up a lot of memory.
//

#define	MAXDUMPCNT	(100-1)

static const char *srcfilename=0;
static const char *aliasfilename=0;
static const char *tmpfilename=0;
static const char *dumpflag=0;

static struct courier_args arginfo[]={
	{"src", &srcfilename},
	{"alias", &aliasfilename},
	{"tmp", &tmpfilename},
	{"dump", &dumpflag},
	{0}
	} ;

static void create_list(std::list<std::string> &addrlist, std::string listname,
			DbObj &newaliases, DbObj &workalias, DbObj &newindex)
{
	std::list<std::string>::iterator alb, ale;

	AliasRecord	newaliases_buffer(newaliases);
	AliasRecord	list_buffer(workalias);
	AliasRecord	list_parent(workalias);
	AliasRecord	dependencies(newindex);
	AliasRecord	aux_keys2(newindex);
	std::string	addr;
	int	firstdef;
	std::list<std::string>	added_list, added_list2;
	std::set<std::string> added_lookup, added2_lookup;

	std::string		sp;

	// Go through the list of addresses to add.

	list_buffer.Init(listname);
	firstdef=list_buffer.Init();

	// Step 1: adding addresses A1 .. An
	//
	// If Ax is itself a list, add Ax's members to the list of addresses
	// being added.
	//
	// For optimization purposes, the addresses that were actually added
	// (weren't in the list already) are saved to the list_added list.

	alb=addrlist.begin();
	ale=addrlist.end();

	while (alb != ale)
	{
		sp= *alb++;

		list_parent.Init(sp);

		int	notfound=list_parent.Init();

		if (sp == listname || notfound)
		{
			// foo: foo is allowed
			if (added_lookup.find(sp) == added_lookup.end())
			{
				added_list.push_back(sp);
				added_lookup.insert(sp);

#if	ALIASES_DEBUG
std::cout << "List: " << listname << ", member " << sp << std::endl;
#endif
			}
		}

		addr='\n' + sp + '\n';

		std::string found=newindex.Fetch(addr, "");

		if (sp == listname || !found.size())
		{
			if (added2_lookup.find(sp) == added2_lookup.end())
			{
				added_list2.push_back(sp);
				added2_lookup.insert(sp);
#if	ALIASES_DEBUG
				std::cout << "List: " << listname << ", real member " << sp << " (2)" << std::endl;
#endif
			}
		}

		newaliases_buffer.Init(sp);

		list_parent.StartForEach();
		while ((addr=list_parent.NextForEach()).size())
		{
			sp=addr;
			if (sp == listname)	continue;

			if (added_lookup.find(sp) == added_lookup.end())
			{
				added_list.push_back(sp);
				added_lookup.insert(sp);
#if	ALIASES_DEBUG
std::cout << "List: " << listname << ", member " << sp << std::endl;
#endif
			}
		}


		newaliases_buffer.StartForEach();
		while ( (addr=newaliases_buffer.NextForEach()).size())
		{
			sp=addr;
			if (added2_lookup.find(sp) == added2_lookup.end())
			{
#if	ALIASES_DEBUG
				std::cout << "List: " << listname << ", real member " << sp << " (1)" << std::endl;
#endif
				added_list2.push_back(sp);
				added2_lookup.insert(sp);
			}
		}
	}

	list_buffer.Add(added_list, 1);

	newaliases_buffer.Init(listname);
	newaliases_buffer.Add(added_list2, 1);

	if (added_list.empty())	return;

	// Populate 'dependencies' with lists that include this
	// list, to which added_list addresses were just added.

	dependencies.Init(listname);
	dependencies.StartForEach();

	while ( (addr=dependencies.NextForEach()).size() != 0)
	{
#if	ALIASES_DEBUG
		std::cout << "Copying added members to: " << addr << std::endl;
#endif
		list_parent.Init(addr);

		if (firstdef)	list_parent.Delete(listname.c_str());
			// If this is the first listname definition, delete the
			// reference to the listname from the dependent
			// list's contents.

		list_parent.Add(added_list, 0);


		newaliases_buffer.Init(addr);
		newaliases_buffer.Add(added_list2, 0);
	}

	// Now, if we added address/list X to list Y, make sure X's
	// dependencies include all of Y's dependencies.

	std::list<std::string> dependency_list;

	dependencies.StartForEach();
	dependency_list.push_back(listname);

#if	ALIASES_DEBUG
	std::cout << "Preparing to add dependencies: " << listname;
#endif
	while ( (addr=dependencies.NextForEach()).size())
	{
		if (addr != listname)
		{
#if	ALIASES_DEBUG
			std::cout << ", " << addr;
#endif
			dependency_list.push_back(addr);
		}
	}
#if	ALIASES_DEBUG
	std::cout << std::endl;
#endif

	std::list<std::string>::iterator addb, adde;

	addb=added_list.begin();
	adde=added_list.end();

	while (addb != adde)
	{
		std::string s= *addb++;

#if	ALIASES_DEBUG
		std::cout << "... Added to " << s << std::endl;
#endif
		aux_keys2.Init(s);
		aux_keys2.Add(dependency_list, 0);
	}
	return;
}

static int add_aliases(std::istream &i, std::string aliasname,
		       DbObj &newaliases, DbObj &newtmp, DbObj &newauxtmp)
{
	std::list<std::string> addrlist;
	std::string line;

	for (;;)
	{
		if (!std::getline(i, line).good()) return (1);

		if (line.size() == 0)	break;

		if (addrlist.size() > MAXDUMPCNT)
		{
			create_list(addrlist, aliasname, newaliases,
				    newtmp, newauxtmp);
			addrlist.clear();
		}
		addrlist.push_back(line.substr(1, line.size()-2));
	}

	if (addrlist.size())
		create_list(addrlist, aliasname, newaliases, newtmp, newauxtmp);
	return (0);
}

static int makealiases(std::istream &is)
{
	std::string	line;
	DbObj newaliases, workaliases, newtmp;
	char	*auxtmpfilename=mktmpfilename();
	char	*auxtmpfilename2=mktmpfilename();

	if (!auxtmpfilename || !auxtmpfilename2 ||
	    newaliases.Open(tmpfilename, "N") ||
		workaliases.Open(auxtmpfilename, "N") ||
		newtmp.Open(auxtmpfilename2, "N"))
	{
		clog_msg_errno();
		return (1);
	}
	unlink(auxtmpfilename);
	unlink(auxtmpfilename2);
	free(auxtmpfilename);
	free(auxtmpfilename2);

	for (;;)
	{
		if (!std::getline(is, line).good())
			return (1);

		if (line == ".")	break;

		if (line.substr(0, 1) == "*")
		{
			// List of aliases at the beginning of the stream
			line[0]='\n';
			line += '\n';

			if (newtmp.Store(line, "1", "R"))
			{
				clog_msg_errno();
				return (-1);
			}
			continue;
		}

		if (add_aliases(is, line.substr(1, line.size()-2),
			newaliases, workaliases, newtmp))	return (1);
	}

	if (dumpflag)
	{
		std::string	dumpbuf, dumplist, addr, listaddr;
		std::string	key, value;
		AliasRecord new_list(newaliases);
		int	dumpcount=atoi(dumpflag);

		if (dumpcount < 1)
			dumpcount=MAXDUMPCNT;

		for (key=newaliases.FetchFirstKeyVal(value);
		     key.size() > 0;
		     key=newaliases.FetchNextKeyVal(value))
		{
			addr=key;
			if (addr.find('\n') != addr.npos)
			{
				continue;	// Continuation record,
						// we'll get it as part of the
						// original.
			}

			new_list.Init(addr);

			dumpbuf=addr + ":";
		
			std::string dumpsep=dumpbuf;

			dumplist="";

		int	dumpcnt=0;

			new_list.StartForEach();
			while ((listaddr=new_list.NextForEach()).size())
			{
				if (dumpcnt++ >= dumpcount)
				{
					std::cout << dumplist
						  << std::endl << std::endl;
					dumplist=addr;
					dumpsep=":";
					dumpcnt=1;
				}
				else if (dumplist.size() +
					 listaddr.size() > 76)
				{
					std::cout << dumplist << dumpsep
						  << std::endl;
					dumplist="        " + listaddr;
					dumpsep=",";
					continue;
				}

				dumplist += dumpsep;
				dumplist += ' ';
				dumplist += listaddr;
				dumpsep=",";
			}
			if (dumplist.size() == 0)
				dumplist += dumpsep;
			std::cout << dumplist << std::endl << std::endl;
		}
	}

	newaliases.Close();
	workaliases.Close();
	newtmp.Close();
	if (!dumpflag && rename(tmpfilename, aliasfilename) < 0)
	{
		clog_msg_prerrno();
		return (1);
	}
	return (0);
}

static void usage()
{
	std::cerr << "Usage: makealiases -tmp=tmpfile -alias=aliasfile -dump" << std::endl;
	exit (1);
}

static void cleanup()
{
	unlink(tmpfilename);
}

static RETSIGTYPE sigint_sig(int signum)
{
	cleanup();
	signal(SIGINT, SIG_DFL);
	kill(getpid(), SIGINT);
#if	RETSIGTYPE != void
	return (0);
#endif
}

static RETSIGTYPE sigterm_sig(int signum)
{
	cleanup();
	signal(SIGTERM, SIG_DFL);
	kill(getpid(), SIGTERM);
#if	RETSIGTYPE != void
	return (0);
#endif
}

static RETSIGTYPE sighup_sig(int signum)
{
	cleanup();
	signal(SIGHUP, SIG_DFL);
	kill(getpid(), SIGHUP);
#if	RETSIGTYPE != void
	return (0);
#endif
}

int cppmain(int argc, char **argv)
{
#if HAVE_SETLOCALE
	setlocale(LC_ALL, "C");
#endif

	cargs(argc, argv, arginfo);

	if ((!dumpflag && !aliasfilename) || !tmpfilename)
		usage();

	signal(SIGINT, sigint_sig);
	signal(SIGTERM, sigterm_sig);
	signal(SIGHUP, sighup_sig);
	if (atexit(cleanup)) clog_msg_errno();

	clog_open_stderr("makealiases");

	int rc=makealiases(std::cin);

	cleanup();
	return(rc);
}
