/*
** Copyright 2000-2009 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"config.h"

#include	"afx/afx.h"
#include	"rfc822/rfc822.h"
#include	"random128/random128.h"
#include	"numlib/numlib.h"
#include	"dbobj.h"

#if	HAVE_LOCALE_H
#include	<locale.h>
#endif

#include	<iostream>
#include	<sstream>
#include	<iomanip>
#include	<fstream>
#include	<signal.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/wait.h>
#include	<fcntl.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#include	<errno.h>
#include	<ctype.h>
#include	<time.h>
#include	"mydirent.h"
#include	<sysexits.h>

#include "cmlm.h"
#include "cmlmsublist.h"
#include "cmlmsubunsub.h"
#include "cmlmsubunsubmsg.h"
#include "cmlmcmdmisc.h"
#include "cmlmstartmail.h"
#include "cmlmarchive.h"
#include "cmlmmoderate.h"
#include "cmlmbounce.h"
#include "cmlmcleanup.h"
#include "cmlmfetch.h"

#include <string>
#include <vector>
#include <set>

extern "C" void rfc2045_error(const char *errmsg)
{
        std::cerr << errmsg << std::endl;
        exit(EX_SOFTWARE);
}

//	Keywords we try to trap for messages sent to the list address.


static const char *admin_keywords[]={
	"subscribe",
	"unsubscribe",
	"help",
	0
	};

static void usage()
{
	std::cerr << "Usage: cmlm [command] [directory] [options]..." << std::endl;
	exit(EX_TEMPFAIL);
}


static int cmdctlmsg(const std::vector<std::string> &);
static int cmdmsg(const std::vector<std::string> &);

static struct ncmdtab {
	const char *cmdname;
	int (*cmdfunc)(const std::vector<std::string> &);
} ncommands[]= {
	{"create", cmdcreate},
	{"update", cmdupdate},
	{"ctlmsg", cmdctlmsg},
	{"msg", cmdmsg},
	{"set", cmdset},
	{"sub", cmdsub},
	{"unsub", cmdunsub},
	{"lsub", cmdlsub},
	{"export", cmdexport},
	{"import", cmdimport},
	{"laliases", cmdlaliases},
	{"info", cmdinfo},
	{"hourly", cmdhourly},
	{"daily", cmddaily},
	{"digest", cmddigest},
};

int main(int argc, char **argv)
{
	if (argc < 3)	usage();

	setlocale(LC_ALL, "");

	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, SIG_DFL);

const char *cmd=argv[1];
const char *dir=argv[2];

	if (strcmp(cmd, "create") == 0)
	{
	int	i;

		for (i=3; i<argc; i++)
			if (strncmp(argv[i], "ADDRESS=", 8) == 0)
				break;

		if (i >= argc)
		{
			std::cerr << "Missing ADDRESS." << std::endl;
			return (EX_SOFTWARE);
		}

		if (mkdir(dir, 0700))
		{
			perror(dir);
			exit(EX_OSERR);
		}
	}

	if (chdir(dir))
	{
		perror(dir);
		exit(EX_SOFTWARE);
	}

unsigned	i;

	std::vector<std::string> argv_cpy;

	argv_cpy.insert(argv_cpy.end(), argv+3, argv+argc);
	for (i=0; i<sizeof(ncommands)/sizeof(ncommands[0]); i++)
		if (strcmp(ncommands[i].cmdname, cmd) == 0)
			exit ( (*ncommands[i].cmdfunc)(argv_cpy));


	usage();
	return (0);
}

////////////////////////////////////////////////////////////////////////////
//
// Control message
//

static int help();

static int cmdctlmsg(const std::vector<std::string> &)
{
const char *cmd=getenv("DEFAULT");

	if (!cmd)	cmd="";

	if (strcasecmp(cmd, "help") == 0)
		return (help());

	if (strncasecmp(cmd, "subscribe", 9) == 0)
	{
		cmd += 9;
		if (*cmd == 0 || (*cmd == '-' && *++cmd != 0))
			return (dosub(cmd));
	}
	else if (strncasecmp(cmd, "alias-subscribe", 15) == 0)
	{
		cmd += 15;
		if (*cmd == 0 || (*cmd == '-' && *++cmd != 0))
			return (doalias(cmd));
	}
	else if (strncasecmp(cmd, "modsubconfirm-", 14) == 0)
	{
		cmd += 14;
		if (*cmd)
			return (domodsub(cmd));
	}
	else if (strncasecmp(cmd, "unsubscribe", 11) == 0)
	{
		cmd += 11;
		if (*cmd == 0 || (*cmd == '-' && *++cmd != 0))
			return (dounsub(cmd));
	}
	else if (strncasecmp(cmd, "subconfirm-", 11) == 0)
	{
		cmd += 11;
		if (*cmd)
			return (dosubunsubconfirm(cmd, "sub",
					docmdsub,
					"sub2.tmpl",
					"sub3.tmpl", 0));
	}
	else if (strncasecmp(cmd, "aliasconfirm-", 13) == 0)
	{
		cmd += 13;
		if (*cmd)
		{
		int rc=dosubunsubconfirm(cmd, "alias",
					docmdalias,
					"sub2.tmpl",
					"sub3.tmpl", 1);
			if (rc == 9)
				rc=EX_SOFTWARE;
			return (rc);
		}
	}
	else if (strncasecmp(cmd, "bounce-", 7) == 0)
	{
		cmd += 7;
		if (*cmd)
			return (dobounce(cmd));
	}
	else if (strncasecmp(cmd, "bounce1-", 8) == 0)
	{
		if (*cmd)
			return (dobounce1(cmd));
	}
	else if (strncasecmp(cmd, "bounce2-", 8) == 0)
	{
		if (*cmd)
			return (dobounce2(cmd));
	}
	else if (strncasecmp(cmd, "unsubconfirm-", 13) == 0)
	{
		cmd += 13;
		if (*cmd)
			return (dosubunsubconfirm(cmd, "unsub",
						  docmdunsub,
						  "unsub2.tmpl",
						  "unsub3.tmpl", 0));
	}
	else if (strcasecmp(cmd, "moderate") == 0)
	{
		return (cmdmoderate());
	}
	else if (strncasecmp(cmd, "index", 5) == 0)
	{
		cmd += 5;
		if (*cmd == 0 || *cmd++ == '-')
			return (doindex(cmd));
	}
	else if (strncasecmp(cmd, "fetch-", 6) == 0)
	{
		cmd += 6;
		if (*cmd)
			return (dofetch(cmd));
	}
	std::cerr << "Invalid address.\n";
	return (EX_SOFTWARE);
}

//////////////////////////////////////////////////////////////////////////
//
// Send back a template, followed by copy of the original headers
//
//////////////////////////////////////////////////////////////////////////

void ack_template(std::ostream &fs, const char *filename,
		  std::string webcmd, std::string msg)
{
	fs << "Mime-Version: 1.0" << std::endl
	   << "Content-Type: multipart/mixed; boundary=courier_mlm_bound"
	   << std::endl
	   << std::endl
	   << std::endl
	   << "--courier_mlm_bound"
	   << std::endl;

	copy_template(fs, filename, webcmd);

	fs << std::endl << "--courier_mlm_bound" << std::endl
	   << "Content-Type: text/rfc822-headers; charset=iso-8859-1"
	   << std::endl
	   << "Content-Transfer-Encoding: 8bit"
	   << std::endl
	   << std::endl;

	std::istringstream i(msg);
	std::string buf;

	while (std::getline(i, buf).good())
	{
		if (buf == "")
			break;

		fs << buf << std::endl;
	}

	fs << std::endl << "--courier_mlm_bound--" << std::endl;
}

void simple_template(std::ostream &fs, const char *filename, std::string msg)
{
	copy_template(fs, filename, "");

	fs << MSGSEPARATOR << std::endl << std::endl;

	std::istringstream i(msg);
	std::string buf;

	while (std::getline(i, buf).good())
	{
		if (buf == "")
			break;

		fs << buf << std::endl;
	}
}

void copy_template(std::ostream &fs, const char *filename,
		   std::string webcmd)
{
	std::ifstream	ifs(filename);
	std::string buf;

	if (!ifs.is_open())
	{
		perror(filename);
		exit(EX_OSERR);
	}

	while (std::getline(ifs, buf).good())
	{
		std::string::iterator b=buf.begin(), e=buf.end(), p=b;

		while ((p=std::find(p, e, '[')) != e)
		{
			std::string::iterator q=p;
			size_t offset=q-b;


			p=std::find(p, e, ']');

			if (p != e)
				++p;

			std::string token(q, p);

			std::string repl;

			if (token == "[url]")
			{
				repl=cmdget_s("URL") + "/" + webcmd;
			}
			else
			{
				if (webcmd != "" && cmdget_s("URL") != "")
					copy_template(fs,
						      token.substr(1,
								   token.size()
								   -2)
						      .c_str(),
						      webcmd);
			}

			buf=std::string(b, q)
				+ repl
				+ std::string(p, e);

			b=buf.begin();
			e=buf.end();
			p=b + offset + repl.size();
		}

		fs << buf << std::endl;
	}
}


//
//  Determine if this is a good confirm.
//
bool checkconfirm(std::string msg)
{
	std::string subject=header_s(msg, "subject");

	static const char yes[]="yes";

	if (cmdget_s("SIMPLECONFIRM") == "1")
		return (true);

	if (std::search(subject.begin(), subject.end(),
			yes, yes+sizeof(yes)-1) == subject.end())
		return false;

	return true;
}

/////////////////////////////////////////////////////////////////////////////
//
// Distribute a message.
//
/////////////////////////////////////////////////////////////////////////////

static const char *fn;
static const char *fn2;

static RETSIGTYPE sighandler(int n)
{
	unlink(fn);
	if (fn2)	unlink(fn2);
	_exit(EX_OSERR);
#if	RETSIGTYPE != void
	return (0);
#endif
}

void trapsigs(const char *p)
{
	fn=p;
	fn2=0;
	signal(SIGINT, sighandler);
	signal(SIGHUP, sighandler);
	signal(SIGTERM, sighandler);
	signal(SIGPIPE, SIG_IGN);
}

static void trapsig2(const char *p)
{
	fn2=p;
}

void clearsigs(int rc)
{
	if (rc)
	{
		unlink(fn);
		if (fn2)	unlink(fn2);
	}

	signal(SIGINT, SIG_DFL);
	signal(SIGHUP, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
}

static int savemsg(std::istream &, std::ostream &);

static int cmdmsg(const std::vector<std::string> &argv)
{
	std::string postoptions= cmdget_s("POST");

	if (postoptions == "mod")
	{
		int	rc=0;

		//
		//	Insert into the moderators queue
		//

		std::string modfilename=mktmpfilename();
		std::string tmodname=TMP "/" + modfilename;
		std::string mmodname=MODQUEUE "/" + modfilename;

		trapsigs(tmodname.c_str());

		std::fstream	modfs(tmodname.c_str(),
				      std::ios::in | std::ios::out | std::ios::trunc);

		if (!modfs.is_open())
		{
			perror(tmodname.c_str());
			rc=EX_TEMPFAIL;
		}

		const char *token=random128_alpha();

		if (rc == 0)
		{
			modfs << "X-Magic-Token: " << token << std::endl;
			rc=savemsg(std::cin, modfs);
		}

		ExclusiveLock modqueue_lock(MODQUEUELOCKFILE);

		if (rc || rename(tmodname.c_str(), mmodname.c_str()))
		{
			modfs.close();
			clearsigs(rc);
			return (rc);
		}

		clearsigs(0);
		modfs.close();

		int fd_msg=open(mmodname.c_str(), O_RDONLY);
		if (fd_msg < 0)
		{
			perror(mmodname.c_str());
			exit(EX_OSERR);
		}

		afxipipestream ifs(fd_msg);

		rc=sendinitmod(ifs, modfilename.c_str(), "modtext.tmpl");
		return (rc);
	}
	return (postmsg(std::cin, savemsg));
}

//----------------------------------------------------------------------------
//
// The postmsg() function posts a message to the mailing list.  postmsg()
// is used to post messages to both unmoderated and moderated mailing lists.
// For a moderated mailing list, postmsg() is called to post a moderator
// approved message.  postmsg()'s behavior is controlled by passing it two
// arguments:
//
// msgsource - the source of the message.  For unmoderated mailing lists,
// this is cin/stdin, the message as its being delivered directly to
// couriermlm. For moderated mailing list, this is an opened file in
// modqueue, that the moderator just approved.
//
// savefunc - a function that copies a message from an input source (the
// msgsource argument), to an output stream, transforming the message for
// mailing list posting.  For unmoderated mailing lists, this is savemsg(),
// which removed and adds headers based upon the headerdel/headeradd control
// files.  For a moderated list this is a stub function that simply copies
// an input stream to an output stream (that's because savemsg() was already
// called previously when the message was inserted into the moderator queue.
//
// In both cases, savefunc *must* call flush on the output stream!
//
// postmsg() allocates a message sequence number in the archive directory,
// calls savefunc() to save the message in the archive directory, then
// repeatedly mails the message to everyone on the mailing list.
//
//----------------------------------------------------------------------------

int postmsg(std::istream &msgsource, int (*savefunc)(std::istream &, std::ostream &))
{
char	buf[NUMBUFSIZE+100], buf2[NUMBUFSIZE+100];
unsigned long nextseqno;
int	rc;
std::fstream fs;

{
Archive archive;

	if ( (rc=archive.get_seq_no(nextseqno)) != 0)
		return (rc);

	libmail_str_off_t(nextseqno, buf);

	if ( (rc=archive.save_seq_no()) != 0)
		return (rc);

	std::string filename_str(archive.filename(nextseqno));

	trapsigs(filename_str.c_str());

	fs.open(filename_str.c_str(),
		std::ios::in | std::ios::out | std::ios::trunc);
	if (!fs.is_open())
	{
		perror(filename_str.c_str());
		rc=EX_TEMPFAIL;
	}
	else	rc=(*savefunc)(msgsource, fs);

	if (rc == 0)
	{
		if (rename(NEXTSEQNO, SEQNO))
		{
			perror(SEQNO);
			rc=EX_TEMPFAIL;
		}
	}

	std::string digestdir;

	if (rc == 0)
	{
		digestdir=cmdget_s("DIGEST");
		if (digestdir.size() > 0)
		{
			digestdir += "/" MODQUEUE "/";
			digestdir += buf;
			trapsig2(digestdir.c_str());
			if ((rc=link(filename_str.c_str(),
				     digestdir.c_str())) != 0)
				perror(digestdir.c_str());
		}
	}

	clearsigs(rc);
}
	if (rc == 0)
	{
		strcpy(buf2, "bounce-");
		strcat(buf2, buf);
		post(fs, buf2);
	}

	fs.close();
	return (rc);
}

static void get_post_address(const char *p, std::string &addr)
{
	if (strncmp(p, "x-couriermlm-date:", 18) == 0)
	{
		p += 18;
		while (*p && *p != '\n' && isspace((int)(unsigned char)*p))
			++p;
		while (isdigit((int)(unsigned char)*p))
		{
			++p;
		}
		while (*p && *p != '\n' && isspace((int)(unsigned char)*p))
			++p;

		size_t i;
		bool atfound=false;

		for (i=0; p[i] && p[i] != '\n'; i++)
			if (p[i] == '@')
				atfound=true;

		if (atfound)
			addr=std::string(p, p+i);
	}
}

void post(std::istream &fs, const char *verp_ret)
{
	SubscriberList	sublist;
	StartMail	mail(fs, verp_ret);
	std::string	addr, lastaddr;

	if ( sublist.Next(addr) )
	{
		get_post_address(sublist.sub_info.c_str(), addr);
		mail.To(addr);
		lastaddr=addr;

		// Call Send() when we change domains, VERP works better
		// that way.

		while (sublist.Next(addr))
		{
			const char *a=strrchr(addr.c_str(), '@');
			const char *b=strrchr(lastaddr.c_str(), '@');

			if (!a || !b || strcmp(a, b))
				mail.Send();

			get_post_address(sublist.sub_info.c_str(), addr);
			mail.To(addr);
			lastaddr=addr;
		}
	}
	mail.Send();
}

static int savemsg(std::istream &msgs, std::ostream &fs)
{
	const char *dt=getenv("DTLINE");
	std::string buf;

	int	n;
	std::string keyword= cmdget_s("KEYWORD"), keywords;
	int	adminrequest=0;

	if (keyword.size() > 0)
		keywords="["+keyword+"]";

	if (dt && *dt)
		fs << dt << std::endl;

	{
		std::ifstream ifs(HEADERADD);

		if (ifs.is_open())
		{
			std::string line;

			while (std::getline(ifs, line).good())
				fs << line << std::endl;
		}
	}

	std::set<std::string> delete_headers;

	{
		std::ifstream ifs(HEADERDEL);

		std::string header_name;

		if (ifs.is_open())
			while (!std::getline(ifs, header_name).eof())
			{
				std::transform(header_name.begin(),
					       header_name.end(),
					       header_name.begin(),
					       std::ptr_fun(::tolower));

				TrimLeft(header_name);
				TrimRight(header_name);

				delete_headers.insert(header_name);
			}
	}

	std::string h, from;
	bool dodelete=false;

	while (std::getline(msgs, buf).good())
	{
		if (buf.size() == 0)
			break;

		size_t colon_pos=buf.find(':');

		if (isspace((int)(unsigned char)*buf.c_str()))
			colon_pos=buf.npos;

		if (colon_pos == buf.npos)
		{
			if (h == "from:")
			{
				from += " ";
				from += buf;
			}

			if (!dodelete)
				fs << buf << std::endl;
			continue;
		}

		h=buf.substr(0, ++colon_pos);
		std::transform(h.begin(), h.end(), h.begin(),
			       std::ptr_fun(::tolower));

		if (h == "subject:")
		{
			std::string firstword=buf.substr(colon_pos);
			TrimLeft(firstword);

			firstword=
				std::string(firstword.begin(),
					    std::find_if(firstword
							 .begin(),
							 firstword
							 .end(),
							 std::not1
							 (std::ptr_fun
							  (::isalpha)))
					    );


			std::transform(firstword.begin(),
				       firstword.end(),
				       firstword.begin(),
				       std::ptr_fun(::tolower));

			size_t i;

			for (i=0; admin_keywords[i]; i++)
			{
				if (firstword == admin_keywords[i])
				{
					adminrequest=1;
					break;
				}
			}
		}

		if (h == "subject:" && keyword.size() > 0)
		{

			std::string keyword_br="[" + keyword + "]";

			if (std::search(buf.begin(), buf.end(),
					keyword_br.begin(),
					keyword_br.end()) ==
			    buf.end())
			{

				buf=buf.substr(0, colon_pos) + " "
					+ keyword_br
					+ buf.substr(colon_pos);
			}
		}

		dodelete=delete_headers.find(h) != delete_headers.end();

		if (h == "from:")
			from=buf.substr(colon_pos);

		if (!dodelete)
			fs << buf << std::endl;
	}

	fs << "\n";


	struct rfc822t *t=rfc822t_alloc_new(from.c_str(), 0, 0);

	if (!t)
	{
		perror("malloc");
		return (EX_TEMPFAIL);
	}

	struct rfc822a *a=rfc822a_alloc(t);

	if (!a)
	{
		rfc822t_free(t);
		perror("malloc");
		return (EX_TEMPFAIL);
	}

	char *nn=0;

	if (a->naddrs > 0)
	{
		nn=rfc822_getaddr(a, 0);
		if (!nn)
		{
			rfc822a_free(a);
			rfc822t_free(t);
			perror("malloc");
			return (EX_TEMPFAIL);
		}
	}
	rfc822a_free(a);
	rfc822t_free(t);
	if (nn)
	{
		from=nn;
		free(nn);
	}
	else	from="";


	int	first_line=1;

	while (std::getline(msgs, buf).good())
	{
		fs << buf << std::endl;

		// Check for "subscribe/unsubscribe on the first line" wankers.

		if (!first_line)	continue;

		std::string firstword(buf.begin(),
				      std::find_if(buf.begin(),
						   buf.end(),
						   std::not1(std::ptr_fun
							     (::isalpha))));

		std::transform(firstword.begin(), firstword.end(),
			       firstword.begin(), std::ptr_fun(::tolower));

		for (n=0; admin_keywords[n]; n++)
		{
			if (firstword == admin_keywords[n])
				adminrequest=1;
		}

		if (buf.size() > 0)
			first_line=0;
	}

	fs.flush();
	fs.seekp(0);
	if (fs.bad() || fs.fail())
	{
		perror(	"write" );
		return (EX_TEMPFAIL);
	}

	if ( cmdget_s("NOBOZOS") == "0")
		adminrequest=0;

	if (adminrequest)
	{
		std::ifstream ifs("adminrequest.tmpl");
		int	flag=0;

		if (ifs.is_open())
		{
			while (std::getline(ifs, buf).good())
			{
				std::cout << buf << std::endl;
				flag=1;
			}
		}

		if (!flag)	std::cout << "Administractive request blocked."
					  << std::endl;
		std::cout << std::flush;
		return (EX_SOFTWARE);
	}


	int	rc=is_subscriber(from.c_str());

	if (rc == EX_NOUSER)
	{
		std::string postoptions= cmdget_s("POST");

		if (postoptions == "all")
			rc=0;
	}

	if (rc)
	{
		if (rc == EX_NOUSER)
		{
			std::cout << "You are not subscribed to this mailing list."
				  << std::endl;
		}
		rc=EX_SOFTWARE;
		return (rc);
	}

	return (0);
}

void copy_report(const char *s, afxopipestream &o)
{
	int i_fd=open(s, O_RDONLY);

	if (i_fd < 0)
	{
		perror(s);
		exit(EX_OSERR);
	}

	afxipipestream i(i_fd);

	if (copyio_noseek(i, o) < 0)
	{
		perror(s);
		exit(EX_OSERR);
	}
}

int copyio(afxipipestream &i, afxopipestream &o)
{
	i.seekg(0);
	if (i.bad())
	{
		perror("seek");
		return (EX_OSERR);
	}

	return (copyio_noseek(i, o));
}

int copyio_noseek(afxipipestream &i, afxopipestream &o)
{
	return (copyio_noseek_cnt(i, o, 0));
}

int copyio_noseek_cnt(afxipipestream &i, afxopipestream &o,
		      unsigned long *maxbytes)
{
char	buf[BUFSIZ];

	i.read(buf, sizeof(buf));

int	x;

	while ((x=i.gcount()) > 0)
	{
		if (maxbytes)
		{
			if ((unsigned long)x > *maxbytes)
			{
				std::cerr << "Message too large." << std::endl;
				return (EX_SOFTWARE);
			}
			*maxbytes -= x;
		}

		o.write(buf, x);
		i.read(buf, sizeof(buf));
	}
	if (o.bad() || i.bad())
		return (EX_OSERR);
	return (0);
}

static int tryboundary(afxipipestream &io, const char *boundary)
{
	int	boundary_l=strlen(boundary);
	std::string line;

	io.clear();
	io.seekg(0);
	if (io.fail())	return (-1);

	while (std::getline(io, line).good())
	{
		if (strncmp(line.c_str(), boundary, boundary_l) == 0)
			return (1);
	}
	io.clear();
	return (0);
}

// Create MIME multipart boundary delimiter.

std::string mkboundary_msg_s(afxipipestream &io)
{
	std::string	base=mkfilename();
	unsigned i=0;
	std::string boundary;
	int	rc;

	do
	{
		std::ostringstream o;

		o << base << "." << std::setw(3) << std::setfill('0')
		  << i++;

		boundary=o.str();

	} while ((rc=tryboundary(io, boundary.c_str())) > 0);
	if (rc)	return ("");
	return (boundary);
}

static int help()
{
	std::string	msg(readmsg());
	std::string	addr=returnaddr(msg, "");

	pid_t	p;

	if (addr.find('@') == addr.npos)	return (0);

	time_t	curtime;

	time(&curtime);

	addrlower(addr);

	std::string vaddr= TMP "/help." + toverp(addr);

	struct	stat	sb;

	if (stat(vaddr.c_str(), &sb) == 0 && sb.st_mtime + 30 * 60 > curtime)
		return (0);

	close(open(vaddr.c_str(), O_RDWR|O_CREAT, 0755));

	std::string owner=get_verp_return("owner");

	int	nodsn= (cmdget_s("NODSN") == "1");
	afxopipestream ack(sendmail_bcc(p, owner, nodsn));

	ack << "From: " << myname() << " <" << owner << ">" << std::endl
	    << "To: " << addr << std::endl
	    << "Bcc: " << addr << std::endl;

	simple_template(ack, "help.tmpl", msg);
	ack.close();
	return (wait4sendmail(p));
}
