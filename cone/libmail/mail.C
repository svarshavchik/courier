/*
** Copyright 2002-2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "mail.H"
#include "sync.H"

#include "driver.H"
#include "mbox.H"

#include "runlater.H"
#include "logininfo.H"
#include <iostream>
#include <errno.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pwd.h>
#include <arpa/inet.h>
#include "rfc822/rfc822.h"
#include "unicode/unicode.h"
#include "maildir/maildiraclt.h"
#include "misc.H"
#include <fstream>

using namespace std;

list<mail::account *> mail::account::mailaccount_list;

//
// Initialize mail::folder's default.  mail::folder is a superclass for
// mail folder objects.  Each mail account type is expected to use a
// subclass for the actual implementation.
//
// Each mail::folder object is associated with a corresponding mail::account
// object.  The constructor saves a pointer, and provides methods that
// the subclass can use to determine whether the mail::account object still exists

mail::folder::folder(mail::account *ptr) : myServer(ptr)
{
}

mail::folder::~folder()
{
}

//
// The first order of business for any mail::folder method call is to
// determine whether the mail::account object still exists.  This method is
// a shortcut for most methods that receive a mail::callback object.
// If the mail::account object was destroyed, callback's fail method is called
// appropriately.

bool mail::folder::isDestroyed(mail::callback &callback) const
{
	if (myServer.isDestroyed())
	{
		callback.fail("Server connection closed.");
		return true;
	}

	return false;
}

bool mail::folder::isDestroyed() const
{
	return myServer.isDestroyed();
}

string mail::folder::isNewsgroup() const
{
	return "";
}

void mail::folder::getMyRights(mail::callback &getCallback,
			       std::string &rights) const
{
	getCallback.fail("This server does not implement access control lists.");

}

void mail::folder::getRights(mail::callback &getCallback,
			     std::list<std::pair<std::string, std::string> >
			     &rights) const
{
	rights.clear();
	getCallback.fail("This server does not implement access control lists.");
}

void mail::folder::setRights(mail::callback &setCallback,
			     string &errorIdentifier,
			     vector<std::string> &errorRights,
			     string identifier,
			     string rights) const
{
	errorIdentifier="";
	errorRights.clear();
	setCallback.fail("This server does not implement access control lists.");
}

void mail::folder::delRights(mail::callback &setCallback,
			     string &errorIdentifier,
			     vector<std::string> &errorRights,
			     std::string identifier) const
{
	errorIdentifier="";
	errorRights.clear();
	setCallback.fail("This server does not implement access control lists.");
}

//
// callback::folder miscellania

mail::callback::folder::folder()
{
}

mail::callback::folder::~folder()
{
}

void mail::callback::folder::saveSnapshot(std::string snapshotId)
{
}

// mail::callback::folderList miscellania

mail::callback::folderList::folderList()
{
}

mail::callback::folderList::~folderList()
{
}

// mail::callback::folderInfo miscellania

mail::callback::folderInfo::folderInfo()
	: messageCount(0), unreadCount(0), fastInfo(false)
{
}

mail::callback::folderInfo::~folderInfo()
{
}

void mail::callback::folderInfo::success()
{
}

// messageInfo miscellania

mail::messageInfo::messageInfo()
{
	uid="";
	draft=replied=marked=deleted=unread=recent=false;
}

mail::messageInfo::messageInfo(string s)
{
	uid="";
	draft=replied=marked=deleted=unread=recent=false;

	string::iterator b=s.begin(), e=s.end();

	while (b != e)
	{
		switch (*b++) {
		case 'D':
			draft=true;
			break;
		case 'R':
			replied=true;
			break;
		case 'M':
			marked=true;
			break;
		case 'T':
			deleted=true;
			break;
		case 'U':
			unread=true;
			break;
		case 'C':
			recent=true;
			break;
		case ':':
			uid=string(b, e);
			b=e;
			break;
		}
	}
}

mail::messageInfo::~messageInfo()
{
}

mail::messageInfo::operator string() const
{
	string s;

	if (draft) s += "D";
	if (replied) s += "R";
	if (marked) s += "M";
	if (deleted) s += "T";
	if (unread) s += "U";
	if (recent) s += "C";

	return s + ":" + uid;
}


//
// mail::account superclass constructor.  The main task at hand is to save
// a reference to the disconnect callback, for later use, and to initialize
// the default character set.
//

mail::account::account(mail::callback::disconnect &callbackArg)
	: disconnect_callback(callbackArg)
{
	my_mailaccount_p=mailaccount_list.insert(mailaccount_list.end(), this);
}

mail::account::~account()
{
	// We can't just remove (this) from mailaccount_list, because we
	// might be iterating mail:;account::process().

	*my_mailaccount_p=NULL;
}

//////////////////////////////////////////////////////////////////////////////
//
// Utility functions.
//
//////////////////////////////////////////////////////////////////////////////

LIBMAIL_START

string hostname()
{
	char h[512];

	h[sizeof(h)-1]=0;

	if (gethostname(h, sizeof(h)-1) < 0)
		strcpy(h, "localhost");
	return h;
}

string homedir()
{
	const char *h=getenv("HOME");

	struct passwd *pw=getpwuid(getuid());

	if (!pw)
		return "";

	if (h == NULL || !*h)
		h=pw->pw_dir;

	return h;
}

void upper(string &w)
{
	for (string::iterator b=w.begin(), e=w.end(); b != e; ++b)
		if (*b >= 'a' && *b <= 'z')
			*b += 'A'-'a';
}
LIBMAIL_END

static const char x[]="0123456789ABCDEF";

static string encword(string w)
{
	string ww="";

	string::iterator b=w.begin(), e=w.end(), c;

	while (b != e)
	{
		for (c=b; c != e; c++)
			if ( *c == '@' || *c == '/' || *c == '%' || *c == ':')
				break;

		ww.append(b, c);

		b=c;

		if (b != e)
		{
			ww += '%';
			ww += x[ (*b / 16) & 15 ];
			ww += x[ *b & 15];
			b++;
		}
	}

	return ww;
}

LIBMAIL_START
string loginUrlEncode(string method, string server, string uid,
		      string pwd)
{
	return (method + "://" +
		(uid.size() > 0 ? encword(uid)
		 + (pwd.size() > 0 ? ":" + encword(pwd):"") + "@":"")
		+ server);
}
LIBMAIL_END

static string decword(string w)
{
	string ww="";

	string::iterator b=w.begin(), e=w.end(), c;

	while (b != e)
	{
		for (c=b; c != e; c++)
			if ( *c == '%' && e-c > 2)
				break;

		ww.append(b, c);

		b=c;

		if (b != e)
		{
			b++;

			const char *c1= strchr(x, *b++);
			const char *c2= strchr(x, *b++);

			char c=0;

			if (c1) c= (c1-x) * 16;
			if (c2) c += c2-x;

			ww += c;
		}
	}

	return ww;
}

LIBMAIL_START

// Parse url format: method://uid:pwd@server/option/option...

bool loginUrlDecode(string url, mail::loginInfo &loginInfo)
{
	size_t n=url.find(':');

	if (n == std::string::npos)
		return false;

	loginInfo.method=url.substr(0, n);

	if (url.size() - n < 3 || url[n+1] != '/' || url[n+2] != '/')
		return false;

	string serverStr=url.substr(n+3);
	string options="";

	n=serverStr.find('/');

	if (n != std::string::npos)
	{
		options=serverStr.substr(n+1);
		serverStr.erase(n);
	}

	n=serverStr.rfind('@');
	string uidpwd="";

	if (n != std::string::npos)
	{
		uidpwd=serverStr.substr(0, n);
		serverStr=serverStr.substr(n+1);
	}
	loginInfo.server=serverStr;

	n=uidpwd.find(':');

	string pwd="";

	if (n != std::string::npos)
	{
		pwd=uidpwd.substr(n+1);
		uidpwd=uidpwd.substr(0, n);
	}

	loginInfo.uid=decword(uidpwd);
	loginInfo.pwd=decword(pwd);

	while (options.size() > 0)
	{
		n=options.find('/');

		string option;

		if (n == std::string::npos)
		{
			option=options;
			options="";
		}
		else
		{
			option=options.substr(0, n);
			options=options.substr(n+1);
		}

		string optionVal;

		n=option.find('=');

		if (n != std::string::npos)
		{
			optionVal=option.substr(n+1);
			option=option.substr(0, n);
		}

		loginInfo.options.insert(make_pair(option, optionVal));
	}
	return true;
}

LIBMAIL_END

mail::account::openInfo::openInfo() : loginCallbackObj(NULL)
{
}

mail::account::openInfo::~openInfo()
{
}

mail::account *mail::account::open(mail::account::openInfo oi,
				   mail::callback &callback,
				   mail::callback::disconnect
				   &disconnectCallback)
{
	mail::driver **l=mail::driver::get_driver_list();

	while (*l)
	{
		mail::account *a;

		if ( (*(*l)->open_func)(a, oi, callback, disconnectCallback))
		{
			if (!a)
				callback.fail(strerror(errno));
			return a;
		}

		l++;
	}

	callback.fail("Invalid mail account URL.");
	return NULL;
}

bool mail::account::isRemoteUrl(std::string url)
{
	mail::driver **l=mail::driver::get_driver_list();

	while (*l)
	{
		bool flag;

		if ( (*(*l)->isRemote_func)(url, flag))
			return flag;

		l++;
	}

	return false;
}

// Default implementation of getSendFolder: no such luck.

mail::folder *mail::account::getSendFolder(const class mail::smtpInfo &info,
					   const mail::folder *folder,
					   string &errmsg)
{
	errno=ENOENT;
	errmsg="Not implemented.";
	return NULL;
}

//
// The main function.  Where everything good happens.
//
// mail::account::process calls each existing mail::account object's handler method.
//
void mail::account::process(vector<pollfd> &fds, int &timeout)
{
	list<mail::account *>::iterator b, e, cur;

	fds.clear();
	b=mailaccount_list.begin();
	e=mailaccount_list.end();

	while (b != e)
	{
		// mail::account may destroy itself, so increment the iterator
		// before invoking the method

		cur=b;
		b++;

		if (*cur == NULL)
			mailaccount_list.erase(cur);
		else
		{
			(*cur)->handler(fds, timeout);
		}
	}

	// Postponed handlers.
	mail::runLater::checkRunLater(timeout);
}

//
// Resume after suspend.
//

void mail::account::resume()
{
	list<mail::account *>::iterator b, e, cur;

	b=mailaccount_list.begin();
	e=mailaccount_list.end();

	while (b != e)
	{
		// mail::account may destroy itself, so increment the iterator
		// before invoking the method

		cur=b;
		++b;

		if (*cur)
			(*cur)->resumed();
	}
}

void mail::account::updateNotify(bool enableDisable, callback &callbackArg)
{
	callbackArg.success("OK");
}

void mail::account::moveMessagesTo(const std::vector<size_t> &messages,
				   mail::folder *copyTo,
				   mail::callback &callback)
{
	generic::genericMoveMessages(this, messages, copyTo, callback);
}

#if 0
void mail::account::removeMessages(const std::vector<size_t> &messages,
				   callback &cb)
{
	cb.fail("TODO");
}

#endif

void mail::account::updateKeywords(const vector<size_t> &messages,
				   const set<string> &keywords,
				   bool setOrChange,
				   // false: set, true: see changeTo
				   bool changeTo,
				   mail::callback &cb)
{
	cb.fail("Not implemented");
}

void mail::account::updateKeywords(const vector<size_t> &messages,
				   const vector<string> &keywords,
				   bool setOrChange,
				   bool changeTo,
				   mail::callback &cb)
{
	set<string> s;

	s.insert(keywords.begin(), keywords.end());

	updateKeywords(messages, s, setOrChange, changeTo, cb);
}


void mail::account::updateKeywords(const vector<size_t> &messages,
				   const list<string> &keywords,
				   bool setOrChange,
				   bool changeTo,
				   mail::callback &cb)
{
	set<string> s;

	s.insert(keywords.begin(), keywords.end());

	updateKeywords(messages, s, setOrChange, changeTo, cb);
}

void mail::account::getFolderKeywordInfo(size_t messageNum,
					 set<string> &keywordRet)
{
	keywordRet.clear();
}




//
// Must provide default methods for mail::callback::message, not all
// subclasses must implement every method.
//

mail::callback::message::message()
{
}

mail::callback::message::~message()
{
}

void mail::callback::message::messageEnvelopeCallback(size_t n,
						      const mail::envelope
						      &env)
{
}

void mail::callback::message::messageReferencesCallback(size_t n,
							const vector<string>
							&references)
{
}

void mail::callback::message::messageStructureCallback(size_t n,
							 const mail::mimestruct
							 &mstruct)
{
}

void mail::callback::message::messageTextCallback(size_t n, string text)
{
}

void mail::callback::message
::messageArrivalDateCallback(size_t messageNumber,
			     time_t datetime)
{
}

void mail::callback::message::messageSizeCallback(size_t messageNumber,
						    unsigned long messagesize)
{
}

LIBMAIL_START

string toutf8(string s)
{
	string u;

	char *p=libmail_u_convert_toutf8(s.c_str(),
					 unicode_default_chset(), NULL);

	try {
		u=p;
		free(p);
	} catch (...) {
		free(p);
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
	return u;
}

string fromutf8(string s)
{
	string u;

	char *p=libmail_u_convert_fromutf8(s.c_str(), unicode_default_chset(),
					   NULL);

	try {
		u=p;
		free(p);
	} catch (...) {
		free(p);
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
	return u;
}
LIBMAIL_END

//
// Common code for mbox and maildir
//

string mail::mbox::translatePathCommon(string path,
				       const char *verbotten,
				       const char *sep)
{
	string newpath;

	do
	{
		string component;

		size_t n=path.find('/');

		if (n == std::string::npos)
		{
			component=path;
			path="";
		}
		else
		{
			component=path.substr(0, n);
			path=path.substr(n+1);
		}

		vector<unicode_char> ucvec;

		unicode_char *uc;
		size_t ucsize;
		libmail_u_convert_handle_t h;

		if ((h=libmail_u_convert_tou_init(unicode_default_chset(),
						  &uc, &ucsize, 1)) == NULL)
		{
			uc=NULL;
		}
		else
		{
			libmail_u_convert(h, component.c_str(),
					  component.size());

			if (libmail_u_convert_deinit(h, NULL))
				uc=NULL;
		}

		if (!uc)
		{
			errno=EINVAL;
			return "";
		}

		try {
			size_t n;

			for (n=0; uc[n]; n++)
				;

			ucvec.insert(ucvec.end(), uc, uc+n);

			for (n=0; n<ucvec.size(); )
			{
				if (ucvec[n] == '\\' && n+1 < ucvec.size())
				{
					ucvec.erase(ucvec.begin()+n,
						    ucvec.begin()+n+1);
					n++;
					continue;
				}
				if (ucvec[n] == '%')
				{
					unicode_char ucnum=0;
					size_t o=n+1;

					while (o < ucvec.size())
					{
						if ((unsigned char)ucvec[o]
						    != ucvec[o])
							break;
						if (!isdigit(ucvec[o]))
							break;

						ucnum=ucnum * 10 +
							ucvec[o]-'0';
						++o;
					}

					if (o < ucvec.size() &&
					    ucvec[o] == ';')
						++o;

					ucvec[n++]=ucnum;
					ucvec.erase(ucvec.begin()+n,
						    ucvec.begin()+o);
					continue;
				}
				n++;
			}
		} catch (...) {
			free(uc);
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}

		free(uc);
		std::string p(mail::iconvert::convert(ucvec,
						      std::string(unicode_x_imap_modutf7 " ")
						      + verbotten));


		if (newpath.size() > 0)
			newpath += sep;
		newpath += p;
	} while (path.size() > 0);

	return newpath;
}
