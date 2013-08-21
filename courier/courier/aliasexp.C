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

#if	HAVE_SYS_STAT_H
#include	<sys/stat.h>
#endif
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<iostream>
#include	<iomanip>
#include	<fstream>
#include	<sstream>
#include	<string>
#include	<algorithm>
#include	<functional>
#include	<cctype>



// MAXDUMP is the maximum # of addresses printed per list by the -dump
// flag.  Larger lists are printed in parts.  This allows the output
// of -dump to be refed into makealiases without using up a lot of memory.
//

static const char *srcfilename=0;
static const char *tmpdir=0;
static const char *xpfix="";

static struct courier_args arginfo[]={
	{"src", &srcfilename},
	{"xaliastmpdir", &tmpdir},
	{"xaliaspfix", &xpfix},
	{0}
	} ;

static std::string normalize(struct rw_transport *module,
			     struct rfc822token *t,
			     int &islocal)
{
struct	rw_info_rewrite	rwir;
struct	rw_info rwi;
char	*address, *p;
char	*hostdomain;

	islocal=0;
	rw_info_init(&rwi, t, rw_err_func);

	rwi.mode=RW_ENVRECIPIENT;
	rwi.udata=(void *)&rwir;
	rwir.buf=0;
	rwir.errmsg=0;

	p=rfc822_gettok(rwi.ptr);

	rw_rewrite_module(module, &rwi, rw_rewrite_chksyn_at_ok_print);
	address=((struct rw_info_rewrite *)rwi.udata)->buf;
	if (!address)
	{
		if (!p)	clog_msg_errno();
		clog_msg_start_err();
		clog_msg_str(p);
		free(p);
		clog_msg_str(": ");
		while ((p=strchr(((struct rw_info_rewrite *)rwi.udata)->errmsg,
			'\n')) != 0)
			*p=p[1] ? '/':'\0';
		clog_msg_str(((struct rw_info_rewrite *)rwi.udata)->errmsg);
		clog_msg_send();
		exit(1);
	}
	free(p);

	hostdomain=0;
	if ((p=strrchr(address, '@')) == 0
		|| (config_islocal(p+1, &hostdomain) && hostdomain == 0))
	{
		locallower(address);
		islocal=1;
	}
	if (hostdomain)	free(hostdomain);

	domainlower(address);

	std::string s=address;
	free(address);

	return (s);
}

/*
** Ok, we have an alias defined as /pathname, or | prog.  Emulate it by
** substituting a dummy address for it.
*/

static void sendmailext(std::string aliasname, std::string cmd)
{
	static int first_time=1;

	if (tmpdir == 0)	return;

	std::ostringstream aliasnameEscaped;
	std::string::iterator b=aliasname.begin(), e=aliasname.end();

	while (b != e)
	{
		if (strchr("/.-+\"\\", *b))
		{
			aliasnameEscaped << '+' <<
				std::hex << std::setw(2) << std::setfill('0')
					 << (int)*b;
		}
		else
			aliasnameEscaped << *b;
		++b;
	}

	std::string s=aliasnameEscaped.str();

	if (first_time)
	{
		first_time=0;
		mkdir(tmpdir, 0755);
	}

	std::string t=std::string(tmpdir) + "/" + s;

	std::cout << "<\"" << xpfix << s
		  << "\"@" << config_defaultdomain() << ">" << std::endl;

	std::ofstream	o(t.c_str(), std::ios::out | std::ios::app);

	if (o.fail())
	{
		perror(t.c_str());
		exit(1);
	}

	o << cmd << std::endl << std::flush;

	if (o.fail() || (o.close(), o.fail()))
	{
		perror(t.c_str());
		exit(1);
	}
}

static void doline(std::string alias,
		   const char *line, struct rw_transport *module)
{
struct rfc822t *tp=rfc822t_alloc_new( line, NULL, NULL);
int	i;

	if (!tp)	clog_msg_errno();

struct rfc822a *ta=rfc822a_alloc(tp);

	if (!ta)	clog_msg_errno();

	for (i=0; i<ta->naddrs; i++)
	{
	int	islocal=0;

		if (!ta->addrs[i].tokens)	continue;

		std::string p;

		if (*alias.c_str() != '@')
		{
			p=normalize(module, ta->addrs[i].tokens, islocal);
		}
		else
		{
		/*
		** When the name of the alias starts with an @, it's a virtual
		** domain definition, so don't convert the addresses to
		** canonical format!!
		*/
			char *ps=rfc822_gettok(ta->addrs[i].tokens);

			if (!ps)	clog_msg_errno();
			p=ps;
			free(ps);
		}

		std::cout << '<' << p << '>' << std::endl;
	}
	rfc822a_free(ta);
	rfc822t_free(tp);
}

static int doexpaliases(std::string line, struct rw_transport *module)
{
	std::string	name;

	std::string::iterator b=std::find_if(line.begin(),
					     line.end(),
					     std::not1(std::ptr_fun(isspace)));
	std::string::iterator e=line.end();

	while (b < e && isspace(e[-1]))
		--e;

	line=std::string(b, e);

	if (line.size() == 0)	return (0);

	e=line.end();

	b=std::find(line.begin(), e, ':');

	if (b == e)
		return -1;

	name=std::string(line.begin(), b);
	line=std::string(++b, line.end());

	struct rfc822t	*nametok=rw_rewrite_tokenize(name.c_str());
	int dummy;
	std::string norm_addr=normalize(module, nametok->tokens, dummy);

	rfc822t_free(nametok);
	std::cout << '<' << norm_addr << '>' << std::endl;

	line=std::string(std::find_if(line.begin(), line.end(),
				      std::not1(std::ptr_fun(isspace))),
			 line.end());

	const char *linep=line.c_str();

	if (*linep == '/' || *linep == '|')
	{
		sendmailext(norm_addr, line);
		std::cout << std::endl;
		return (0);
	}

	if (line.substr(0, 9) != ":include:")
	{
		doline(norm_addr, line.c_str(), module);
		std::cout << std::endl;
		return (0);
	}

	line=line.substr(9);
	if ( line.substr(0, 1) != "/")
	{
		clog_msg_start_err();
		clog_msg_str("Absolute pathname is required:");
		clog_msg_send();
		return (-1);
	}

	std::ifstream	ifs;

	ifs.open(line.c_str());
	if (!ifs.is_open())
		clog_msg_errno();

	while (std::getline(ifs, line).good())
	{
		std::string::iterator b=line.begin(), e=line.end(),
			p=std::find(b, e, '#');

		if (p != e)
			line=std::string(b, p);
		doline(norm_addr, line.c_str(), module);
	}
	std::cout << std::endl;
	return (0);
}

static int expaliases(std::string line, struct rw_transport *module,
		      unsigned linenum, std::string &filename)
{
	if (doexpaliases(line, module))
	{
		clog_msg_start_err();
		if (filename.size() > 0)
		{
			clog_msg_str( filename.c_str() );
			clog_msg_str(": ");
		}
		clog_msg_str("syntax error, line ");
		clog_msg_uint(linenum);
		clog_msg_send();
		return (-1);
	}
	return (0);
}

static int aliasexp(std::istream &is, struct rw_transport *module)
{
std::string	line, next_line;
unsigned linenum=0;
int	rc=0;
std::string	filename;

	line="";

	while (std::getline(is, next_line).good())
	{
		++linenum;

		if (next_line.substr(0, 15) == "##MaKeAlIaSeS##")
		{
			linenum=0;
			filename=next_line.substr(15);
		}

		std::string::iterator b=next_line.begin(),
			e=next_line.end(),
			p=std::find(b, e, '#');

		if (p != e)
			next_line=std::string(b, p);

		b=next_line.begin();
		e=next_line.end();

		while (b < e && isspace(e[-1]))
			--e;

		next_line=std::string(b, e);

		const	char *cp=next_line.c_str();

		if (!cp || *cp == ' ' || *cp == '\t')
		{
			line += next_line;
			continue;
		}

		if (expaliases(line, module, linenum, filename))
			rc=1;
		line=next_line;
	}

	if (expaliases(line, module, linenum, filename))
		rc=1;

	if (rc == 0)	std::cout << "." << std::endl;
	return (rc);
}

int cppmain(int argc, char **argv)
{
	int	argn;
	const	char *module="local";
	std::ifstream	ifs;
	std::istream		*i;

	argn=cargs(argc, argv, arginfo);

	if (argn < argc)
		module=argv[argn++];

	if (rw_init_courier(0))	exit (1);
	clog_open_stderr("aliasexp");
	if (!srcfilename || strcmp(srcfilename, "-") == 0)
		i= &std::cin;
	else
	{
		ifs.open(srcfilename);
		if (ifs.fail())
		{
			clog_msg_start_err();
			clog_msg_str("Unable to open ");
			clog_msg_str(srcfilename);
			clog_msg_send();
			exit(1);
		}
		i= &ifs;
	}

struct rw_transport *modulep=rw_search_transport(module);

	if (!modulep)
	{
		clog_msg_start_err();
		clog_msg_str(module);
		clog_msg_str(" - not a valid module.");
		clog_msg_send();
		exit(1);
	}

int rc=aliasexp(*i, modulep);

	return(rc);
}
