/*
** Copyright 2000-2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"config.h"
#include	"cmlm.h"
#include	"cmlmsubunsubmsg.h"
#include	"afx/afx.h"
#include	"random128/random128.h"
#include	"dbobj.h"
#include	"mydirent.h"
#include	<iostream>
#include	<fstream>
#include	<ctype.h>
#include	<time.h>
#include	<errno.h>
#include	<sysexits.h>
#include	<string.h>

//
//  Initial subscription/unsubscription request.
//

static int dosubunsub(std::string, std::string,
	const char *, const char *, const char *, int, const char *);

int dosub(const char *address)
{
	std::string postoptions= cmdget_s("SUBSCRIBE");
	std::string msg(readmsg());
	std::string addr=returnaddr(msg, address);

	if (postoptions == "mod")
		return (dosubunsub(msg, addr, "sub", "modsubconfirm",
			"sub4.tmpl", 1, 0));

	return (dosubunsub(msg, addr, "sub", "subconfirm", "sub.tmpl", 0, 0));
}

int dounsub(const char *address)
{
	std::string msg(readmsg());
	std::string addr=returnaddr(msg, address);

	return (dosubunsub(msg, addr, "unsub", "unsubconfirm", "unsub.tmpl",
		0, 0));
}

// Set a write-only alias.

int doalias(const char *address)
{
	std::string msg(readmsg());
	std::string addr=returnaddr(msg, address);

	std::string x_alias="X-Alias: " + header_s(msg, "subject");

	return (dosubunsub(msg, addr, "alias", "aliasconfirm", "sub.tmpl", 0,
			   x_alias.c_str()));
}

// Moderated subscription approval

int domodsub(const char *address)
{
	std::string msg(readmsg());
	std::string addr="";

	std::string filename=address;

	std::string::iterator b=filename.begin(), e=filename.end();

	std::string::iterator p=
		std::find_if(b, e, std::not1(std::ptr_fun(::isalpha)));

	if (p == e)
	{
		std::transform(b, e, b, std::ptr_fun(::toupper));

                filename=COMMANDS "/sub." + filename;

		std::ifstream	ifs(filename.c_str());

		if (!std::getline(ifs, addr).good())
                        addr="";
        }

	if (addr == "")
	{
		std::cerr << "Invalid address." << std::endl;
		return (EX_SOFTWARE);
	}

	std::string token;
	std::string subj;

{
CommandLock	cmd_lock;

DbObj	dat;

	if (dat.Open(COMMANDSDAT, "C"))
	{
		perror(COMMANDSDAT);
		return (EX_OSERR);
	}

	std::ifstream	ifs;

	std::string filename;
	std::string key="sub." + addr;
	std::string subfilename;

	addrlower(key);

	filename=dat.Fetch(key, "");

	if (filename.size())
	{
		subfilename= COMMANDS "/" + filename;

		ifs.open(subfilename.c_str());
	}

	if (!ifs.is_open() || !std::getline(ifs, token).good())
	{
		std::cerr << "Invalid confirmation." << std::endl;
		return (EX_SOFTWARE);
	}

	while (std::getline(ifs, token).good())
		if (token.size() == 0) break;

	while (std::getline(ifs, token).good())
		if (token == MSGSEPARATOR)	break;

	subj=header_s(msg, "subject");		// Original subject

// Rebuild original msg.

	msg="";

	while (std::getline(ifs, token).good())
	{
		if (token.size() == 0)	continue;
		msg += token;
		msg += '\n';
	}

	ifs.close();
	unlink(subfilename.c_str());
	dat.Delete(key);
}

	TrimLeft(subj);

	if (subj.substr(0, 2) == "no")
	{
		std::string owner=get_verp_return("owner");

		pid_t	p;
		afxopipestream ack(sendmail_bcc(p, owner));

		ack << "From: " << myname() << " <" << owner << ">" << std::endl
			<< "To: " << addr << std::endl
			<< "Bcc: " << addr << std::endl;

		simple_template(ack, "sub5.tmpl", msg);
		ack.close();
		return (wait4sendmail(p));
	}

	return (dosubunsub(msg, addr, "sub", "subconfirm", "sub.tmpl", 0, 0));
}

static int dosubunsub(std::string msg,	// Message headers, for logging
		      std::string addr,	// Parsed address
		      const char *cmdpfix, // Prefix for the commands/ file
		      const char *retpfix, // Prefix on the return address
		      const char *tpl,	// Template for the acknowledgement
		      int flag,		// Non-true if moderator sub approval
		      const char * xh)	// Extra header on response, for aliases
{
	std::fstream	tokfile;

	if (addr.size() == 0)
	{
		std::cerr << "Invalid address." << std::endl;
		return (EX_SOFTWARE);
	}

CommandLock	cmd_lock;

DbObj	dat;

	if (dat.Open(COMMANDSDAT, "C"))
	{
		perror(COMMANDSDAT);
		return (EX_OSERR);
	}

	std::string key(cmdpfix);

	key += ".";
	key += addr;

	addrlower(key);

	std::string subfilename;

	struct	stat stat_buf;

	std::string filename;

	filename=dat.Fetch(key, "");

	if (filename.size())
	{
		// Already received previous command from this sender

		subfilename= COMMANDS "/" + filename;
	}
	else
	{
		// Create new command file for this sender

		std::string f;

		do
		{
			f=cmdpfix;
			f += ".";
			f += random128_alpha();

			subfilename = COMMANDS "/" + f;

		} while (stat(subfilename.c_str(), &stat_buf) == 0);

		if (dat.Store(key, f, "R"))
		{
			perror(COMMANDSDAT);
			return (EX_OSERR);
		}
	}

time_t	curtime;

	time(&curtime);

//
//  At most, acknowledge a subscription for the same addresses no
//  more frequently then once per 30 minutes -- this stops a subscription bomb.
//
	if (stat(subfilename.c_str(), &stat_buf) == 0
	    && stat_buf.st_mtime >= curtime - 30 * 60)
		return (0);

//
//  Into the token file we write: address, newline, token, newline, then the
//  confirmation request.
//

	tokfile.open(subfilename.c_str(),
		     std::ios::in | std::ios::out | std::ios::trunc);

	if (!tokfile.is_open())
	{
		perror("open");
		return (1);
	}

	std::string me=retpfix;

	me += "-";
	me += strrchr(subfilename.c_str(), '.')+1;

	me=get_verp_return(me);

	std::string owner=get_verp_return("owner");

	// Use get_verp_return to compute the mailing list owner's address

	std::string sendto=addr;


	if (flag)
		sendto=owner;

	tokfile << addr << std::endl;

	if (xh)
		tokfile << xh << std::endl;

	tokfile << "From: " << myname() << " <" << owner << ">" << std::endl
		<< "Reply-To: " << me << std::endl
		<< "To: " << sendto << std::endl;

	{
		std::ifstream ifs("confsubj.tmpl");

		if (!ifs.bad())
		{
			std::string s;

			if (std::getline(ifs, s).good())
			{
				tokfile << s << std::endl;
			}
		}
	}

	copy_template(tokfile, "subjrequest.tmpl", "");

	ack_template(tokfile, tpl,
		     std::string(retpfix) + "/" +
		     (strrchr(subfilename.c_str(), '.')+1), msg);

	std::string buf;

	tokfile.seekg(0);
	if (tokfile.bad())
	{
		perror("write");
		tokfile.close();
		unlink(subfilename.c_str());
		return (EX_OSERR);
	}

	pid_t	p;
	int	nodsn= (cmdget_s("NODSN") == "1");
	afxopipestream	ack(sendmail_bcc(p, owner.c_str(), nodsn));

	ack << "Bcc: " << sendto << std::endl;

	std::getline(tokfile, buf); // Skip token number

	while (std::getline(tokfile, buf).good())
		ack << buf << std::endl;

	tokfile.close();
	ack.flush();
	if (ack.fail())
	{
		std::cerr << strerror(errno) << std::endl;
		exit(1);
	}

	ack.close();
	return (wait4sendmail(p));
}

//
//  Subscription confirmation.
//

int dosubunsubconfirm(const char *address, const char *pfix,
		      int (*func)(const char *, std::string),
		      const char *good_template,
		      const char *bad_template,
		      int donotsend)
{
	std::string msg(readmsg());
	std::string addr="";

	std::string filename=address;

	std::string::iterator b=filename.begin(), e=filename.end();

	std::string::iterator p=
		std::find_if(b, e, std::not1(std::ptr_fun(::isalpha)));

	if (p == e)
	{
		std::transform(b, e, b, std::ptr_fun(::toupper));

		filename=COMMANDS "/" + (pfix + ("." + filename));

		std::ifstream	ifs(filename.c_str());

		if (!std::getline(ifs, addr).good())
			addr="";
	}

	if (addr != "")
	{
	CommandLock	cmd_lock;

	DbObj	dat;

		if (dat.Open(COMMANDSDAT, "C"))
		{
			perror(COMMANDSDAT);
			return (EX_OSERR);
		}

		std::ifstream	ifs;

		std::string filename;
		std::string key=pfix;

		key += '.';
		key += addr;

		std::string subfilename;

		addrlower(key);

		filename=dat.Fetch(key, "");

		if (filename.size())
		{
			subfilename= COMMANDS "/" + filename;

			ifs.open(subfilename.c_str());
		}

		std::string token;

		if (ifs.is_open() &&
		    std::getline(ifs, token).good() &&
		    checkconfirm(msg))
		{
//
//  We will save the original subscription request, and the acknowledgement,
//  in this luser's subscribtion record.
//
			std::string subbuf;

			while (std::getline(ifs, token).good())
			{
				subbuf += token;
				subbuf += '\n';
			}
			subbuf += "\n*** ACKNOWLEDGEMENT ***\n";
			subbuf += msg;
			ifs.close();

			int	rc=(*func)(addr.c_str(), subbuf);

			if (rc == 0)
				donotsend=0;

			if (rc == 0 || rc == 9)
			{
				unlink(subfilename.c_str());
				dat.Delete(key);

				if (!donotsend)
				{
					pid_t	p;
					std::string owner=
						get_verp_return("owner");

					afxopipestream ack(sendmail_bcc(p, owner));
				
					owner= myname() + (" <" + owner + ">");

					ack << "Bcc: " << addr << std::endl
						<< "From: " << owner << std::endl
						<< "Reply-To: " << owner
								<< std::endl
						<< "To: "
							<< addr
							<< std::endl;

					copy_template(ack, "suback.tmpl", "");
					ack_template(ack,
						     rc ? bad_template:
						     good_template, "", msg);
					ack.flush();
					if (ack.fail())
					{
						std::cerr << strerror(errno) << std::endl;
						exit(1);
					}
					ack.close();
					rc=wait4sendmail(p);
				}

			}
			dat.Close();

		DIR	*dirp;
		struct	dirent	*de=0;

			if ((dirp=opendir(COMMANDS)) != 0)
			{
				while ((de=readdir(dirp)) != 0)
					if ((de->d_name[0]) != '.')
						break;
				closedir(dirp);
			}

			if (de == 0)	// Commands subdir is empty
				unlink(COMMANDSDAT);
			return (rc);
		}
	}
	std::cerr << "Invalid confirmation.\n" << std::endl;
	return (EX_NOPERM);
}
