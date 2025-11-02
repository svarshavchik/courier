/*
** Copyright 1998 - 2025 Double Precision, Inc.
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
#include	<sstream>
#include	<string>
#include	<algorithm>
#include	<functional>
#include	<fstream>
#include	<cctype>



// MAXDUMP is the maximum # of addresses printed per list by the -dump
// flag.  Larger lists are printed in parts.  This allows the output
// of -dump to be refed into makealiases without using up a lot of memory.
//

const char *srcfilename=0;
static const char *tmpdir=0;
static const char *xpfix="";

courier_args aliasexp_arginfo[]={
	{"src", &srcfilename},
	{"xaliastmpdir", &tmpdir},
	{"xaliaspfix", &xpfix},
	{0}
	} ;

static std::string normalize(struct rw_transport *module,
			     const rfc822::tokens &t,
			     int &islocal)
{
	struct	rw_info_rewrite	rwir;
	struct	rw_info rwi{RW_ENVRECIPIENT, {}, {}};
	std::string orig_addr;
	char	*address, *p;
	char	*hostdomain;

	islocal=0;
	rw_info_init(&rwi, t, rw_err_func);

	rwi.udata=(void *)&rwir;
	rwir.buf=0;
	rwir.errmsg=0;

	size_t l=rwi.addr.print( rfc822::length_counter{} );
	orig_addr.reserve(l);
	rwi.addr.print(std::back_inserter(orig_addr));

	rw_rewrite_module(module, &rwi, rw_rewrite_chksyn_at_ok_print);
	address=((struct rw_info_rewrite *)rwi.udata)->buf;
	if (!address)
	{
		clog_msg_start_err();
		clog_msg_str(orig_addr.c_str());
		clog_msg_str(": ");

		while ((p=strchr(((struct rw_info_rewrite *)rwi.udata)->errmsg,
			'\n')) != 0)
			*p=p[1] ? '/':'\0';
		clog_msg_str(((struct rw_info_rewrite *)rwi.udata)->errmsg);
		clog_msg_send();
		exit(1);
	}

	hostdomain=0;
	if ((p=strrchr(address, '@')) == 0
		|| (config_islocal(p+1, &hostdomain) && hostdomain == 0))
	{
		/*
		** Local address, convert the userid to lowercase.
		*/
		char *ua=ulocallower(address);
		free(address);
		address=ua;
		islocal=1;
	}
	if (hostdomain)	free(hostdomain);

	/*
	** Make sure the domain is in UTF8, and is in lowercase.
	*/
	char *addressu=udomainutf8(address);

	free(address);

	std::string s=addressu;
	free(addressu);

	return (s);
}

/*
** Ok, we have an alias defined as /pathname, or | prog.  Emulate it by
** substituting a dummy address for it.
*/

static void sendmailext(std::ostream &aliasout,
			std::string aliasname, std::string cmd)
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

	aliasout << "<\"" << xpfix << s
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

static void doline(std::ostream &aliasout,
		   std::string alias,
		   const char *line, struct rw_transport *module)
{
	rfc822::tokens tp{line};

	rfc822::addresses ta{tp};

	for (auto &addr:ta)
	{
		int	islocal=0;

		if (addr.address.empty())	continue;

		std::string p;

		if (*alias.c_str() != '@')
		{
			p=normalize(module, addr.address, islocal);
		}
		else
		{
		/*
		** When the name of the alias starts with an @, it's a virtual
		** domain definition, so don't convert the addresses to
		** canonical format!!
		*/
			size_t l=addr.address.print( rfc822::length_counter{} );
			p.reserve(l);
			addr.address.print(std::back_inserter(p));
		}

		aliasout << '<' << p << '>' << std::endl;
	}
}

static int doexpaliases(std::ostream &aliasout,
			std::string line, struct rw_transport *module)
{
	std::string	name;

	std::string::iterator b=std::find_if(line.begin(),
					     line.end(),
					     [](char c){return !isspace(c);});
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

	int dummy;
	std::string norm_addr=normalize(module,
					rw_rewrite_tokenize(name.c_str()),
					dummy);

	aliasout << '<' << norm_addr << '>' << std::endl;

	line=std::string(std::find_if(line.begin(), line.end(),
				      [](char c){return !isspace(c);}),
			 line.end());

	const char *linep=line.c_str();

	if (*linep == '/' || *linep == '|')
	{
		sendmailext(aliasout, norm_addr, line);
		aliasout << std::endl;
		return (0);
	}

	if (line.substr(0, 9) != ":include:")
	{
		doline(aliasout, norm_addr, line.c_str(), module);
		aliasout << std::endl;
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
		doline(aliasout, norm_addr, line.c_str(), module);
	}
	aliasout << std::endl;
	return (0);
}

static int expaliases(std::ostream &aliasout,
		      std::string line, struct rw_transport *module,
		      unsigned linenum, std::string &filename)
{
	if (doexpaliases(aliasout, line, module))
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

int aliasexp(std::istream &is,
	     std::ostream &os, struct rw_transport *module)
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

		if (expaliases(os, line, module, linenum, filename))
			rc=1;
		line=next_line;
	}

	if (expaliases(os, line, module, linenum, filename))
		rc=1;

	if (rc == 0)	os << "." << std::endl;
	return (rc);
}
