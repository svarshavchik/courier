/*
** Copyright 2003-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "myfolder.H"
#include "mymessage.H"
#include "myserver.H"
#include "specialfolder.H"
#include "addressbook.H"
#include "gettext.H"
#include "htmlparser.H"
#include "init.H"
#include "libmail/rfc2047encode.H"
#include "libmail/rfc2047decode.H"
#include "libmail/misc.H"

#include "myservercallback.H"
#include "myserverpromptinfo.H"
#include "libmail/rfcaddr.H"
#include "curses/cursesstatusbar.H"

#include <errno.h>
#include <strings.h>
#include <unistd.h>
#include <pwd.h>

#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sstream>
#include <iomanip>
#include <fstream>
#include <iostream>
#include <algorithm>

using namespace std;

extern CursesStatusBar *statusBar;

extern void editScreen(void *);
extern void hierarchyScreen(void *);

//
// Callback for reading message information.

class myMessageCallback : public mail::callback::message,
			  public myServer::Callback {
	myMessage *m;

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);

public:
	myMessageCallback(myMessage *mArg);
	~myMessageCallback();

	myMessage::ReadText *read;

	void messageEnvelopeCallback(size_t messageNumber,
				     const class mail::envelope
				     &envelope);

	void messageStructureCallback(size_t messageNumber,
				      const class mail::mimestruct
				      &messageStructure);
	void messageTextCallback(size_t n, string text);

	void success(string message);
	void fail(string message);
};

myMessageCallback::myMessageCallback(myMessage *mArg)
	: m(mArg), read(NULL)
{
}

myMessageCallback::~myMessageCallback()
{
}

void myMessageCallback::reportProgress(size_t bytesCompleted,
				       size_t bytesEstimatedTotal,

				       size_t messagesCompleted,
				       size_t messagesEstimatedTotal)
{
	myServer::reportProgress(bytesCompleted,
				 bytesEstimatedTotal,
				 messagesCompleted,
				 messagesEstimatedTotal);
}

void myMessageCallback::messageEnvelopeCallback(size_t messageNumber,
				     const class mail::envelope
				     &envelope)
{
	m->envelope=envelope;
}

void myMessageCallback::messageStructureCallback(size_t messageNumber,
						 const class mail::mimestruct
						 &messageStructure)
{
	m->structure=messageStructure;
}

void myMessageCallback::messageTextCallback(size_t n, string text)
{
	if (read)
		(*read)(text);
}

void myMessageCallback::success(string message)
{
	myServer::Callback::success(message);
}

void myMessageCallback::fail(string message)
{
	myServer::Callback::fail(message);
}

myMessage *myMessage::theMessage=NULL;

myMessage::myMessage(mail::account *myserverArg) : serverPtr(myserverArg),
						   myfolder(NULL),
						   tmpaccount(NULL),
						   usingTmpaccount(false),
						   orig_status_code(' '),
						   haveAttributes(false)

{
	if (theMessage)
		delete theMessage;

	theMessage=this;
}

mail::account *myMessage::getMessageSource(myServer::Callback &cb,
					   size_t &messageNumRet)
{
	messageNumRet=messagenum;

	if (tmpaccount && usingTmpaccount) // Decrypted message.
	{
		messageNumRet=0;
		return tmpaccount;
	}

	if (serverPtr.isDestroyed())
	{
		cb.fail("Server connection closed.");
		return NULL;
	}

	return serverPtr;
}

myMessage::~myMessage()
{
	if (tmpaccount)
		delete tmpaccount;

	theMessage=NULL;
	if (myfolder)
		myfolder->mymessage=NULL;

	vector<string>::iterator b=tmpfiles.begin(), e=tmpfiles.end();

	while (b != e)
		unlink ( (*b++).c_str());
}

void myMessage::disconnected(const char *errmsg)
{
}

void myMessage::servererror(const char *errmsg)
{
}

void myMessage::newMessages()
{
}

void myMessage::messagesRemoved(std::vector< std::pair<size_t, size_t> > &dum)
{
}

void myMessage::messageChanged(size_t n)
{
}

void myMessage::folderResorted()
{
	size_t n=myfolder->size();

	size_t i;

	for (i=0; i<n; i++)
		if (myfolder->getServerIndex(i) == messagenum)
		{
			messagesortednum=i;
			break;
		}

	char status_code=myfolder->getIndex(messagesortednum).status_code;

	// Opening a message drops its unread status, therefore update
	// the shown status only in all other circumstances.

	if (orig_status_code != 'N' || status_code != ' ')
		orig_status_code=status_code;

}

bool myMessage::readAttributes()
{
	if (haveAttributes)
		return true;

	orig_status_code=myfolder->getIndex(messagesortednum).status_code;

	myMessageCallback callback(this);

	vector<size_t> msg;

	size_t n;

	mail::account *myserver=getMessageSource(callback, n);
	msg.push_back(n);

	if (!myserver)
		return false;

	myserver->readMessageAttributes(msg, mail::account::ENVELOPE |
					mail::account::MIMESTRUCTURE,
					callback);

	if (myServer::eventloop(callback))
	{
		haveAttributes=true;
		return true;
	}
	return false;
}

bool myMessage::readMessage(mail::mimestruct *info,
			    mail::readMode readType,
			    myMessage::ReadText &text)
{
	myMessageCallback callback(this);
	callback.read= &text;

	size_t n;

	mail::account *myserver=getMessageSource(callback, n);

	if (myserver)
	{
		if (info == NULL)
		{
			vector<size_t> dummy;

			dummy.push_back(n);
			myserver->readMessageContent(dummy, false,
						     readType,
						     callback);
		}
		else
			myserver->readMessageContent(n, false, *info,
						     readType,
						     callback);
	}
	return myServer::eventloop(callback);
}

bool myMessage::readMessage(myMessage::ReadText &text)
{
	myMessageCallback callback(this);
	callback.read= &text;

	vector<size_t> a;

	size_t n;

	mail::account *myserver=getMessageSource(callback, n);

	a.push_back(n);

	if (myserver)
		myserver->readMessageContent(a, false,
					     mail::readBoth, callback);

	return myServer::eventloop(callback);
}


bool myMessage::readMessageContent(mail::mimestruct &info,
				   myMessage::ReadText &text)
{
	myMessageCallback callback(this);
	callback.read= &text;

	size_t n;

	mail::account *myserver=getMessageSource(callback, n);

	if (myserver)
		myserver->readMessageContentDecoded(n, false, info,
						    callback);

	return myServer::eventloop(callback);
}

void myMessage::copyContentsTo(mail::folder *f, myServer::Callback &cb)
{
	vector<size_t> msgList;

	size_t n;
	bool wasUsingTmpaccount=usingTmpaccount;

	try {
		usingTmpaccount=false; // Copy original, unencrypted source

		mail::account *myserver=getMessageSource(cb, n);
		usingTmpaccount=wasUsingTmpaccount;

		msgList.push_back(n);

		if (myserver)
			myserver->copyMessagesTo(msgList, f, cb);
	} catch (...) {
		usingTmpaccount=wasUsingTmpaccount;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}


void myMessage::createAttFilename(string &tmpfile, string &attfile,
				  string uniq)
{
	string uniq2="";
	size_t cnt=0;


	for (;;)
	{
		string buffer;

		{
			ostringstream o;

			o << time(NULL);
			buffer=o.str();
		}

		tmpfile=myServer::getConfigDir() + "/att" + buffer + uniq
			+ uniq2 + ".tmp";
		attfile=myServer::getConfigDir() + "/att" + buffer + uniq
			+ uniq2 + ".txt";

		struct stat stat_buf;

		if (stat(attfile.c_str(), &stat_buf))
			break;

		{
			ostringstream o;

			o << setw(6) << setfill('0') << ++cnt;
			buffer=o.str();
		}
		uniq2=buffer;
	}
}

//
// If when checking for autosaved message we find a pending draft, and
// a user wants to open the draft folder, we need to unwind the stack and
// exit the folder index screen (if that's where we are), because in the
// folder index screen we have IDLE enabled, and we shouldn't switch folders
// like that.
//

static void goDraftFolder(void *dummy)
{
	myServer::nextScreen= &hierarchyScreen;
	myServer::nextScreenArg=NULL; // The default

	CursesMainScreen::Lock logoutLock(mainScreen, true);

	myServer *serverPtr;
	mail::folder *f=SpecialFolder::folders.find(DRAFTS)
		->second.getFolder(serverPtr);

	if (!f)
	{
		statusBar->clearstatus();
		statusBar->status(strerror(errno));
		statusBar->beepError();
		return;
	}

	serverPtr->openFolder(f, true);
	delete f;
}

//
// Check for any autosaved copy of a message, meaning that we crashed while
// composing a message.  Salvage it.
//

bool myMessage::checkInterrupted(bool ignoreDrafts)
{
	string messagetxt=myServer::getConfigDir() + "/message.txt";

	if (access(messagetxt.c_str(), R_OK))
	{
		if (ignoreDrafts)
			return true; // Opening a saved draft message

		mail::callback::folderInfo folderInfo;

		myServer *serverPtr;
		mail::folder *f=SpecialFolder::folders.find(DRAFTS)
			->second.getFolder(serverPtr);

		if (!f)
			return false;

		try {
			myServer::Callback callback;

			callback.noreport=true;

			f->readFolderInfo(folderInfo, callback);

			if (!myServer::eventloop(callback))
			{
				delete f;
				return true;
			}
			statusBar->clearstatus();

			if (folderInfo.messageCount > 0)
			{
				myServer::promptInfo prompt=myServer::
					prompt(myServer::
					       promptInfo(Gettext(_("You have %1% message(s) in %2%, open? (Y/N) "))
							  << folderInfo.messageCount
							  << f->getName())
					       .yesno());

				if (prompt.abortflag)
				{
					delete f;
					return false;
				}

				if ((string)prompt != "Y")
				{
					delete f;
					return true;
				}
				delete f;
				myServer::nextScreen= &goDraftFolder;
				myServer::nextScreenArg=NULL;
				Curses::keepgoing=false;
				return false;
			}
			delete f;
		} catch (...) {
			delete f;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}
		return true;
	}

	myServer::promptInfo prompt=myServer::
		prompt(myServer::
		       promptInfo(_("An interrupted message was preserved, resume? (Y/N) "))
		       .yesno());

	if (prompt.abortflag)
		return false;

	if (!((string)prompt == "Y"))
		return false;

	Curses::keepgoing=false;
        myServer::nextScreen= &editScreen;
        myServer::nextScreenArg=NULL;
	return false;
}

//
// Write a new message.
//

void myMessage::newMessage(const mail::folder *folderPtr,
			   myServer *serverPtr,
			   std::string mailtoUrl)
{
	string fromhdr="";
	string replytohdr="";

	string from="", replyto="";
	string fcc;
	string customheaders="";

	if (!getDefaultHeaders(folderPtr, serverPtr, from, replyto, fcc,
			       customheaders))
		return;

	clearAttFiles();

	string messagetxt=myServer::getConfigDir() + "/message.txt";
	string messagetmp=myServer::getConfigDir() + "/message.tmp";

	{
		ofstream o(messagetmp.c_str());

		if (serverPtr)
			o << "X-Server: "
			  << (string)mail::rfc2047::encode(serverPtr->url,
							   "iso-8859-1")
			  << "\n";
		if (folderPtr)
			o << "X-Folder: "
			  << (string)mail::rfc2047::encode(folderPtr
							   ->getPath(),
							   "iso-8859-1")
			  << "\n";

		{
			string n;

			if (serverPtr && folderPtr &&
			    (n=folderPtr->isNewsgroup()).size() > 0)
				o << "Newsgroups: " << n << "\n";
		}

		if (fcc.size() > 0)
			o << "X-Fcc: "
			  << (string)mail::rfc2047::encode(fcc, "UTF-8")
			  << "\n";

		if (customheaders.size() > 0)
			o << customheaders << "\n";

		o << "From: " << from << "\n"
		  << "Reply-To: " << replyto << "\n\n";

		if (mailtoUrl.size() > 0)
		{
			std::string params;

			size_t p=mailtoUrl.find('?');

			if (p != std::string::npos)
			{
				params=mailtoUrl.substr(p+1);
				mailtoUrl=mailtoUrl.substr(0, p);
			}

			newMessageAddressHdr(mailtoUrl, "To: ", o);

			map<string, vector<unicode_char> > args;

			{
				vector<unicode_char> uparams;

				htmlParser::urlDecode(params, uparams);

				htmlParser::cgiDecode(uparams, args);
			}

			vector<unicode_char> &s=args["subject"];

			string ss=mail::iconvert::convert(s, "utf-8");

			if (ss.size() > 0)
			{
				ss=mail::rfc2047::encode(ss, "utf-8");

				o << "Subject: " <<
					ss.c_str() << "\n";
			}
		}

		o.flush();

		if (o.bad() || o.fail() ||
		    (o.close(), rename(messagetmp.c_str(), messagetxt.c_str())
		     ) < 0)
		{
			statusBar->clearstatus();
			statusBar->status(strerror(errno));
			statusBar->beepError();
			return;
		}
	}

	Curses::keepgoing=false;
	myServer::nextScreen=editScreen;
	myServer::nextScreenArg=NULL;
}

void myMessage::newMessageAddressHdr(string addresses,
				     string hdrname,
				     ostream &o)
{
	size_t errIndex;

	vector<mail::emailAddress> addrvecNative;

	{
		vector<mail::address> addrvec;

		if (!mail::address::fromString(addresses, addrvec, errIndex))
		{
			o << hdrname << addresses << "\n";
			return; // If addresses don't parse, punt.
		}

		/*
		** We obtained the addresses in their native form, so convert
		** them properly.
		*/

		addrvecNative.reserve(addrvec.size());

		vector<mail::address>::iterator b, e;

		for (b=addrvec.begin(), e=addrvec.end(); b != e; ++b)
		{
			mail::emailAddress addr;

			addr.setDisplayName(b->getName(),
					    unicode_default_chset());
			addr.setDisplayAddr(b->getAddr(),
					    unicode_default_chset());
			addrvecNative.push_back(addr);
		}
	}

	if (addrvecNative.size() == 0)
		return;

	{
		vector<mail::emailAddress> newAddrVec;

		if (AddressBook::searchAll(addrvecNative, newAddrVec))
			addrvecNative=newAddrVec;
	}

	o << mail::address::toString(hdrname, addrvecNative) << "\n";
}


//
// Get default set of headers for this folder.
//

bool myMessage::getDefaultHeaders(const mail::folder *folder,
				  myServer *s,
				  string &from, string &replyto,
				  string &sentfolder,
				  string &customheaders)
{

	SpecialFolder &configSentFolder=
		SpecialFolder::folders.find(SENT)->second;

	if (configSentFolder.folderList.size() == 0)
	{
		// Initialize the default...

		myServer *sentServer;

		mail::folder *f=configSentFolder.getFolder(sentServer);

		if (f)
		{
			delete f;
			myServer::saveconfig();
		}
		else
		{
			Curses::keepgoing=false;
			myServer::nextScreen= &hierarchyScreen;
			myServer::nextScreenArg=NULL;
			PreviousScreen::previousScreen();
			return false;
		}
	}

	from="";
	replyto="";
	sentfolder="";
	customheaders="";

	if (folder && s)
	{
		from=s->getFolderConfiguration(folder, "FROM");

		replyto=s->getFolderConfiguration(folder, "REPLY-TO");

		sentfolder=s->getFolderConfiguration(folder, "FCC");

		customheaders=s->getFolderConfiguration(folder, "CUSTOM");
	}

	if (from.size() == 0 && s)
	{
		from=s->getServerConfiguration("FROM");
		replyto=s->getServerConfiguration("REPLY-TO");
		customheaders=s->getServerConfiguration("CUSTOM");
	}

	if (from.size() == 0)
	{
		std::vector<myServer *>::iterator
			b=myServer::server_list.begin(),
			e=myServer::server_list.end();

		while (b != e && from.size() == 0)
		{
			myServer *ss= *b++;

			from=ss->getServerConfiguration("FROM");
			replyto=ss->getServerConfiguration("REPLY-TO");
			customheaders=ss->getServerConfiguration("CUSTOM");
		}
	}

	if (sentfolder.size() == 0 && s)
		sentfolder=s->getServerConfiguration("FCC");

	if (sentfolder.size() == 0)
	{
		sentfolder=configSentFolder.folderList[0].nameUTF8;
	}
	else
	{
		sentfolder=mail::rfc2047::decoder::decodeSimple(sentfolder);
	}

	if (from.size() == 0)
	{
		string n="", a;
		struct passwd *pw=getpwuid(getuid());

		a= string(pw ? pw->pw_name:"nobody") + "@" +
			mail::hostname();

		if (pw)
		{
			n=pw->pw_gecos;

			size_t p=n.find(',');

			if (p != std::string::npos)
				n=n.substr(0, std::string::npos);

			mail::emailAddress addr;

			addr.setDisplayName(n, unicode_default_chset());
			addr.setAddr(a);
			from=addr.toString();
		}
	}
	return true;
}

// Sort names of attachments.

class myMessage::ReadAttFilesSort {
public:
	bool operator()(const pair<string, time_t> &a,
			const pair<string, time_t> &b)
	{
		if (a.second != b.second)
			return (a.second < b.second);

		return (a.first < b.first);
	}
};

void myMessage::readAttFiles(vector<string> &filenameList)
{
	string d=myServer::getConfigDir();

	DIR *dirp=opendir(d.c_str());

	vector< pair<string, time_t> > attachments;

	try {
		struct dirent *de;

		while (dirp && (de=readdir(dirp)) != NULL)
		{
			const char *p;

			if (strncmp(de->d_name, "att", 3))
				continue;

			p=strrchr(de->d_name, '.');

			if (!p || strcmp(p, ".txt"))
				continue;

			string n=d + "/" + de->d_name;

			struct stat stat_buf;

			if (stat(n.c_str(), &stat_buf) < 0)
				stat_buf.st_mtime=0;

			attachments.push_back(make_pair(n, stat_buf.st_mtime));
		}

		if (dirp)
			closedir(dirp);
	} catch (...) {
		if (dirp)
			closedir(dirp);
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	sort(attachments.begin(), attachments.end(),
	     ReadAttFilesSort() );
	vector< pair<string, time_t> >::iterator b, e;

	b=attachments.begin();
	e=attachments.end();

	while (b != e)
	{
		string s= b->first;

		b++;

		filenameList.push_back(s);
	}
}

//
// Get rid of any leftover existing scrap files.
//

void myMessage::clearAttFiles()
{
	string d=myServer::getConfigDir();

	DIR *dirp=opendir(d.c_str());

	try {
		struct dirent *de;

		while (dirp && (de=readdir(dirp)) != NULL)
		{
			const char *p=strrchr(de->d_name, '.');

			if (!p || strcmp(p, ".txt"))
				continue;

			string n=d + "/" + de->d_name;

			unlink(n.c_str());
		}

		if (dirp)
			closedir(dirp);
	} catch (...) {
		if (dirp)
			closedir(dirp);
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

myMessage::ReadText::ReadText()
{
}

myMessage::ReadText::~ReadText()
{
}

//////////////////////////////////////////////////////////////////////////
//
// Helper class used by CursesMessageDisplay to update the title bar


myMessage::FolderUpdateCallback *
myMessage::FolderUpdateCallback::currentCallback=NULL;

myMessage::FolderUpdateCallback::FolderUpdateCallback()
{
	currentCallback=this;
}

myMessage::FolderUpdateCallback::~FolderUpdateCallback()
{
	currentCallback=NULL;
}

//////////////////////////////////////////////////////////////////////////
//
// GnuPG-related stuff


bool myMessage::isSigned()
{
	return tmpaccount == NULL // Already decrypted
		&& readAttributes() && isSignedEncrypted(structure, false);
}

bool myMessage::isEncrypted()
{
	return tmpaccount == NULL // Already decrypted
		&& readAttributes() && isSignedEncrypted(structure, true);
}

bool myMessage::isSignedEncrypted(mail::mimestruct &s, bool isEnc)
{
	if (isEnc ? s.multipartencrypted():s.multipartsigned())
		return true;

	size_t n=s.getNumChildren();
	size_t i;

	for (i=0; i<n; i++)
		if (isSignedEncrypted(*s.getChild(i), isEnc))
			return true;
	return false;
}
