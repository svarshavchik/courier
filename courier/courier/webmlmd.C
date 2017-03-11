/*
** Copyright 2007-2009 Double Precision, Inc.  See COPYING for
** distribution information.
**
*/

#include	"config.h"
#include	"cgi/cgi.h"
#include	"rfc822/rfc822.h"
#include	"rfc822/rfc2047.h"
#include	"numlib/numlib.h"
#include	<courier-unicode.h>
#include	"datadir.h"

#include	<stdio.h>
#include	<errno.h>
#include	<stdlib.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<string.h>
#include	<signal.h>

#include	<sys/types.h>
#include	<sys/stat.h>
#include        <sys/socket.h>
#include        <sys/un.h>
#if	HAVE_LOCALE_H
#if	HAVE_SETLOCALE
#include	<locale.h>
#endif
#endif

#include	<list>
#include	<vector>
#include	<string>
#include	<algorithm>
#include	<iostream>
#include	<fstream>
#include	<sstream>
#include	<map>

#include	"cmlm.h"
#include	"cmlmcmdmisc.h"
#include	"webmlmd.H"
#include	"webmlmdcmlm.H"
#include	"webmlmddirs.H"
#include	"webmlmdhandlers.H"

#define TEMPLATEDIR DATADIR "/couriermlm"

extern "C" {

	void fake_exit(int rc)
	{
		exit(rc);
	}

	void rfc2045_enomem(const char *errmsg)
	{
		cginocache();
		std::cout << "Content-Type: text/plain" << std::endl
			  << std::endl
			  << errmsg
			  << std::endl << std::flush;
		fake_exit(1);
	}

	void error(const char *errmsg)
	{
		cginocache();
		printf("Content-Type: text/html; charset=utf-8\n\n"
                       "<html><head><title>%s</title></head><body><h1>%s</h1></body></html>\n",
                       errmsg, errmsg);
		fake_exit(1);
	}
}

webmlmd::dirs mlm_dirs;

std::string toutf8str(const std::u32string &w)
{
	return unicode::iconvert::convert(w, "utf-8");
}

// First component of PATH_INFO is the mailing list name

std::string webmlmd::list_name()
{
	const char *pi=getenv("PATH_INFO");

	if (!pi)
		return "";

	if (*pi == '/')
		++pi;

	std::string list_name(pi);

	std::string::iterator b=list_name.begin(),
		p=std::find(b, list_name.end(), '/');

	return std::string(b, p);
}

// The remaining component in PATH_INFO is the list command

static std::string list_cmd()
{
	const char *pi=cgiextrapath();

	if (*pi == '/')
		++pi;

	std::string list_name(pi);

	std::string::iterator b=list_name.begin(), e=list_name.end(),
		p=std::find(b, e, '/');

	if (p != e)
		++p;

	return std::string(p, e);
}

static void main2(void *);

static void init_dirs(void *dummy)
{
	const char *dirs=getenv("LISTS");

	if (!mlm_dirs.initialize(dirs ? dirs:""))
	{
		exit(1);
	}
}

int main(int argc, char **argv)
{
	const char *cmd=NULL;
	const char *port_s=getenv("PORT");
	const char *prefork_s=getenv("PREFORK");

	if (argc > 1)
		cmd=argv[1];

#if HAVE_SETLOCALE
	setlocale(LC_ALL, "C");
#endif

	/*
	** For convenience's sake, we need to start webmlmd by a helper script.
	** Rather than confusing folks with webmlmd.rc, automatically invoke
	** the helper script, when needed.
	*/

	if (cmd && strcmp(cmd, "check"))
	{
		std::vector<char> shscript_v;

		{
			std::string shscript=argv[0];

			shscript += ".rc";

			shscript_v.reserve(shscript.size()+1);

			shscript_v.insert(shscript_v.end(),
					  shscript.begin(),
					  shscript.end());
			shscript_v.push_back(0);
		}

		std::vector<char *> new_argv;

		new_argv.push_back(&shscript_v[0]);

		new_argv.insert(new_argv.end(), argv, argv+argc);
		new_argv.push_back(0);

		execvp(new_argv[0], &new_argv[0]);
		perror(new_argv[0]);
		exit(1);
	}

	if (!port_s || !*port_s)
	{
		std::cerr << "PORT not set in the configuration file"
			  << std::endl;
		exit(1);
	}

	if (argc > 1 && strcmp(argv[1], "check") == 0)
	{
		if (access(port_s, 0) == 0)
		{
			if (access(port_s, W_OK) < 0)
			{
				perror(port_s);
				exit(1);
			}
		}
		else
		{
			int fd=socket(PF_UNIX, SOCK_STREAM, 0);

			if (fd >= 0)
			{
				struct sockaddr_un skun;

				memset(&skun, 0, sizeof(skun));

				if (strlen(port_s) >= sizeof(skun.sun_path))
				{
					fprintf(stderr, "%s: pathname too long\n",
						port_s);
					exit(1);
				}

				skun.sun_family=AF_UNIX;

				strcpy(skun.sun_path, port_s);

				if (bind(fd, (const struct sockaddr *)&skun,
					 sizeof(skun)) ||
				    listen(fd, 5) < 0)
				{
					close(fd);
					fd= -1;
				}
			}

			if (fd < 0)
			{
				perror(port_s);
				exit(1);
			}
			close(fd);
		}
	}

	if (cmd)
	{
		init_dirs(NULL);
		exit(0); /* The "check" command */
	}

	cgi_daemon(atoi(prefork_s), port_s, init_dirs, main2, NULL);

	return (0);
}

std::string getoption(std::string listdir,
		      std::string option)
{
	std::string n=listdir + "/options";

	std::ifstream i(n.c_str());

	std::string s;

	if (!i.is_open())
		return s;

	while (std::getline(i, s).good())
	{
		std::string::iterator b=s.begin(), e=s.end(),
			p=std::find(b, e, '=');

		if (std::string(b, p) == option)
		{
			if (p != e)
				++p;

			return std::string(p, e);
		}
	}
	return "";
}

std::string getoption(std::string option)
{
	return getoption(".", option);
}

std::u32string getwoption(std::string dir, std::string option)
{
	std::string s=getoption(dir, option);

	std::u32string u;

	unicode::iconvert::convert(s, "utf-8", u);

	return u;
}

std::u32string getwoption(std::string option)
{
	return getwoption(".", option);
}

std::string getlistname(std::string dir, std::string dirbasename)
{
	std::u32string listname=getwoption(dir, "LISTNAME");

	if (listname.size() == 0)
	{
		listname.insert(listname.end(),
				dirbasename.begin(),
				dirbasename.end());
	}

	return toutf8str(webmlmd::html_escape(listname));
}

std::string getlistname(std::string dirbasename)
{
	return getlistname(".", dirbasename);
}

HANDLER("LISTS", emit_lists)
{
	std::vector<std::string>::iterator b=mlm_dirs.begin(),
		e=mlm_dirs.end();

	std::cout << "<ul>";

	while (b != e)
	{
		std::string n=webmlmd::basename(*b);

		std::cout << "<li><a href=\"" << cgirelscriptptr() << "/"
			  << webmlmd::html_escape(n) << "\">"
			  << getlistname(*b, n)
			  << "</a></li>";
		++b;
	}
	std::cout << "</ul>";
}

HANDLER("LISTNAME", emit_list_name)
{
	std::cout << getlistname(list_name());
}

HANDLER("LISTDESCR", emit_list_descr)
{
	std::cout << getoption("LISTDESCR");
}

HANDLER("LISTURL", emit_list_url)
{
	std::cout << cgirelscriptptr() << "/" << list_name();
}

HANDLER("REPEATURL", emit_repeat_url)
{
	std::cout << cgirelscriptptr() << cgiextrapath();
}

static void emit_select(const char *name,
			const char *values,
			const std::list<std::string> &options,
			std::string default_option,
			size_t list_size=0,
			bool allow_multiple=false)
{
	std::string options_str;

	std::list<std::string>::const_iterator b=options.begin();

	while (b != options.end())
	{
		options_str += *b++;
		options_str += "\n";
	}

	char *buf=cgi_select(name, values, options_str.c_str(),
			     getoption("POST").c_str(), 0, 0);

	if (buf)
	{
		std::cout << buf;
		free(buf);
	}
}


static void emit_checkbox(const char *name, const char *value,
			  const char *opts)
{
	char *buf=cgi_checkbox(name, value, opts);

	if (buf)
	{
		std::cout << buf;
		free(buf);
	}
}

static void emit_textarea(const char *name, std::string value,
			  const char *wrap,
			  int rows, int cols, const char *opts)
{
	std::vector<char32_t> u;

	u.reserve(value.size()+1);

	u.insert(u.end(), value.begin(), value.end());
	u.push_back(0);

	char *buf=cgi_textarea(name, rows, cols, &u[0], wrap, opts);

	u.clear();

	if (buf)
	{
		std::cout << buf;
		free(buf);
	}
}

static void emit_input(const char *name, std::u32string value,
		       int size, int maxlength,
		       const char *opts)
{
	std::vector<char32_t> unicode_buf;

	unicode_buf.reserve(value.size()+1);

	unicode_buf.insert(unicode_buf.end(), value.begin(), value.end());
	unicode_buf.push_back(0);

	char *buf=cgi_input(name, &unicode_buf[0], size, maxlength, "");
	unicode_buf.clear();

	if (buf)
	{
		std::cout << buf;
		free(buf);
	}
}

HANDLER("ADMINUPDATE", do_admin_update)
{
	if (!*cgi("update"))
		return;

	std::list<std::string> new_cmds;

	static const char * const booleans[]={
		"!simpleconfirm",
		"casesensitive",
		"nobozos",
		"nodsn",
	};

	size_t i;

	for (i=0; i<sizeof(booleans)/sizeof(booleans[0]); i++)
	{
		const char *p=booleans[i];

		const char *yes, *no;

		if (*p =='!')
		{
			++p;

			yes="0";
			no="1";
		}
		else
		{
			yes="1";
			no="0";
		}

		std::string setting=p;

		std::transform(setting.begin(), setting.end(),
			       setting.begin(), std::ptr_fun(toupper));

		std::string varname("opt");

		varname += p;

		new_cmds.push_back(setting + "=" +
				   (*cgi(varname.c_str()) ? yes:no));
	}

	new_cmds.push_back(*cgi("optsubscribe") ?
			   "SUBSCRIBE=mod":"subscribe=all");

	static const char * const plains[]={
		"post",
		"postarchive",
		"reportaddr",
		"digest",
		"listname",
		"keyword"
	};

	for (i=0; i<sizeof(plains)/sizeof(plains[0]); i++)
	{

		std::string setting=plains[i];

		std::transform(setting.begin(), setting.end(),
			       setting.begin(), std::ptr_fun(toupper));

		std::string varname("opt");

		varname += plains[i];

		new_cmds.push_back(setting + "=" + cgi(varname.c_str()));
	}

	static const struct {
		const char *name;
		unsigned minvalue;
		unsigned maxvalue;
	} numerical[]={
		{"startprobe", 3, 30},
		{"maxbounces", 10, 50},
		{"purgebounce", 5, 30},
		{"maxmodnotices", 5, 99},
		{"remoderate", 12, 48},
		{"maxfetchsize", 100, 9999},
		{"purgearchive", 2, 9999},
		{"purgecmd", 6, 720},
	};

	for (i=0; i<sizeof(numerical)/sizeof(numerical[0]); i++)
	{

		std::string setting=numerical[i].name;

		std::transform(setting.begin(), setting.end(),
			       setting.begin(), std::ptr_fun(toupper));

		std::string varname("opt");

		varname += numerical[i].name;

		std::ostringstream o;

		o << setting << "=";

		int nn=atoi(cgi(varname.c_str()));

		if (nn > 0)
		{
			if ((unsigned)nn < numerical[i].minvalue)
				nn=(int)numerical[i].minvalue;
			if ((unsigned)nn > numerical[i].maxvalue)
				nn=(int)numerical[i].maxvalue;

			o << nn;
		}

		new_cmds.push_back(o.str());
	}

	char *p=rfc2047_encode_str(cgi("optname"), "utf-8",
				   rfc2047_qp_allow_word);

	new_cmds.push_back(std::string("NAME=") + (p ? p:""));

	if (p)
		free(p);

	std::vector<std::string> args_array;

	args_array.reserve(new_cmds.size());
	args_array.insert(args_array.end(), new_cmds.begin(),
			  new_cmds.end());

	cmdset(args_array, false);

	{
		std::string key=cgi("optheaderadd");

		key += "\n";

		std::istringstream i(key);

		std::ofstream ofs(HEADERADD ".new");

		std::string s;

		while (std::getline(i, s).good())
			if (s.size() > 0)
			{
				ofs << s << "\n";
			}

		ofs.close();
		rename(HEADERADD ".new", HEADERADD);
	}

	{
		std::string key=cgi("optheaderdel");
		key += "\n";

		std::istringstream i(key);

		std::ofstream ofs(HEADERDEL ".new");

		std::string s;

		while (std::getline(i, s).good())
			if (s.size() > 0)
			{
				ofs << s << "\n";
			}

		ofs.close();
		rename(HEADERDEL ".new", HEADERDEL);
	}

}

HANDLER("OPTPOST", emit_optpost_setting)
{
	if (args.size() != 3)
		return;

	emit_select("optpost",
		    "all\n"
		    "subscribers\n"
		    "mod\n", args, getoption("POST"));
}

HANDLER("OPTSUBSCRIBE", emit_optsubscribe)
{
	emit_checkbox("optsubscribe", "X",
		      getoption("SUBSCRIBE") == "mod" ? "*":"");
}

HANDLER("OPTSIMPLECONFIRM", emit_simpleconfirm)
{
	emit_checkbox("optsimpleconfirm", "X",
		      getoption("SIMPLECONFIRM") != "1" ? "*":"");
}

HANDLER("OPTPURGECMD", emit_purgecmd)
{
	emit_input("optpurgecmd", getwoption("PURGECMD"), 8, 8, "");
}

HANDLER("OPTKEYWORD", emit_keyword)
{
	emit_input("optkeyword", getwoption("KEYWORD"), 32, 255, "");
}

HANDLER("OPTNAME", emit_name)
{
	std::string n=getoption("NAME");

	char *p=rfc822_display_hdrvalue_tobuf("subject",
					      n.c_str(),
					      "utf-8",
					      NULL,
					      NULL);
	if (p)
	{
		std::u32string u;

		unicode::iconvert::convert(p, "utf-8", u);

		emit_input("optname", u, 32, 255,  "");
		free(p);
	}
}

HANDLER("OPTLISTNAME", emit_listname)
{
	emit_input("optlistname", getwoption("LISTNAME"), 32, 255, "");
}

HANDLER("OPTPOSTARCHIVE", emit_postarchive)
{
	if (args.size() != 2)
		return;

	emit_select("optpostarchive",
		    "all\n"
		    "subscribers\n", args, getoption("POSTARCHIVE"));
}

HANDLER("OPTCASESENSITIVE", emit_casesensitive)
{
	emit_checkbox("optcasesensitive", "X",
		      getoption("CASESENSITIVE") == "1" ? "*":"");
}

HANDLER("OPTREPORTADDR", emit_reportaddr)
{
	emit_input("optreportaddr", getwoption("REPORTADDR"), 32, 255, "");
}

HANDLER("OPTNOBOZOS", emit_nobozos)
{
	emit_checkbox("optnobozos", "X",
		      getoption("NOBOZOS") != "0" ? "*":"");
}

HANDLER("OPTDIGEST", emit_digest)
{
	emit_input("optdigest", getwoption("DIGEST"), 32, 255, "");
}

HANDLER("OPTMAXFETCHSIZE", emit_maxfetchsize)
{
	emit_input("optmaxfetchsize", getwoption("MAXFETCHSIZE"), 8, 8, "");
}

HANDLER("OPTNODSN", emit_nodsn)
{
	emit_checkbox("optnodsn", "X",
		      getoption("NODSN") == "1" ? "*":"");
}

HANDLER("OPTSTARTPROBE", emit_startprobe)
{
	emit_input("optstartprobe", getwoption("STARTPROBE"), 8, 8, "");
}

HANDLER("OPTMAXBOUNCES", emit_maxbounces)
{
	emit_input("optmaxbounces", getwoption("MAXBOUNCES"), 8, 8, "");
}

HANDLER("OPTPURGEBOUNCE", emit_purgebounce)
{
	emit_input("optpurgebounce", getwoption("PURGEBOUNCE"), 8, 8, "");
}

HANDLER("OPTMAXMODNOTICES", emit_maxmodnotices)
{
	emit_input("optmaxmodnotices", getwoption("MAXMODNOTICES"), 8, 8, "");
}

HANDLER("OPTREMODERATE", emit_remoderate)
{
	emit_input("optremoderate", getwoption("REMODERATE"), 8, 8, "");
}

HANDLER("OPTPURGEARCHIVE", emit_purgearchive)
{
	emit_input("optpurgearchive", getwoption("PURGEARCHIVE"), 8, 8, "");
}

HANDLER("OPTHEADERADD", emit_headeradd)
{
	std::ostringstream o;

	std::ifstream ifs(HEADERADD);

	if (ifs.is_open())
	{
		std::string line;

		while (std::getline(ifs, line).good())
			if (line.size())
				o << line << std::endl;
	}

	emit_textarea("optheaderadd", o.str(), "off", 4, 40, "");
}

HANDLER("OPTHEADERDEL", emit_headerdel)
{
	std::ostringstream o;

	std::ifstream ifs(HEADERDEL);

	if (ifs.is_open())
	{
		std::string line;

		while (std::getline(ifs, line).good())
			if (line.size())
				o << line << std::endl;
	}

	emit_textarea("optheaderdel", o.str(), "off", 4, 40, "");
}



static void showform(const char *filename,
		     const std::map<std::string, std::string> &parms)
{
	webmlmd::handler_list handlers;

	std::ifstream i(filename);

	if (!i.is_open())
	{
		std::cout << filename << ": cannot open"
			  << std::endl;
		return;
	}

	std::string line;

	while (std::getline(i, line).good())
	{
		std::string::iterator b=line.begin(), e=line.end();

		while (b != e)
		{
			static const char mac_open[]="[@";
			static const char mac_close[]="@]";

			std::string::iterator p=
				std::search(b, e, mac_open, mac_open+2);

			std::cout << std::string(b, p);

			b=p;

			if (b == e)
				break;

			b += 2;

			p=std::search(b, e, mac_close, mac_close+2);

			std::list<std::string> args;

			std::replace(b, p, '_', ' ');
			while (b != p)
			{
				std::string::iterator word_start=b;

				b=std::find(b, p, ':');

				args.push_back(std::string(word_start, b));

				if (b != p)
					++b;
			}

			if (b != e)
				b += 2;

			std::string handler_name;

			if (!args.empty())
			{
				handler_name=args.front();
				args.pop_front();
			}

			webmlmd::handler_list::iterator
				h=handlers.find(handler_name);

			if (h != handlers.end())
				(*h->second)(args);

			std::map<std::string, std::string>::const_iterator
				vb=parms.find(handler_name);

			if (vb != parms.end())
			{
				std::string txt=vb->second;

				if (vb->first.substr(0, 1) != ".")
					txt= webmlmd::html_escape(txt);
				std::cout << txt;
			}
		}
		std::cout << std::endl;
	}
}

void webmlmd::showhtmlform(const char *formname,
			   std::map<std::string, std::string> &parms)
{
	cginocache();
	std::cout << "Content-Type: text/html; charset=utf-8"
		  << std::endl
		  << std::endl;

	showform(formname, parms);
}

void webmlmd::showhtmlform(const char *formname)
{
	std::map<std::string, std::string> dummy;

	webmlmd::showhtmlform(formname, dummy);
}

static void showerrorform(const char *formname,
			  std::string errmsg)
{
	std::map<std::string, std::string> parms;

	parms["ERRMSG"]=errmsg;

	webmlmd::showhtmlform(formname, parms);
}

static void sendsubunsub(std::string ext)
{
	std::string address(cgi("address"));

	std::string::iterator b=address.begin(),
		e=address.end(),
		p=std::find(b, e, '@');

	if (p == e || std::find(p+1, e, '@') != e ||
	    std::find(p, e, '/') != e)
	{
		showerrorform("webmlmerror.tmpl.html",
			      "Invalid E-mail address: " + address);
		return;
	}

	webmlmd::cmlm ctlmsg;

	if (ctlmsg.start(ext, address, "ctlmsg"))
	{
		FILE *stdinptr=ctlmsg.stdinptr();

		ctlmsg.mk_received_header();
		fprintf(stdinptr, "From: %s\n", address.c_str());
		fprintf(stdinptr, "Subject: %s\n", ext.c_str());
		fprintf(stdinptr, "\nSubscription request received.\n");

		if (ctlmsg.wait())
		{
			webmlmd::showhtmlform("webmlmrequestreceived.tmpl.html");
			return;
		}
	}

	showerrorform("webmlmerror.tmpl.html",
		      "An error occured processing this request.\n"
		      "Please try again later.\n");
}

/*
** /admin request, password is good
*/

static void adminrequest(std::string admin_path)
{
	if (admin_path == "gosublist")
	{
		std::map<std::string, std::string> parms;

		parms["REFRESHURL"]=std::string(cgirelscriptptr())
			+ "/" + webmlmd::list_name() + "/admin/sublist";
		webmlmd::showhtmlform("webmlmpleasewait.tmpl.html", parms);
		return;
	}

	if (admin_path == "sublist")
	{
		webmlmd::showhtmlform("webmlmsublist.tmpl.html");
		return;
	}

	if (admin_path == "subinfo")
	{
		webmlmd::showhtmlform("webmlmsubinfo.tmpl.html");
		return;
	}

	if (admin_path == "mod")
	{
		std::string filename;

		std::map<std::string, std::string> parms;

		if (*cgi("domoderate"))
		{
			filename=cgi("msgname");

			if (filename.find('/') != filename.npos)
				filename="";
		}

		if (filename != "" && strcmp(cgi("modaction"), "accept") == 0)
		{
			parms[".ERRMSG"]=webmlmd::do_mod_accept(filename);

			if (parms[".ERRMSG"] != "")
				filename="";
		}
		else if (filename != "" && strcmp(cgi("modaction"), "reject")
			 == 0)
		{
			parms[".ERRMSG"]=
				webmlmd::do_mod_reject(filename,
						       *cgi("returntosender")
						       != 0,
						       cgi("rejectmsg"));

			if (parms[".ERRMSG"] != "")
				filename="";
		}

		if (filename != "")
		{
			size_t dummy;

			filename=webmlmd::get_next_mod_filename(filename,
								dummy);

			if (filename == "")
			{
				std::string url=std::string(cgirelscriptptr())
					+ "/" + webmlmd::list_name()
					+ "/admin";

				cgiredirect(url.c_str());

				fflush(stdout);
				std::cout << "\nRedirect to " << url << "\n";
				return;
			}

			cgi_put("msgname", filename.c_str());

		}

		webmlmd::showhtmlform("webmlmlistadminmod.tmpl.html", parms);
		return;
	}

	webmlmd::showhtmlform("webmlmlistadmin.tmpl.html");
}

/*
** Some list request
*/

static void listrequest2(std::string list_name, std::string path_info)
{
	if (path_info == "sendsub")
	{
		sendsubunsub("subscribe");
		return;
	}

	if (path_info == "sendunsub")
	{
		sendsubunsub("unsubscribe");
		return;
	}

	if (path_info == "doconfirm")
	{
		std::string method(cgi("method"));
		std::string token(cgi("token"));

		std::string errmsg="Confirmation failed.  No further information is available.";

		if (std::find_if(method.begin(), method.end(),
				 std::not1(std::ptr_fun(::isalpha))) ==
		    method.end() &&
		    std::find_if(token.begin(), token.end(),
				 std::not1(std::ptr_fun(::isalpha))) ==
		    token.end())
		{
			webmlmd::cmlm confirm;

			if (confirm.start(method + "-" + token, "",
					  "ctlmsg"))
			{
				FILE *stdinptr=confirm.stdinptr();

				confirm.mk_received_header();
				fprintf(stdinptr,
					"Subject: yes -- confirmed by WebMLM\n"
					"\n"
					"Confirmed\n");

				if (confirm.wait())
				{
					webmlmd::showhtmlform("webmlmprocessed.html");
					return;
				}

				char buf[1024];
				bool isfirst=true;

				FILE *stdoutptr=confirm.stdoutptr();

				while (fgets(buf, sizeof(buf), stdoutptr))
				{
					if (isfirst)
					{
						isfirst=false;
						errmsg="";
					}
					errmsg += buf;
				}
			}
		}

		std::map<std::string, std::string> args;

		args["ERROR"]=webmlmd::html_escape(errmsg);
		webmlmd::showhtmlform("webmlmnotprocessed.html", args);
		return;
	}


	std::string::iterator b=path_info.begin(), e=path_info.end();

	std::string::iterator p=std::find(b, e, '/');

	std::string w(b, p);

	if (p != e) ++p;

	if (w == "subconfirm" || w == "unsubconfirm")
	{
		std::map<std::string, std::string> args;

		args["METHOD"]=w;
		args["TOKEN"]=std::string(p, e);
		webmlmd::showhtmlform("webmlmconfirm.html", args);
		return;
	}

	if (w == "style.css")
	{
		std::map<std::string, std::string> args;

		std::cout << "Content-Type: text/css"
			  << std::endl << std::endl;
		showform("style.css.tmpl", args);
		return;
	}

	if (w == "admin")
	{
		char *password_cookie=cgi_get_cookie("password");
		const char *password;

		if (password_cookie)
		{
			if (getoption("LISTPW") == password_cookie)
			{
				free(password_cookie);
				adminrequest(std::string(p, e));
				return;
			}

			free(password_cookie);
		}

		password=cgi("password");

		if (*password && getoption("LISTPW") == password)
		{
			struct cgi_set_cookie_info cookie_info;

			cgi_set_cookie_info_init(&cookie_info);

			cgi_set_cookie_session(&cookie_info, "password",
					       password);

			std::string cookie_path;

			cookie_path=cgirelscriptptr();

			cookie_path += "/";
			cookie_path += list_name;

			cgi_set_cookie_url(&cookie_info, cookie_path.c_str());

			cgi_set_cookies(&cookie_info, 1);
			cgi_set_cookie_info_free(&cookie_info);
			adminrequest(std::string(p, e));
			return;
		}
		webmlmd::showhtmlform("webmlmlistadminpw.tmpl.html");
		return;
	}

	// Clear the password cookie when returning to the list screen

	{
		struct cgi_set_cookie_info cookie_info;

		cgi_set_cookie_info_init(&cookie_info);

		cgi_set_cookie_expired(&cookie_info, "password");

		std::string cookie_path;

		cookie_path=cgirelscriptptr();

		cookie_path += "/";
		cookie_path += list_name;

		cgi_set_cookie_url(&cookie_info, cookie_path.c_str());

		cgi_set_cookies(&cookie_info, 1);
		cgi_set_cookie_info_free(&cookie_info);
	}

	webmlmd::showhtmlform("webmlmlistindex.tmpl.html");
}

static void listrequest(std::string list_name,
			std::string path_info)
{
	std::vector<std::string>::iterator lb=mlm_dirs.begin(),
		le=mlm_dirs.end();

	while (lb != le)
	{
		std::string d=*lb++;

		if (webmlmd::basename(d) == list_name)
		{
			if (chdir(d.c_str()) < 0)
			{
				cginocache();
				std::cout << "Content-Type: text/plain"
					  << std::endl
					  << std::endl;
				perror(d.c_str());
				return;
			}
			listrequest2(list_name, path_info);
			return;
		}
	}

	cginocache();
	std::cout << "Content-Type: text/plain"
		  << std::endl
		  << std::endl
		  << "List " << list_name << " not found."
		  << std::endl;
	return;
}

static void main2(void *dummy)
{
	cgi_setup();

	std::string n=webmlmd::list_name();

	if (n.size() == 0)
	{
		webmlmd::showhtmlform(TEMPLATEDIR "/webmlmidx.html");
	}
	else
	{
		listrequest(n, list_cmd());
	}

	std::cout.flush();
}
