/*
** Copyright 2002-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "mail.H"
#include "misc.H"
#include "logininfo.H"
#include "sync.H"
#include "envelope.H"
#include "structure.H"
#include "maildir.H"
#include "rfcaddr.H"
#include "addmessage.H"
#include "smtpinfo.H"
#include "rfc822/rfc822.h"
#include <cstring>

#if HAVE_TERMIOS_H
#include <termios.h>
#endif

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include <sstream>
#include <errno.h>

using namespace std;

static void error(string errmsg)
{
	cerr << "ERROR: " << errmsg << endl;
	exit (1);
}

static void error(mail::ACCOUNT *p)
{
	error(p->errmsg);
}

extern "C" void rfc2045_error(const char *p)
{
	cerr << "ERROR: " << p << endl;
	exit (0);
}

static void showenvelope(const mail::envelope &env, string pfix="")
{
	if (env.date > 0)
	{
		char buffer[200];

		rfc822_mkdate_buf(env.date, buffer);
		cout << pfix << "      Date: " << buffer
		     << endl;
	}

	cout << pfix << "   Subject: " << env.subject << endl;
	cout << pfix << "In-ReplyTo: " << env.inreplyto
	     << endl;

	vector<string>::const_iterator b=env.references.begin(),
		e=env.references.end();

	while (b != e)
	{
		cout << pfix << " Reference: <" << *b << ">" << endl;
		b++;
	}

	cout << pfix << "Message-ID: " << env.messageid
	     << endl;
	cout << pfix << mail::address::toString("      From: ",
					env.from)
	     << endl;
	cout << pfix << mail::address::toString("    Sender: ",
					env.sender)
	     << endl;
	cout << pfix << mail::address::toString("  Reply-To: ",
					env.replyto)
	     << endl;
	cout << pfix << mail::address::toString("        To: ",
					env.to)
	     << endl;
	cout << pfix << mail::address::toString("        Cc: ",
					env.cc)
	     << endl;
	cout << pfix << mail::address::toString("       Bcc: ",
					env.bcc)
	     << endl;
}

static void showstructure(const mail::mimestruct &mps, string pfix="")
{
	cout << pfix << "                  Mime-ID: " << mps.mime_id << endl;
	cout << pfix << "             Content-Type: " << mps.type << "/"
	     << mps.subtype << endl;

	mail::mimestruct::parameterList::const_iterator b, e;

	b=mps.type_parameters.begin();
	e=mps.type_parameters.end();

	while (b != e)
	{
		cout << pfix << "                          "
		     << b->first << "=" << b->second << endl;
		b++;
	}

	cout << pfix << "               Content-Id: " << mps.content_id << endl;
	cout << pfix << "      Content-Description: " << mps.content_description
	     << endl;
	cout << pfix << "Content-Transfer-Encoding: "
	     << mps.content_transfer_encoding << endl;
	cout << pfix << "                     Size: " << mps.content_size
	     << endl;
	cout << pfix << "                    Lines: " << mps.content_lines
	     << endl;
	cout << pfix << "              Content-MD5: " << mps.content_md5
	     << endl;
	cout << pfix << "         Content-Language: " << mps.content_language
	     << endl;
	cout << pfix << "      Content-Disposition: "
	     << mps.content_disposition << endl;

	b=mps.content_disposition_parameters.begin();
	e=mps.content_disposition_parameters.end();

	while (b != e)
	{
		cout << pfix << "                          "
		     << b->first << "=" << b->second << endl;
		b++;
	}

	if (mps.messagerfc822())
	{
		showenvelope(mps.getEnvelope(), pfix + "    ");
	}

	size_t n=mps.getNumChildren();
	size_t i;

	for (i=0; i<n; i++)
	{
		cout << pfix
		     << "---------------------------------------------------"
		     << endl;
		showstructure(*mps.getChild(i), pfix + "    ");
	}
}

static void showFolderList(mail::ACCOUNT *p,
			   mail::ACCOUNT::FolderList &list, int nestinglevel,
			   bool tree)
{
	vector<const mail::folder *> cpy;

	cpy.insert(cpy.end(), list.begin(), list.end());

	sort(cpy.begin(), cpy.end(), mail::folder::sort(false));

	vector<const mail::folder *>::iterator
		b=cpy.begin(), e=cpy.end();

	bool extraSpace=false;

	while (b != e)
	{
		const mail::folder *f= *b++;

		if (extraSpace)
			cout << endl;

		extraSpace=false;

		if (tree)
		{
			mail::ACCOUNT::FolderInfo info;

			if (f->hasMessages())
				p->readFolderInfo(f, info);

			cout << setw(nestinglevel * 4 + 1) << " " << setw(0)
			     << f->getName();

			if (info.unreadCount || info.messageCount)
			{
				cout << " (";
				if (info.messageCount)
				{
					cout << info.messageCount << " messages";
					if (info.unreadCount)
						cout << ", ";
				}

				if (info.unreadCount)
					cout << info.unreadCount << " unread";
				cout << ")";
			}

			cout << endl;
		}
		else
		{
			cout << f->getPath() << endl;
		}

		if (f->hasSubFolders() || !f->hasMessages())
		{
			mail::ACCOUNT::FolderList subfolders;

			if (p->getSubFolders(f, subfolders))
			{
				showFolderList(p, subfolders, nestinglevel+1,
					       tree);
				extraSpace=tree;
			}
			else if (f->hasSubFolders())
			{
				error(p);
			}
		}
	}
}

static void showEnvelope(mail::xenvelope &env)
{
	char date[100];

	if (env.arrivalDate)
	{
		rfc822_mkdate_buf(env.arrivalDate, date);

		cout << " Arrival-Date: " << date << endl;
	}

	cout << "         Size: " << env.messageSize << endl;

	if (env.date)
	{
		rfc822_mkdate_buf(env.date, date);

		cout << "         Date: " << date << endl;
	}

	cout << "      Subject: " << env.subject << endl;
	cout << "   Message-Id: " << env.messageid << endl;
	if (env.inreplyto.size() > 0)
		cout << "  In-Reply-To: " << env.inreplyto << endl;

	vector<string>::const_iterator b=env.references.begin(),
		e=env.references.end();

	while (b != e)
	{
		cout << "    Reference: <" << *b << ">" << endl;
		b++;
	}

	if (env.from.size() > 0)
		cout << mail::address::toString("         From: ", env.from)
		     << endl;

	if (env.sender.size() > 0)
		cout << mail::address::toString("       Sender: ", env.sender)
		     << endl;

	if (env.replyto.size() > 0)
		cout << mail::address::toString("     Reply-To: ",
						env.replyto)<< endl;
	if (env.to.size() > 0)
		cout << mail::address::toString("           To: ", env.to)
		     << endl;

	if (env.cc.size() > 0)
		cout << mail::address::toString("           Cc: ",
						env.cc) << endl;

	if (env.bcc.size() > 0)
		cout << mail::address::toString("          Bcc: ", env.bcc)
		     << endl;
}

static bool getIndex(mail::ACCOUNT *p, mail::folder *f)
{
	if (!p->openFolder(f, NULL))
		return false;

	size_t n=p->getFolderIndexSize();
	size_t i;

	vector<size_t> msgnums;

	cout << "Total: " << n << " messages." << endl;

#if 1
	for (i=0; i<n; i++)
		msgnums.push_back(i);

	vector<mail::xenvelope> envelopes;

	if (!p->getMessageEnvelope(msgnums, envelopes))
		return false;

	for (i=0; i<n; i++)
	{
		if (i > 0)
			cout << endl;
		cout << "Message " << (i+1) << ":" << endl;

		showEnvelope(envelopes[i]);
	}
#endif

#if 0
	vector<mail::mimestruct> structures;

	if (!p->getMessageStructure(msgnums, structures))
		error(p);

	vector<mail::mimestruct>::iterator b=structures.begin(),
		e=structures.end();

	i=0;
	while (b != e)
	{
		cout << endl;
		cout << "-------------------------------------------" << endl;
		cout << "       Message " << ++i << endl;
		cout << "-------------------------------------------" << endl;
		showstructure( *b++ );
	}
#endif
	return true;

}

class DisplayHeader : public mail::ACCOUNT::Store {
public:
	DisplayHeader();
	~DisplayHeader();
	void store(size_t, string);
};

DisplayHeader::DisplayHeader()
{
}

DisplayHeader::~DisplayHeader()
{
}

void DisplayHeader::store(size_t dummy, string txt)
{
	cout << txt;
}

static bool getHeaders(mail::ACCOUNT *p, mail::folder *f, size_t n)
{
	if (!p->openFolder(f, NULL))
		return false;

	vector<size_t> v;

	v.push_back(n-1);

	DisplayHeader display_header;

	return (p->getMessageContent(v, false, mail::readHeadersFolded,
				     display_header));
}

static bool removeMessages(mail::ACCOUNT *p, mail::folder *f, const char *msgs)
{
	if (!p->openFolder(f, NULL))
		return false;

	vector<size_t> v;

	while (*msgs)
	{
		if (!isdigit(*msgs))
		{
			msgs++;
			continue;
		}

		size_t n=0;

		while (*msgs && isdigit(*msgs))
			n= n * 10 + (*msgs++ - '0');

		if (n > 0)
		{
			cout << "Removing " << n << endl;
			v.push_back(n-1);
		}
	}

	cout << "Ready: ";

	string reply;

	if (getline(cin, reply).fail())
		return true;

	return (p->removeMessages(v));
}

static bool doFilterFolder(mail::ACCOUNT *p, mail::folder *f)
{
	if (!p->openFolder(f, NULL))
		return false;

	size_t n=p->getFolderIndexSize();

	cout << "Total: " << n << " messages." << endl;

	size_t i;

	for (i=0; i<n; i++)
	{
		vector<size_t> msgnums;

		msgnums.push_back(i);

		vector<mail::xenvelope> envelopes;

		if (!p->getMessageEnvelope(msgnums, envelopes))
			return false;

		cout << endl << "Message " << (i+1) << ":" << endl;
		showEnvelope(envelopes[0]);

		cout << "D)elete, S)kip, E)xit? (S) " << flush;

		string reply;

		if (getline(cin, reply).fail())
			return true;

		if (reply.size() == 0)
			continue;

		mail::messageInfo msgInfo=p->getFolderIndexInfo(n);

		switch (reply[0]) {
		case 'D':
		case 'd':
			msgInfo.deleted=true;

			if (!p->saveFolderIndexInfo(n, msgInfo))
				return false;
			continue;
		case 'E':
		case 'e':
			break;
		default:
			continue;
		}
		break;
	}

	p->updateFolderIndexInfo();

	return true;
}

static bool doUploadMessage(mail::ACCOUNT *p, mail::folder *f)
{
	class uploadMessage : public mail::addMessagePull {
	public:

		string getMessageContents()
		{
			char buffer[BUFSIZ];

			int n=read(0, buffer, sizeof(buffer));

			if (n <= 0)
				return "";

			return (string(buffer, buffer+n));
		}
	};

	uploadMessage upload;

	return p->addMessage(f, upload);
}

class readStdin : public mail::addMessagePull {
public:
	readStdin();
	~readStdin();
	string getMessageContents();
};

readStdin::readStdin()
{
}

readStdin::~readStdin()
{
}

string readStdin::getMessageContents()
{
	char buffer[BUFSIZ];

	int n=cin.read(buffer, sizeof(buffer)).gcount();

	if (n <= 0)
		return "";

	return string(buffer, buffer+n);
}

class copyProgressReporter : public mail::ACCOUNT::Progress {

	string lastmsg;

	static void fmtByte(ostringstream &o, size_t byteCount);

	size_t lastCompletedShown;

public:
	size_t bytesCompleted, bytesEstimatedTotal,
		messagesCompleted, messagesEstimatedTotal;

	bool doReportProgress;
	time_t timestamp;
	bool final;

	copyProgressReporter(mail::ACCOUNT *acct);
	~copyProgressReporter();

	void operator()(size_t bytesCompleted,
			size_t bytesEstimatedTotal,

			size_t messagesCompleted,
			size_t messagesEstimatedTotal);

	void operator()();
};

copyProgressReporter::copyProgressReporter(mail::ACCOUNT *acct)
	: Progress(acct), lastmsg(""),
	  lastCompletedShown(0),
	  bytesCompleted(0), bytesEstimatedTotal(0),
	  messagesCompleted(0), messagesEstimatedTotal(0),
	  doReportProgress(isatty(1) != 0), timestamp(time(NULL)),
	  final(false)
{
}

copyProgressReporter::~copyProgressReporter()
{
}

void copyProgressReporter::operator()(size_t bytesCompletedArg,
				      size_t bytesEstimatedTotalArg,

				      size_t messagesCompletedArg,
				      size_t messagesEstimatedTotalArg)
{
	bytesCompleted=bytesCompletedArg;
	bytesEstimatedTotal=bytesEstimatedTotalArg;
	messagesCompleted=messagesCompletedArg;
	messagesEstimatedTotal=messagesEstimatedTotalArg;

	if (!doReportProgress)
		return;

	time_t t=time(NULL);

	if (t == timestamp)
		return;

	timestamp=t;

	(*this)();
}

void copyProgressReporter::operator()()
{
	if (lastCompletedShown != messagesCompleted)
		bytesCompleted=bytesEstimatedTotal=0;
	// Background noise.

	lastCompletedShown=messagesCompleted;

	size_t i, n=lastmsg.size();

	for (i=0; i<n; i++)
		cout << '\b';
	for (i=0; i<n; i++)
		cout << ' ';
	for (i=0; i<n; i++)
		cout << '\b';

	ostringstream o;

	if (bytesCompleted)
	{
		fmtByte(o, bytesCompleted);

		if (bytesCompleted < bytesEstimatedTotal)
		{
			o << " of ";
			fmtByte(o, bytesEstimatedTotal);
		}
	}

	if (final || messagesEstimatedTotal > 1)
	{
		if (bytesCompleted)
			o << "; ";

		o << messagesCompleted;
		if (messagesCompleted < messagesEstimatedTotal)
			o << " of " << messagesEstimatedTotal;
		o << " msgs copied...";
	}

	lastmsg=o.str();
	cout << lastmsg << flush;
}

void copyProgressReporter::fmtByte(ostringstream &o, size_t byteCount)
{
	if (byteCount < 1024)
	{
		o << byteCount;
		return;
	}

	if (byteCount < 1024 * 1024)
	{
		o << (byteCount + 512) / 1024 << " Kb";
		return;
	}

	o << byteCount / (1024 * 1024) << "."
	  << (byteCount % (1024 * 1024)) * 10 / (1024 * 1024) << " Mb";
}

static string docopyfolderordir(mail::ACCOUNT *fromAccount,
				mail::ACCOUNT *toAccount,
				mail::folder *fromfolder,
				mail::folder *tofolder);

static string docopyfolder(mail::ACCOUNT *fromAccount,
			   mail::ACCOUNT *toAccount,
			   mail::folder *fromfolder,
			   mail::folder *tofolder);


static string docopy(mail::ACCOUNT *fromAccount,
		     mail::ACCOUNT *toAccount,
		     string fromFolderName,
		     string toFolderName,
		     bool recurse)
{
	string fromNameServer=fromFolderName, toNameServer=toFolderName;

	mail::folder *fromfolder=
		fromAccount->getFolderFromPath(fromNameServer);

	if (!fromfolder)
		return fromAccount->errmsg;

	mail::folder *tofolder=
		toAccount->getFolderFromPath(toNameServer);

	if (!tofolder)
		return toAccount->errmsg;

	string errmsg=recurse ?
		docopyfolderordir(fromAccount, toAccount,
				  fromfolder, tofolder)
		: docopyfolder(fromAccount, toAccount,
			       fromfolder, tofolder);

	delete fromfolder;
	delete tofolder;
	return errmsg;
}

static string docopyfolderordir(mail::ACCOUNT *fromAccount,
				mail::ACCOUNT *toAccount,
				mail::folder *fromfolder,
				mail::folder *tofolder)
{
	if (fromfolder->hasSubFolders())
	{
		toAccount->createFolder(tofolder, true);
		// Ignore error, subdirectory may already exist

		mail::ACCOUNT::FolderList subfolders;

		if (!fromAccount->getSubFolders(fromfolder, subfolders))
			return fromfolder->getName() + ": " +
				fromAccount->errmsg;

		mail::ACCOUNT::FolderList::iterator b=subfolders.begin(),
			e=subfolders.end();

		while (b != e)
		{
			mail::folder *f= *b++;
			string n=f->getName();

			mail::folder *newFolder=
				toAccount->createFolder(tofolder,
							n,
							!f->hasMessages());

			if (!newFolder) // May already exist
			{
				mail::ACCOUNT::FolderList subfolders2;

				if (!toAccount->getSubFolders(tofolder,
							      subfolders2))
					return tofolder->getName() + ": "
						+ toAccount->errmsg;

				mail::ACCOUNT::FolderList::iterator
					sb=subfolders2.begin(),
					se=subfolders2.end();

				while (sb != se)
				{
					if ( (*sb)->getName() == n)
					{
						newFolder= (*sb)->clone();

						if (!newFolder)
							return strerror(errno);
						break;
					}
					sb++;
				}

				if (!newFolder)
				{
					return tofolder->getName()
						+ ": cannot create "
						+ n;
				}
			}

			string errmsg=docopyfolderordir(fromAccount,
							toAccount,
							f,
							newFolder);

			if (errmsg != "")
				return errmsg;
			delete newFolder;
		}
		if (!fromfolder->hasMessages())
			return "";
	}

	return docopyfolder(fromAccount, toAccount, fromfolder, tofolder);
}

static string docopyfolder(mail::ACCOUNT *fromAccount,
			   mail::ACCOUNT *toAccount,
			   mail::folder *fromfolder,
			   mail::folder *tofolder)
{
	if (!fromAccount->openFolder(fromfolder, NULL))
		return fromfolder->getName() + ": "
			+ fromAccount->errmsg;

	toAccount->createFolder(tofolder, false); // May fail, ignore

	{
		mail::ACCOUNT::FolderInfo dummyInfo;

		if (!toAccount->readFolderInfo(tofolder, dummyInfo))
			return tofolder->getName() + ": " + toAccount->errmsg;
	}

	size_t n=fromAccount->getFolderIndexSize();

	vector<size_t> copyvec;

	copyvec.reserve(n);
	size_t i;

	for (i=0; i<n; i++)
		copyvec.push_back(i);

	{
		copyProgressReporter progressReport(fromAccount);

		cout << fromfolder->getName() << ": ";

		if (!fromAccount->copyMessagesTo(copyvec, tofolder))
		{
			progressReport();
			cout << endl;
			return fromAccount->errmsg;
		}
		progressReport.bytesCompleted=0;
		progressReport.bytesEstimatedTotal=0;
		progressReport.messagesCompleted=n;
		progressReport.messagesEstimatedTotal=n;
		progressReport.final=true;
		progressReport();
		cout << endl;
	}
	return "";
}

static std::string pw_url;

class pw_prompt : public mail::loginCallback {
public:
	pw_prompt();
	~pw_prompt();

	void loginPrompt(callbackType cbType,
			 std::string prompt);
};

pw_prompt::pw_prompt()
{
}

pw_prompt::~pw_prompt()
{
}


void pw_prompt::loginPrompt(mail::loginCallback::callbackType theType,
			    std::string prompt)
{
#if HAVE_TERMIOS_H
	struct termios ti;
#endif

	if (isatty(1))
	{
		if (pw_url.size() > 0)
			cout << pw_url << "> ";

		cout << prompt;

#if HAVE_TERMIOS_H
		if (theType == PASSWORD)
		{
			struct termios ti2;

			if (tcgetattr(0, &ti))
			{
				perror("tcgetattr");
			}

			ti2=ti;

			ti2.c_lflag &= ~ECHO;
			tcsetattr(0, TCSAFLUSH, &ti2);
		}
#endif
	}

	char linebuf[80];

	cin.getline(linebuf, sizeof(linebuf));

#if HAVE_TERMIOS_H
	if (isatty(1))
	{
		if (theType == PASSWORD)
		{
			cout << endl;
			tcsetattr(0, TCSAFLUSH, &ti);
		}
	}
#endif

	char *p=strchr(linebuf, '\n');

	if (p) *p=0;

	callback(string(linebuf));
}

static void getExtraString(mail::account::openInfo &loginInfoArg)
{
	if (strncmp(loginInfoArg.url.c_str(), "nntp", 4) == 0)
	{
		const char *p=getenv("NEWSRC");

		if (p && *p)
			loginInfoArg.extraString=p;
		else
			loginInfoArg.extraString=
				mail::homedir() + "/.newsrc";
		return;
	}

	if (strncmp(loginInfoArg.url.c_str(), "pop3maildrop", 12) == 0)
	{
		cout << "maildrop (./Maildir): " << flush;

		char linebuf[80];

		cin.getline(linebuf, sizeof(linebuf));

		char *p=strchr(linebuf, '\n');

		if (p) *p=0;

		if (linebuf[0] == 0)
			strcpy(linebuf, "Maildir");

		loginInfoArg.extraString=linebuf;
		if (*loginInfoArg.extraString.c_str() != '/')
		{
			if (getcwd(linebuf, sizeof(linebuf)) == NULL)
			{
				perror("getcwd");
				exit(1);
			}

			loginInfoArg.extraString= string(linebuf) + "/" +
				loginInfoArg.extraString;
		}
		mail::maildir::maildirmake(loginInfoArg.extraString, false);
	}
}

int main(int argc, char **argv)
{
	int argn=1;
	const char *messagenum="0";

	string path, newpath, name;
	bool doCreate=false;
	bool doCreateDir=false;
	bool doDelete=false;
	bool doDeleteDir=false;
	bool doTree=false;
	bool doList=false;
	bool doIndex=false;
	bool doHeaders=false;
	bool doRemove=false;
	bool doFilter=false;
	bool doUpload=false;
	bool doMail=false;
	bool doRename=false;

	bool doCopy=false;
	bool doRecurse=false;
	string copyto;
	string tofolder;
	string fromfolder;

	mail::smtpInfo smtpInfo;

	while (argn < argc)
	{
		if (strcmp(argv[argn], "-mailfrom") == 0 && argc - argn >= 2)
		{
			doMail=true;
			smtpInfo.sender=argv[++argn];
			++argn;
			continue;
		}

		if (strcmp(argv[argn], "-to") == 0 && argc - argn >= 2)
		{
			smtpInfo.recipients.push_back(argv[++argn]);
			++argn;
			continue;
		}

		if (strcmp(argv[argn], "-opt") == 0 && argc - argn >= 3)
		{
			string opt=argv[++argn];
			string val=argv[++argn];

			smtpInfo.options.insert(make_pair(opt, val));
			++argn;
			continue;
		}

		if (strcmp(argv[argn], "-create") == 0 && argc - argn >= 3)
		{
			path=argv[++argn];
			name=argv[++argn];
			++argn;
			doCreate=true;
			continue;
		}

		if (strcmp(argv[argn], "-rename") == 0 && argc - argn >= 4)
		{
			path=argv[++argn];
			newpath=argv[++argn];
			name=argv[++argn];
			++argn;
			doRename=true;
			continue;
		}

		if (strcmp(argv[argn], "-createdir") == 0 && argc - argn >= 3)
		{
			path=argv[++argn];
			name=argv[++argn];
			++argn;
			doCreateDir=true;
			continue;
		}

		if (strcmp(argv[argn], "-delete") == 0 && argc - argn >= 2)
		{
			path=argv[++argn];
			++argn;
			doDelete=true;
			continue;
		}

		if (strcmp(argv[argn], "-deletedir") == 0 && argc - argn >= 2)
		{
			path=argv[++argn];
			++argn;
			doDeleteDir=true;
			continue;
		}

		if (strcmp(argv[argn], "-tree") == 0)
		{
			doTree=true;
			++argn;
			continue;
		}

		if (strcmp(argv[argn], "-list") == 0)
		{
			doList=true;
			++argn;
			continue;
		}

		if (strcmp(argv[argn], "-index") == 0 && argc - argn >= 2)
		{
			path=argv[++argn];
			++argn;
			doIndex=true;
			continue;
		}

		if (strcmp(argv[argn], "-upload") == 0 && argc - argn >= 2)
		{
			path=argv[++argn];
			++argn;
			doUpload=true;
			continue;
		}

		if (strcmp(argv[argn], "-filter") == 0 && argc - argn >= 2)
		{
			path=argv[++argn];
			++argn;
			doFilter=true;
			continue;
		}

		if (strcmp(argv[argn], "-headers") == 0 && argc - argn >= 3)
		{
			path=argv[++argn];
			messagenum=argv[++argn];
			++argn;
			doHeaders=true;
			continue;
		}

		if (strcmp(argv[argn], "-remove") == 0 && argc - argn >= 3)
		{
			path=argv[++argn];
			messagenum=argv[++argn];
			++argn;
			doRemove=true;
			continue;
		}

		if (strcmp(argv[argn], "-copyto") == 0 && argc - argn >= 2)
		{
			copyto=argv[++argn];
			++argn;
			doCopy=true;
			continue;
		}

		if (strcmp(argv[argn], "-tofolder") == 0 && argc - argn >= 2)
		{
			tofolder=argv[++argn];
			++argn;
			continue;
		}

		if (strcmp(argv[argn], "-fromfolder") == 0 && argc - argn >= 2)
		{
			fromfolder=argv[++argn];
			++argn;
			continue;
		}

		if (strcmp(argv[argn], "-recurse") == 0)
		{
			doRecurse=true;
			++argn;
			continue;
		}
		break;
	}

	if (argn >= argc)
		exit (0);

	string url=argv[argn];

	mail::ACCOUNT *p=new mail::ACCOUNT();

	if (!p)
	{
		cerr << strerror(errno) << endl;
		exit (0);
	}

	mail::ACCOUNT::FolderList folderList;

	pw_url=url;

	{
		pw_prompt getpassword;

		mail::account::openInfo loginInfoArg;

		loginInfoArg.url=url;

		getExtraString(loginInfoArg);

		loginInfoArg.loginCallbackObj= &getpassword;
		if (!p->login(loginInfoArg) ||
		    !p->getTopLevelFolders(folderList))
		{
			error(p);
		}
	}

	if (doCopy)
	{
		mail::ACCOUNT *toAccount=new mail::ACCOUNT();

		if (!toAccount)
		{
			cerr << strerror(errno) << endl;
			exit (0);
		}

		pw_url=copyto;

		pw_prompt getpassword;

		mail::account::openInfo loginInfo;

		loginInfo.url=copyto;
		loginInfo.loginCallbackObj= &getpassword;
		getExtraString(loginInfo);

		if (!toAccount->login(loginInfo))
		{
			error(toAccount);
		}

		string errmsg=docopy(p, toAccount, fromfolder, tofolder,
				     doRecurse);

		if (errmsg.size() > 0)
			error(errmsg);
		toAccount->logout();
		delete toAccount;
	}
	else if (doCreate || doCreateDir)
	{
		mail::folder *f=p->getFolderFromPath(path);

		if (!f || !(f=p->createFolder(f, name, doCreateDir)))
		{
			error(p);
		}
	}
	else if (doRename)
	{
		mail::folder *oldFolder=p->getFolderFromPath(path);

		if (!oldFolder)
		{
			cerr << "ERROR: " << path << " - folder not found."
			     << endl;
			exit(0);
		}

		mail::folder *newParent=NULL;

		if (newpath.size() > 0)
		{
			newParent=p->getFolderFromPath(newpath);

			if (!newParent)
			{
				cerr << "ERROR: " << newpath
				     << " - folder not found."
				     << endl;
				exit(0);
			}
		}

		if (!(oldFolder=p->renameFolder(oldFolder, newParent, name)))
			error(p);
	}
	else if (doDelete || doDeleteDir)
	{
		mail::folder *f=p->getFolderFromPath(path);

		if (!f || !p->deleteFolder(f, doDeleteDir))
		{
			error(p);
		}
	}
	else if (doIndex)
	{
		mail::folder *f=p->getFolderFromPath(path);

		if (!f || !getIndex(p, f))
			error(p);

#if 0
		cout << "TOTAL # OF MESSAGES: " << p->getFolderIndexSize()
		     << endl;

		string dummy;

		getline(cin, dummy);

		cout << "CHECK NEW MAIL: " << p->checkNewMail() << endl;

		cout << "TOTAL # OF MESSAGES: " << p->getFolderIndexSize()
		     << endl;

		if (!getIndex(p, f))
			error(p);
#endif
	}
	else if (doHeaders)
	{
		mail::folder *f=p->getFolderFromPath(path);

		if (!f || !getHeaders(p, f, atoi(messagenum)))
			error(p);
	}
	else if (doRemove)
	{
		mail::folder *f=p->getFolderFromPath(path);

		if (!f || !removeMessages(p, f, messagenum))
			error(p);
	}
	else if (doFilter)
	{
		mail::folder *f=p->getFolderFromPath(path);

		if (!f || !doFilterFolder(p, f))
			error(p);
	}
	else if (doUpload)
	{
		mail::folder *f=p->getFolderFromPath(path);

		if (!f || !doUploadMessage(p, f))
			error(p);
	}
	else if (doMail)
	{
		readStdin doReadStdin;

		if (!p->send(smtpInfo, NULL, doReadStdin))
		{
			string errmsg=p->errmsg;
			p->logout();
			delete p;
			error(errmsg);
		}
	}
	else if (doTree || doList)
	{
		mail::ACCOUNT::FolderList::iterator b=folderList.begin(),
			e=folderList.end();

		while (b != e)
		{
			if ( (*b)->getPath().size() == 0)
				break;

			++b;
		}

		if (b != e)
		{
			mail::ACCOUNT::FolderList subfolders;

			if (!p->getSubFolders(*b, subfolders))
				error(p);

			showFolderList(p, subfolders, 0, doTree);
		}
		else
			showFolderList(p, folderList, 0, doTree);
	}

	p->logout();

	delete p;
	exit(0);
}
