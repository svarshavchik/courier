/*
**
** Copyright 2007 Double Precision, Inc.
** See COPYING for distribution information.
*/
#include	"config.h"
#include	"webmlmd.H"
#include	"webmlmdhandlers.H"
#include	"webmlmdcmlm.H"

#include	"cmlm.h"
#include	"cmlmsublist.h"
#include	<iostream>
#include	<fstream>
#include	<iomanip>
#include	<sstream>
#include	<cctype>
#include	<algorithm>

#include	"mydirent.h"

#include "cgi/cgi.h"
#include "rfc2045/rfc2045.h"
#include <stdio.h>

extern "C" {
#include "sqwebmail/msg2html.h"
}

static int cmpmodfilenames(std::string a, std::string b)
{
	time_t ta=0;
	time_t tb=0;

	std::istringstream(a) >> ta;
	std::istringstream(b) >> tb;

	if (ta < tb)
		return -1;

	if (ta > tb)
		return 1;

	if (a < b)
		return -1;

	if (a > b)
		return 1;
	return 0;
}

static std::string get_mod_token(std::string n)
{
	std::string f= MODQUEUE "/" + n;

	std::ifstream i(f.c_str());

	if (!i.is_open())
		return "";

	std::string l;

	std::getline(i, l);

	if (l.substr(0, 14) != "X-Magic-Token:")
		return "";

	return std::string(std::find_if(l.begin()+14, l.end(),
					std::not1(std::ptr_fun(isspace))),
			   l.end());
}

std::string webmlmd::get_next_mod_filename(std::string curfilename,
					   size_t &mod_count)
{
	std::string filename;

	DIR *dirp=opendir(MODQUEUE);
	struct dirent *de;

	mod_count=0;

	bool checkdigestdir=false;

	while (dirp && (de=readdir(dirp)))
	{
		if (de->d_name[0] == '.')
			continue;

		++mod_count;

		std::string n=de->d_name;

		if (curfilename != "" && cmpmodfilenames(n, curfilename) <= 0)
			continue;

		if (filename == "" || cmpmodfilenames(n, filename) < 0)
		{
			filename=n;

			if (checkdigestdir)
				continue;

			if (get_mod_token(n) == "")
			{
				closedir(dirp);
				mod_count=0;
				return ""; // This is a mailing list digest
			}

			checkdigestdir=true;
		}
	}
	if (dirp)
		closedir(dirp);

	return filename;
}

HANDLER("MODURL", emit_modlink)
{
	size_t cnt=0;

	webmlmd::get_next_mod_filename("", cnt);

	if (cnt == 0 || args.empty())
		return;

	std::string msgtxt=args.front();

	size_t p=msgtxt.find('@');

	if (p != msgtxt.npos)
	{
		std::ostringstream o;

		o << msgtxt.substr(0, p) << cnt <<
			msgtxt.substr(p+1);

		msgtxt=o.str();
	}

	std::cout << "&nbsp;&nbsp;|&nbsp;&nbsp;<a href=\"";

	emit_list_url();
	std::cout << "/admin/mod\">" << msgtxt << "</a>";
}

static std::string findparam(std::list<std::string> &args, std::string name)
{
	std::list<std::string>::iterator b=args.begin(), e=args.end();

	while (b != e)
	{
		if (b->substr(0, name.size()+1) == name + "=")
			return b->substr(name.size()+1);
		++b;
	}
	return "";
}

static void unknown_attachment_action(struct rfc2045id *idptr,
				      const char *content_type,
				      const char *content_name,
				      off_t size,
				      void *arg)
{
	std::list<std::string> *argsp=(std::list<std::string> *)arg;

	std::string content_type_s(content_type);

	std::replace(content_type_s.begin(),
		     content_type_s.end(), '<', ' ');

	printf("<blockquote><p>%s (%s; %ldkb)</p></blockquote>\n",
	       findparam(*argsp, "ATTACHNOTICE").c_str(),
	       content_type_s.c_str(),
	       (long)((size+512)/1024L));
	       
}

static char *get_textlink(const char *url, void *arg)
{
	return strdup(url ? url:"");
}

static void inline_image_action(struct rfc2045id *idptr,
				const char *content_type,
				void *arg)
{
	fflush(stdout);

	std::cout << "<img src=\"";
	webmlmd::emit_list_url();
	std::cout << "admin/modmimefetch?msgname="
		  << cgi("msgname")
		  << std::flush;
	msg2html_showmimeid(idptr, NULL);

	printf("\" />");
}

static char *get_url_to_mime_part(const char *mimeid,
				  void *arg)
{
	std::ostringstream o;

	o << cgirelscriptptr() << "/" << webmlmd::list_name()
	  << "/admin/modmimefetch?msgname=" << cgi("msgname")
	  << "&mimeid=" << mimeid;

	return strdup(o.str().c_str());
}


HANDLER("MODMSG", emit_modmsg)
{
	size_t cnt=0;
	std::string filename=cgi("msgname");

	if (filename.find('/') != filename.npos)
		filename="";

	if (filename == "")
		filename=webmlmd::get_next_mod_filename(filename, cnt);

	if (filename == "")
		return;

	cgi_put("msgname", filename.c_str());

	std::cout << "<input type='hidden' name='msgname' value='"
		  << filename << "' />";

	std::string modfilename=MODQUEUE "/" + filename;

	FILE *fp=fopen(modfilename.c_str(), "r");

	if (!fp)
		return;

	struct rfc2045 *rfc=rfc2045_fromfp(fp);

	if (!rfc)
	{
		fclose(fp);
		return;
	}

	fseek(fp, 0L, SEEK_END);
	fseek(fp, 0L, SEEK_SET);

	struct msg2html_info *msg2htmlp=msg2html_alloc("utf-8");

	if (!msg2htmlp)
	{
		rfc2045_free(rfc);
		fclose(fp);
		return;
	}

	std::cout << std::flush;

	msg2htmlp->arg=&args;
	msg2htmlp->showhtml=1;

	msg2htmlp->get_url_to_mime_part=get_url_to_mime_part;
	msg2htmlp->unknown_attachment_action=unknown_attachment_action;
	msg2htmlp->inline_image_action=inline_image_action;
	msg2htmlp->get_textlink=get_textlink;
	msg2html(fp, rfc, msg2htmlp);
	msg2html_free(msg2htmlp);
	rfc2045_free(rfc);
	fclose(fp);
}

std::string webmlmd::do_mod_accept(std::string filename)
{
	std::string token=get_mod_token(filename);

	if (token == "")
		return "";

	webmlmd::cmlm ctlmsg;

	if (ctlmsg.start("moderate", "", "ctlmsg"))
	{
		FILE *stdinptr=ctlmsg.stdinptr();

		ctlmsg.mk_received_header();
		fprintf(stdinptr, "Subject: yes\n\n"
			"==CUT HERE==\n\n==CUT HERE==\n\n"
			"[%s]\n"
			"[%s]\n",
			filename.c_str(), token.c_str());

		if (ctlmsg.wait())
			return "";
	}

	return ctlmsg.format_error_message();
}

std::string webmlmd::do_mod_reject(std::string filename,
				   bool returntosender,
				   std::string msg)
{
	std::string token=get_mod_token(filename);

	if (token == "")
		return "";

	webmlmd::cmlm ctlmsg;

	// Reformat message to prepend a space to each line, to avoid
	// clashing with format characters.

	std::istringstream i(msg + "\n");

	std::ostringstream o;

	std::string l;

	while (std::getline(i, l).good())
	{
		o << " " << l << std::endl;
	}

	if (ctlmsg.start("moderate", "", "ctlmsg"))
	{
		FILE *stdinptr=ctlmsg.stdinptr();

		ctlmsg.mk_received_header();
		fprintf(stdinptr, "Subject: %s\n\n"
			"==CUT HERE==\n\n%s==CUT HERE==\n\n"
			"[%s]\n"
			"[%s]\n",
			returntosender ? "reject":"no",
			o.str().c_str(),
			filename.c_str(), token.c_str());

		if (ctlmsg.wait())
			return "";
	}

	return ctlmsg.format_error_message();
}

