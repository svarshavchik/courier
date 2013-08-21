/*
** Copyright 2000-2013 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"config.h"
#include	"cmlm.h"
#include	"cmlmmoderate.h"
#include	"rfc2045/rfc2045.h"
#include	<fstream>
#include	<iterator>
#include	<algorithm>
#include	<fcntl.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<signal.h>
#include	<sysexits.h>
#include	<string.h>


// -----------------------------------------------------------------------
//
// Initial moderator notification.
//
// -----------------------------------------------------------------------

int sendinitmod(afxipipestream &i, const char *filename, const char *tpl)
{
	pid_t	p;
	std::string	boundary;
	const char *argv[6];
	std::string	token;

	argv[0]="sendmail";
	argv[1]="-f";
	argv[2]="";
	argv[3]="-N";
	argv[4]="fail";
	argv[5]=0;

	boundary=mkboundary_msg_s(i);
	if (boundary == "")
		return (EX_OSERR);

	i.clear();
	i.seekg(0);

	size_t	j;

	if (i.bad() || !std::getline(i, token).good()
	    || (j=token.find(':')) == token.npos)
	{
		perror("seek");
		return (EX_OSERR);
	}

	token=token.substr(j+1);
	TrimLeft(token);
	TrimRight(token);

	afxopipestream	ack(sendmail(argv, p));

	std::string	retaddr=get_verp_return("moderate");

	ack << "From: " << retaddr << std::endl
	    << "Reply-To: " << retaddr << std::endl
	    << "To: " << get_verp_return(std::string("owner")) << std::endl;

	{
		std::ifstream	ifs("modsubject.tmpl");
		std::string buf;

		while (std::getline(ifs, buf).good())
		{
			ack << buf << std::endl;
		}
	}

	ack << "MIME-Version: 1.0" << std::endl;
	ack << "Content-Type: multipart/mixed; boundary=\""
			<< boundary << "\"" << std::endl;
	ack << "Content-Transfer-Encoding: 8bit" << std::endl << std::endl;
	ack << "This is a MIME formatted message." << std::endl;
	ack << std::endl << "--" << boundary << std::endl;

	{
		std::ifstream	ifs(tpl);
		std::string buf;

		while (std::getline(ifs, buf).good())
		{
			ack << buf << std::endl;
		}
	}

	ack << "[" << filename << "]" << std::endl;
	ack << "[" << token << "]" << std::endl;

	ack << std::endl << "--" << boundary << std::endl;
	ack << "Content-Type: message/rfc822" << std::endl << std::endl;

	i.seekg(0);
	if (i.bad())
	{
		perror("seek");
		exit(EX_OSERR);
	}

	{
		char	buf[BUFSIZ];

		i.read(buf, sizeof(buf));

		int	x;

		while ((x=i.gcount()) > 0)
		{
			ack.write(buf, x);
			i.read(buf, sizeof(buf));
		}
		if (ack.bad() || i.bad())
		{
			perror("write");
			exit(EX_OSERR);
		}
	}



	ack << std::endl << "--" << boundary << "--" << std::endl;
	ack.flush();
	ack.close();
	return (wait4sendmail(p));
}

static int domodaccept(afxipipestream &, std::string);
static int domodreject(afxipipestream &, std::string);
static int domodbounce(afxipipestream &, std::string);
static int cmdmoderate(afxipipestream &);

static int doconvtoutf8_stdout(const char *ptr, size_t n, void *dummy)
{
	if (fwrite(ptr, n, 1, (FILE *)dummy) != 1)
		return -1;

	return 0;
}

// Process moderation response.

int cmdmoderate()
{
	std::string tmpfile_base=TMP "/" + mktmpfilename();
	std::string tmpfile1 = tmpfile_base + ".1";
	std::string tmpfile2 = tmpfile_base + ".2";

	FILE *f1=fopen(tmpfile1.c_str(), "w+");

	if (!f1)
	{
		perror(tmpfile1.c_str());
		return (EX_OSERR);
	}

	unlink(tmpfile1.c_str());

	FILE *f2=fopen(tmpfile2.c_str(), "w+");

	if (!f2)
	{
		fclose(f1);
		perror(tmpfile2.c_str());
		return (EX_OSERR);
	}

	unlink(tmpfile2.c_str());

	// Copy message from stdin to tmpfile1

	{
		int dup_fd=dup(fileno(f1));

		if (dup_fd < 0)
		{
			perror("dup");
			fclose(f1);
			fclose(f2);
		}

		afxiopipestream iopipe(dup_fd);

		std::copy(std::istreambuf_iterator<char>(std::cin),
			  std::istreambuf_iterator<char>(),
			  std::ostreambuf_iterator<char>(iopipe));
		iopipe.sync();
		iopipe.seekg(0);
	}

	// Convert message in tmpfile1 to utf8, into tmpfile2

	int rc=EX_SOFTWARE;

	struct rfc2045 *p=rfc2045_fromfp(f1);

	if (!p)
	{
		perror("rfc2045_from_fp");
	}
	else
	{
		struct rfc2045src *src=rfc2045src_init_fd(fileno(f1));

		fseek(f1, 0, SEEK_SET);

		if (!src)
		{
			perror("rfc2045src_init_fd");
		}
		else
		{
			struct rfc2045_decodemsgtoutf8_cb cb;

			memset(&cb, 0, sizeof(cb));

			cb.output_func=doconvtoutf8_stdout;
			cb.arg=f2;

			rc=rfc2045_decodemsgtoutf8(src, p, &cb);
			rfc2045src_deinit(src);
		}
		rfc2045_free(p);
	}
	fseek(f2, 0, SEEK_SET);

	if (rc == 0)
	{
		int dup_fd=dup(fileno(f2));

		rc=EX_SOFTWARE;

		if (dup_fd >= 0)
		{
			afxipipestream p(dup_fd);

			rc=cmdmoderate(p);
		}
	}
	fclose(f1);
	fclose(f2);
	return rc;
}

static int cmdmoderate(afxipipestream &tmpfile)
{
	std::string	buf, name, msg, filename, token;
	static int (*cmdfunc)(afxipipestream &, std::string);

	while (std::getline(tmpfile, buf).good())
	{
		size_t i;

		if (buf == "")
			break;
		i=buf.find(':');
		if (i == buf.npos)	continue;

		name=buf.substr(0, i);
		std::transform(name.begin(), name.end(),
			       name.begin(), std::ptr_fun(::tolower));
		if (name != "subject")	continue;

		buf=buf.substr(i+1);
		TrimLeft(buf);

		cmdfunc=domodaccept;

		if (strncasecmp(buf.c_str(), "no", 2) == 0)
			cmdfunc=domodreject;
		else if (strncasecmp(buf.c_str(), "reject", 6) == 0)
			cmdfunc=domodbounce;

		while (std::getline(tmpfile, buf).good())
			if (buf == "")	break;

		static const char cuthere[]="==CUT HERE==";

		while (std::getline(tmpfile, buf).good())
			if (std::search(buf.begin(), buf.end(),
					cuthere, cuthere+sizeof(cuthere)-1)
			    != buf.end())
				break;

		msg="";

		while (std::getline(tmpfile, buf).good())
		{
			if (std::search(buf.begin(), buf.end(),
					cuthere, cuthere+sizeof(cuthere)-1)
			    != buf.end())
				break;

			if (buf.size() + msg.size() > 50000)
				continue;
			if (buf.substr(0, 1) == ">")	continue;
			msg += buf;
			msg += '\n';
		}

		filename="";
		token="";
		while (std::getline(tmpfile, buf).good())
		{
			i=buf.find('[');
			if (i == buf.npos)	continue;
			buf=buf.substr(i+1);
			i=buf.find(']');
			if (i == buf.npos)	continue;
			filename=buf.substr(0, i);
			break;
		}

		while (std::getline(tmpfile, buf).good())
		{
			i=buf.find('[');
			if (i == buf.npos)	continue;
			buf=buf.substr(i+1);
			i=buf.find(']');
			if (i == buf.npos)	continue;
			token=buf.substr(0, i);
			break;
		}

		if (token == "")	break;

		if (filename.find('/') != filename.npos)
			break;	/* Script kiddy */

		filename = MODQUEUE "/" + filename;

		{
			ExclusiveLock modqueue_lock(MODQUEUELOCKFILE);
			int i_fd=open(filename.c_str(), O_RDONLY);

			if (i_fd < 0)
			{
				perror(filename.c_str());
				exit(EX_OSERR);
			}

			afxipipestream ifs(i_fd);

			int	rc;

			if (!std::getline(ifs, buf).good()) break;

			i=buf.find(':');
			if (i == buf.npos)	break;
			buf=buf.substr(i+1);
			TrimLeft(buf);

			if (buf != token)	break;

			rc=(*cmdfunc)(ifs, msg);
			ifs.close();
			if (rc == 0)
				unlink(filename.c_str());
			return (rc);
		}
	}

	std::cerr << "Invalid moderation message." << std::endl;
	return (EX_SOFTWARE);
}

static int docopy_noseek(std::istream &i, std::ostream &o)
{
	char	buf[BUFSIZ];

	i.read(buf, sizeof(buf));

	int	x;

	while ((x=i.gcount()) > 0)
	{
		o.write(buf, x);
		i.read(buf, sizeof(buf));
	}
	if (o.bad() || i.bad())
		return (EX_OSERR);
	return (0);
}

static int domodaccept(afxipipestream &i, std::string s)
{
	return (postmsg(i, docopy_noseek));
}

static int domodreject(afxipipestream &i, std::string s)
{
	// Nothing - silently discard it.

	return (0);
}

static int domodbounce(afxipipestream &i,std::string s)
{
	std::string	buf;
	std::string	from;
	std::string	replyto;

	{
		std::string	headerbuf="";

		while (std::getline(i, buf).good())
		{
			if (buf == "")	break;
			headerbuf += buf;
			headerbuf += '\n';
		}

		from=header_s(headerbuf, "from");
		replyto=header_s(headerbuf, "replyto");
	}

	std::string	boundary=mkboundary_msg_s(i);

	if (replyto.size())	from=replyto;

const char	*argv[6];

	argv[0]="sendmail";
	argv[1]="-f";

	std::string	me=get_verp_return("owner");

	argv[2]=me.c_str();
	argv[3]="-N";
	argv[4]="fail";
	argv[5]=0;

	pid_t	p;
	afxopipestream	ack(sendmail(argv, p));

	ack << "From: " << me << std::endl
		<< "Reply-To: " << me << std::endl
		<< "To: " << from << std::endl
		<< "Mime-Version: 1.0" << std::endl
		<< "Content-Type: multipart/mixed; boundary=\"" << boundary <<
			"\"" << std::endl
		<< "Content-Transfer-Encoding: 8bit" << std::endl;

	copy_template(ack, "modrejheader.tmpl", "");

	ack << std::endl;
	ack << "This is a MIME formatted message." << std::endl;
	ack << std::endl << "--" << boundary << std::endl;
	copy_template(ack, "modrejbody.tmpl", "");
	ack << s << std::endl << "--" << boundary << std::endl;
	ack << "Content-Type: message/rfc822" << std::endl << std::endl;

	i.seekg(0);
	if (i.bad())
	{
		perror("seek");
		return (EX_OSERR);
	}
	std::getline(i, buf); // Magic-Token: header

int	rc=docopy_noseek(i, ack);

	ack << std::endl << "--" << boundary << "--" << std::endl;

	ack.flush();
	if (ack.bad())
	{
		kill(p, SIGTERM);
		wait4sendmail(p);
		return (EX_OSERR);
	}

	ack.close();
int	rc2=wait4sendmail(p);

	if (rc2)	rc=rc2;
	return (rc);
}
