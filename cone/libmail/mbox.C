/*
** Copyright 2002-2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "driver.H"
#include "file.H"
#include "misc.H"
#include "mbox.H"
#include "mboxopen.H"
#include "mboxread.H"
#include "mboxexpunge.H"
#include "mboxgetmessage.H"
#include "search.H"
#include "copymessage.H"
#include "mboxmultilock.H"
#include "liblock/config.h"
#include "liblock/mail.h"

#include "rfc822/rfc822.h"
#include "rfc2045/rfc2045.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>

using namespace std;

/////////////////////////////////////////////////////////////////////////////

LIBMAIL_START

static bool open_inbox(mail::account *&accountRet,
		       mail::account::openInfo &oi,
		       mail::callback &callback,
		       mail::callback::disconnect &disconnectCallback)
{
	if (oi.url.substr(0, 6) != "inbox:")
		return false;

	accountRet=new mail::mbox(true, oi.url.substr(6),
				  callback,
				  disconnectCallback);

	return true;
}

static bool open_mbox(mail::account *&accountRet,
		      mail::account::openInfo &oi,
		      mail::callback &callback,
		      mail::callback::disconnect &disconnectCallback)
{
	if (oi.url.substr(0, 5) != "mbox:")
		return false;

	accountRet=new mail::mbox(false, oi.url.substr(5),
				  callback,
				  disconnectCallback);

	return true;
}

static bool mbox_remote(string url, bool &flag)
{
	if (url.substr(0, 6) == "inbox:" ||
	    url.substr(0, 5) == "mbox:")
	{
		flag=false;
		return true;
	}

	return false;
}

driver inbox_driver= { &open_inbox, &mbox_remote };
driver mbox_driver= { &open_mbox, &mbox_remote };

LIBMAIL_END

/////////////////////////////////////////////////////////////////////////////

mail::mbox::task::task(mail::mbox &mboxArg)
	: mboxAccount(mboxArg)
{
}

mail::mbox::task::~task()
{
}

void mail::mbox::task::done()
{
	if (mboxAccount.tasks.front() != this)
		LIBMAIL_THROW("Assertion failed: mail::mbox::task::done");

	mboxAccount.tasks.pop();
	delete this;
}

/////////////////////////////////////////////////////////////////////////////

mail::mbox::lock::lock()
	: ll(NULL), fd(-1), readOnlyLock(false)
{
}

mail::mbox::lock::lock(string filename)
	: ll(ll_mail_alloc(filename.c_str())), fd(-1), readOnlyLock(false)
{
	if (!ll)
		LIBMAIL_THROW("Out of memory.");
}

mail::mbox::lock *mail::mbox::lock::copy()
{
	lock *c=new lock();

	if (!c)
		LIBMAIL_THROW(strerror(errno));

	c->ll=ll;
	c->fd=fd;
	c->readOnlyLock=readOnlyLock;

	ll=NULL;
	fd=-1;

	return c;
}

mail::mbox::lock::~lock()
{
	if (fd >= 0)
		close(fd);
	if (ll)
		ll_mail_free(ll);
}

bool mail::mbox::lock::operator()(bool readOnly)
{
	if (fd >= 0)
		return true;

	if (!ll)
	{
		errno=ENOENT;
		return false;
	}

	// Create a dot-lock file, first.

	if (ll_mail_lock(ll) < 0 && !readOnly)
		return false;

	// Now, open the file.

	fd=readOnly ? ll_mail_open_ro(ll):ll_mail_open(ll);
	readOnlyLock=readOnly;

	return fd >= 0;
}

/////////////////////////////////////////////////////////////////////////////


mail::mbox::TimedTask::TimedTask(mail::mbox &mboxArg,
				 mail::callback &callbackArg,
				 int timeoutArg)
	: task(mboxArg), timeout(time(NULL) + timeoutArg), nextTry(0),
	  callback(callbackArg)
{
}

mail::mbox::TimedTask::~TimedTask()
{
}

void mail::mbox::TimedTask::doit(int &timeout)
{
	time_t now=time(NULL);

	if (nextTry && now < nextTry)
	{
		int nms=(nextTry - now) * 1000;

		if (timeout > nms)
			timeout=nms;
		return;
	}

	timeout=0;

	if (doit())
		return;

	nextTry = now + 5; // Try again in 5 seconds.

	if (now >= timeout)
		timedOut();
}

void mail::mbox::TimedTask::fail(string errmsg)
{
	callback.fail(errmsg);
	done();
}

void mail::mbox::TimedTask::timedOut()
{
	callback.fail("Operation timed out - mail folder in use.");
	done();
}

void mail::mbox::TimedTask::disconnected()
{
	callback.fail("Operation cancelled.");
}

/////////////////////////////////////////////////////////////////////////////

void mail::mbox::resumed()
{
}

void mail::mbox::handler(vector<pollfd> &fds, int &timeout)
{
	// Our job is to do the first task at hand, as simple as that.

	if (!tasks.empty())
		tasks.front()->doit(timeout);
}

void mail::mbox::installTask(task *t)
{
	if (t == NULL)
		LIBMAIL_THROW("Out of memory.");

	try {
		tasks.push(t);
	} catch (...) {
		delete t;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

mail::mbox::mbox(bool magicInboxArg,
		       string folderRoot, mail::callback &callback,
		       mail::callback::disconnect &disconnect_callback)
	: mail::account(disconnect_callback),
	  calledDisconnected(false),
	  magicInbox(magicInboxArg),
	  inboxFolder("INBOX", *this),
	  hierarchyFolder("", *this),
	  currentFolderReadOnly(false),
	  folderSavedSize(0),
	  folderSavedTimestamp(0),
	  multiLockLock(NULL),
	  folderDirty(false),
	  newMessages(false),
	  cachedMessageRfcp(NULL),
	  cachedMessageFp(NULL),
	  currentFolderCallback(NULL)
{
	sigset_t ss;

	// Ignore SIGUSR2 from c-client

	sigemptyset(&ss);
	sigaddset(&ss, SIGUSR2);
	sigprocmask(SIG_BLOCK, &ss, NULL);

	const char *m=getenv("MAIL");

	string h=mail::homedir();
	struct passwd *pw=getpwuid(getuid());

	if (!pw || h.size() == 0)
	{
		callback.fail("Cannot find my home directory!");
		return;
	}


	inboxMailboxPath=h + "/Inbox";

	// Figure out the mail spool directory.

	if (m && *m)
		inboxSpoolPath=m;
	else
	{
		static const char *spools[]={"/var/spool/mail", "/var/mail",
					     "/usr/spool/mail", "/usr/mail",
					     0};

		size_t i;

		for (i=0; spools[i]; i++)
			if (access(spools[i], X_OK) == 0)
			{
				inboxSpoolPath=string(spools[i]) + "/"
					+ pw->pw_name;
				break;
			}

		if (!spools[i])
		{
			callback.fail("Cannot determine your system mailbox location,");
			return;
		}
	}

	if (folderRoot.size() > 0 && folderRoot[0] != '/')
		folderRoot=h + "/" + folderRoot;

	rootPath=folderRoot;

	// Initialize the top level folder.

	hierarchyFolder.path=folderRoot;
	hierarchyFolder.name=folder::defaultName(folderRoot);

	// First time through, create the top level folder directory, if
	// necessary.

	if (magicInboxArg)
		mkdir(folderRoot.c_str(), 0700);

	struct stat stat_buf;

	if (stat(folderRoot.c_str(), &stat_buf) == 0 &&
	    S_ISDIR(stat_buf.st_mode))
	{
		hierarchyFolder.hasMessages(false);
		hierarchyFolder.hasSubFolders(true);
	}
	else
	{
		hierarchyFolder.hasMessages(true);
		hierarchyFolder.hasSubFolders(false);
	}

	callback.success("Mail folder opened.");
}

mail::mbox::~mbox()
{
	resetFolder();

	while (!tasks.empty())
	{
		tasks.front()->disconnected();
		tasks.front()->done();
	}

	if (!calledDisconnected)
	{
		calledDisconnected=true;
		disconnect_callback.disconnected("");
	}
}

//
// General cleanup when the current folder is closed.
//

void mail::mbox::resetFolder()
{
	if (multiLockLock)
	{
		delete multiLockLock;
		multiLockLock=NULL;
	}

	if (cachedMessageRfcp)
	{
		rfc2045_free(cachedMessageRfcp);
		cachedMessageRfcp=NULL;
	}

	if (cachedMessageFp)
	{
		fclose(cachedMessageFp);
		cachedMessageFp=NULL;
	}

	cachedMessageUid="";

	folderMessageIndex.clear();
	uidmap.clear();
	folderDirty=false;
	newMessages=false;
}

void mail::mbox::logout(mail::callback &callback)
{
	if (!folderDirty)
	{
		callback.success("Mail folder closed.");
		return;
	}

	// Something dirty needs to be saved.

	installTask(new ExpungeTask(*this, callback, false, NULL));
}

void mail::mbox::checkNewMail(class mail::callback &callback)
{
	if (currentFolder.size() == 0)
	{
		callback.success("OK");	// Nothing's opened.
		return;
	}

	installTask(new CheckNewMailTask(*this, currentFolder,
					 callback, NULL));
}

void mail::mbox::checkNewMail()
{
	// The real folder contents have been updated, now compare against
	// the folder contents seen by the app, and create entries for
	// new msgs

	set<string> uidSet;

	// Create an index of all existing msgs seen by the app.

	{
		vector<mail::messageInfo>::iterator b, e;

		b=index.begin();
		e=index.end();

		while (b != e)
			uidSet.insert( (*b++).uid );
	}

	// Step through the real folder index, see what's new.

	{
		vector<mboxMessageIndex>::iterator b, e;

		b=folderMessageIndex.begin();
		e=folderMessageIndex.end();

		while (b != e)
		{
			mail::messageInfo i=(*b++).tag.getMessageInfo();

			if (uidSet.count(i.uid))
				continue;

			index.push_back(i);
			newMessages=true;
		}
	}
}

bool mail::mbox::hasCapability(string capability)
{
	if (capability == LIBMAIL_SINGLEFOLDER)
		return hierarchyFolder.hasMessages();

	return false;
}

string mail::mbox::getCapability(string name)
{
	mail::upper(name);

	if (name == LIBMAIL_SERVERTYPE)
	{
		return "mbox";
	}

	if (name == LIBMAIL_SERVERDESCR)
	{
		return "Local mail folder";
	}

	if (name == LIBMAIL_SINGLEFOLDER)
		return hierarchyFolder.hasMessages() ? "1":"";

	return "";
}

mail::folder *mail::mbox::folderFromString(string s)
{
	string name="";

	string::iterator b=s.begin(), e=s.end();

	while (b != e)
	{
		if (*b == '\\')
		{
			b++;

			if (b == e)
				break;
		} else if (*b == ':')
		{
			b++;
			break;
		}

		name += *b++;
	}

	// If the path component is relative, prepend home dir.

	string path=string(b, e);

	if (path.size() == 0)
		path=rootPath;
	else if (path[0] != '/' && path != "INBOX")
		path=rootPath + "/" + path;

	return new folder(path, *this);
}

void mail::mbox::readTopLevelFolders(mail::callback::folderList &callback1,
				     class mail::callback &callback2)
{
	vector<const mail::folder *> folder_list;

	if (magicInbox)
		folder_list.push_back( &inboxFolder );

	if (rootPath.size() > 0)
		folder_list.push_back( &hierarchyFolder );

	callback1.success(folder_list);
	callback2.success("OK");
}

void mail::mbox::findFolder(string path,
			    mail::callback::folderList &callback1,
			    mail::callback &callback2)
{
	if (path.size() == 0)
		path=rootPath;
	else if (path[0] != '/' && path != "INBOX")
		path=rootPath + "/" + path;

	folder *f=new folder(path, *this);

	if (!f)
	{
		callback2.fail(path + ": " + strerror(errno));
		return;
	}

	try {
		vector<const mail::folder *> folder_list;

		folder_list.push_back(f);

		callback1.success(folder_list);
		callback2.success("OK");
		delete f;
	} catch (...) {
		delete f;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

void mail::mbox::readMessageAttributes(const vector<size_t> &messages,
				       MessageAttributes attributes,
				       mail::callback::message
				       &callback)
{
	vector<size_t> messageCpy=messages;

	// ARRIVALDATE can be handled here, for everything else use the
	// generic methods.

	if (attributes & mail::account::ARRIVALDATE)
	{
		attributes &= ~mail::account::ARRIVALDATE;

		vector<size_t>::iterator b=messageCpy.begin(),
			e=messageCpy.end();

		MONITOR(mail::mbox);

		while (b != e && !DESTROYED())
		{
			size_t n= *b++;

			if (n >= index.size())
				continue;

			string uid=index[n].uid;

			callback
				.messageArrivalDateCallback(n,
							    uidmap.count(uid)
							    == 0 ? 0:
							    folderMessageIndex
							    [ uidmap.find(uid)
							      ->second]
							    .internalDate);
		}

		if (DESTROYED() || attributes == 0)
		{
			callback.success("OK");
			return;
		}
	}

	MultiLockRelease *mlock=new MultiLockRelease(*this, callback);

	if (!mlock)
	{
		callback.fail(strerror(errno));
		return;
	}

	try {
		MultiLockGenericAttributes *mr=
			new MultiLockGenericAttributes(*this,
							messageCpy,
							attributes,
							*mlock);
		if (!mr)
			LIBMAIL_THROW((strerror(errno)));

		try {
			installTask(new MultiLock( *this, *mr ));
		} catch (...) {
			delete mr;
		}
	} catch (...) {
		delete mlock;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

void mail::mbox::readMessageContent(const vector<size_t> &messages,
				    bool peek,
				    enum mail::readMode readType,
				    mail::callback::message &callback)
{
	MultiLockRelease *mlock=new MultiLockRelease(*this, callback);

	if (!mlock)
	{
		callback.fail(strerror(errno));
		return;
	}

	try {
		MultiLockGenericMessageRead *mr=
			new MultiLockGenericMessageRead(*this,
							messages,
							peek,
							readType,
							*mlock);
		if (!mr)
			LIBMAIL_THROW((strerror(errno)));

		try {
			installTask(new MultiLock( *this, *mr ));
		} catch (...) {
			delete mr;
		}
	} catch (...) {
		delete mlock;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

void mail::mbox::readMessageContent(size_t messageNum,
				    bool peek,
				    const mail::mimestruct &msginfo,
				    enum mail::readMode readType,
				    mail::callback::message
				    &callback)
{
	genericReadMessageContent(this, this, messageNum, peek,
				  msginfo, readType,
				  callback);
}

void mail::mbox::readMessageContentDecoded(size_t messageNum,
					   bool peek,
					   const mail::mimestruct &msginfo,
					   mail::callback::message
					   &callback)
{
	genericReadMessageContentDecoded(this, this, messageNum, peek,
					 msginfo,
					 callback);
}

size_t mail::mbox::getFolderIndexSize()
{
	return index.size();
}

mail::messageInfo mail::mbox::getFolderIndexInfo(size_t n)
{
	return n < index.size() ? index[n]:mail::messageInfo();
}

void mail::mbox::saveFolderIndexInfo(size_t messageNum,
				     const mail::messageInfo &info,
				     mail::callback &callback)
{
	MONITOR(mail::mbox);

	if (messageNum < index.size())
	{
		folderDirty=true;

#define DOFLAG(dummy1, field, dummy2) \
		(index[messageNum].field= info.field)

		LIBMAIL_MSGFLAGS;

#undef DOFLAG

	}

	callback.success(currentFolderReadOnly ?
			 "Folder opened in read-only mode.":
			 "Message updated.");

	if (! DESTROYED() && messageNum < index.size()
	    && currentFolderCallback)
		currentFolderCallback->messageChanged(messageNum);
}

void mail::mbox::updateFolderIndexFlags(const vector<size_t> &messages,
					bool doFlip,
					bool enableDisable,
					const mail::messageInfo &flags,
					mail::callback &callback)
{
	vector<size_t>::const_iterator b, e;

	b=messages.begin();
	e=messages.end();

	size_t n=index.size();

	while (b != e)
	{
		size_t i= *b++;

		if (i < n)
		{
#define DOFLAG(dummy, field, dummy2) \
			if (flags.field) \
			{ \
				index[i].field=\
					doFlip ? !index[i].field\
					       : enableDisable; \
			}

			LIBMAIL_MSGFLAGS;
#undef DOFLAG
		}
	}

	folderDirty=true;
	b=messages.begin();
	e=messages.end();

	MONITOR(mail::mbox);

	while (!DESTROYED() && b != e)
	{
		size_t i= *b++;

		if (i < n && currentFolderCallback)
			currentFolderCallback->messageChanged(i);
	}

	callback.success(!DESTROYED() && currentFolderReadOnly ?
			 "Folder opened in read-only mode.":
			 "Message updated.");
}

void mail::mbox::genericMarkRead(size_t messageNumber)
{
	if (messageNumber < index.size() && index[messageNumber].unread)
	{
		index[messageNumber].unread=false;
		folderDirty=true;
		if (currentFolderCallback)
			currentFolderCallback->messageChanged(messageNumber);
	}
}

void mail::mbox::updateFolderIndexInfo(mail::callback &callback)
{
	if (currentFolder.size() == 0)
	{
		callback.success("Mail folder updated");
		return;
	}

	installTask(new ExpungeTask(*this, callback, true, NULL));
}

void mail::mbox::getFolderKeywordInfo(size_t messageNumber,
				      set<string> &keywords)
{
	keywords.clear();

	if (messageNumber < index.size())
	{
		map<string, size_t>::iterator p=
			uidmap.find(index[messageNumber].uid);

		if (p != uidmap.end())
		{
			folderMessageIndex[p->second].tag.getKeywords()
				.getFlags(keywords);
		}
	}
}


void mail::mbox::updateKeywords(const std::vector<size_t> &messages,
				const std::set<std::string> &keywords,
				bool setOrChange,
				// false: set, true: see changeTo
				bool changeTo,
				callback &cb)
{
	genericUpdateKeywords(messages, keywords,
			      setOrChange, changeTo, currentFolderCallback,
			      this, cb);
}

bool mail::mbox::genericProcessKeyword(size_t messageNumber,
				       generic::updateKeywordHelper &helper)
{
	if (messageNumber < index.size())
	{
		map<string, size_t>::iterator p=
			uidmap.find(index[messageNumber].uid);

		if (p != uidmap.end())
		{
			folderDirty=true;
			return helper
				.doUpdateKeyword(folderMessageIndex[p->second]
						 .tag.getKeywords(),
						 keywordHashtable);
		}
	}
	return true;
}

void mail::mbox::removeMessages(const std::vector<size_t> &messages,
				callback &cb)
{
	installTask(new ExpungeTask(*this, cb, true, &messages));
}

void mail::mbox::copyMessagesTo(const vector<size_t> &messages,
				mail::folder *copyTo,
				mail::callback &callback)
{
	mail::copyMessages::copy(this, messages, copyTo, callback);
}

void mail::mbox::searchMessages(const class mail::searchParams &searchInfo,
				mail::searchCallback &callback)
{
	mail::searchMessages::search(callback, searchInfo, this);
}

/////////////////////////////////////////////////////////////////////////////
//
// Generic message access implementation

void mail::mbox::genericMessageRead(string uid,
				    size_t messageNumber,
				    bool peek,
				    mail::readMode readType,
				    mail::callback::message &callback)
{
	installTask(new GenericReadTask(*this, callback, uid, messageNumber,
					peek, readType));
}

bool mail::mbox::verifyUid(string uid, size_t &messageNumber,
			   mail::callback &callback)
{
	if (uidmap.count(uid) > 0)
	{
		if (messageNumber < index.size() &&
		    index[messageNumber].uid == uid)
			return true;

		size_t n=index.size();

		while (n)
		{
			if (index[--n].uid == uid)
			{
				messageNumber=n;
				return true;
			}
		}
	}
	callback.fail("Message no longer exists in the folder.");
	return false;
}

void mail::mbox::genericMessageSize(string uid,
				    size_t messageNumber,
				    mail::callback::message &callback)
{
	if (!verifyUid(uid, messageNumber, callback))
		return;

	size_t n=uidmap.find(uid)->second;

	off_t s=folderMessageIndex[n].startingPos;

	off_t e=n + 1 >= folderMessageIndex.size()
		? folderSavedSize:folderMessageIndex[n+1].startingPos;

	callback.messageSizeCallback( messageNumber, e > s ? e-s:0);
	callback.success("OK");
}

void mail::mbox::genericGetMessageFd(string uid,
				     size_t messageNumber,
				     bool peek,
				     int &fdRet,
				     mail::callback &callback)
{
	if (uid == cachedMessageUid && cachedMessageFp)
	{
		fdRet=fileno(cachedMessageFp);
		callback.success("OK");
		return;
	}

	installTask(new GenericGetMessageTask(*this, callback,
					      uid, messageNumber,
					      peek,
					      &fdRet, NULL));
}

void mail::mbox::genericGetMessageStruct(string uid,
					 size_t messageNumber,
					 struct rfc2045 *&structRet,
					 mail::callback &callback)
{
	if (uid == cachedMessageUid && cachedMessageRfcp)
	{
		structRet=cachedMessageRfcp;
		callback.success("OK");
		return;
	}

	installTask(new GenericGetMessageTask(*this, callback,
					      uid, messageNumber,
					      true,
					      NULL, &structRet));
}

void mail::mbox::genericGetMessageFdStruct(string uid,
					   size_t messageNumber,
					   bool peek,
					   int &fdRet,
					   struct rfc2045 *&structret,
					   mail::callback &callback)
{
	if (uid == cachedMessageUid && cachedMessageRfcp &&
	    cachedMessageFp)
	{
		structret=cachedMessageRfcp;
		fdRet=fileno(cachedMessageFp);
		callback.success("OK");
		return;
	}

	installTask(new GenericGetMessageTask(*this, callback,
					      uid, messageNumber,
					      peek,
					      &fdRet, &structret));
}

bool mail::mbox::genericCachedUid(string uid)
{
	return uid == cachedMessageUid && cachedMessageRfcp;
}

/*
** Hack away at ctime ("Wed Sep 01 13:58:06 2002")
** in the From_ header until we end up with a timestamp
*/

static time_t fromCtime(string hdr)
{
	char mon[4];
	int dom, h, m, s, y;
	char tempbuf[100];

	struct tm *tmptr;
	time_t tv;
	int mnum;

	const char *p=hdr.c_str();

	while (*p && !unicode_isspace((unsigned char)*p))
		p++;

	p++;

	while (*p && !unicode_isspace((unsigned char)*p))
		p++;


	if (sscanf(p, "%*s %3s %d %d:%d:%d %d", mon, &dom,
		   &h, &m, &s, &y)!=6)
		return 0;

	/* Some hackery to get the month number */

	sprintf(tempbuf, "15 %s %d 12:00:00", mon, y);

	tv=rfc822_parsedt(tempbuf);

	if (tv == 0)
		return 0;

	tmptr=localtime(&tv);
	mnum=tmptr->tm_mon;

	/* For real, this time */

	sprintf(tempbuf, "%d %s %d 00:00:00", dom, mon, y);

	tv=rfc822_parsedt(tempbuf);

	if (tv == 0)
		return 0;

	tmptr=localtime(&tv);

	while ((tmptr=localtime(&tv))->tm_year + 1900 < y ||
	       (tmptr->tm_year + 1900 == y && tmptr->tm_mon < mnum) ||
	       (tmptr->tm_year + 1900 == y && tmptr->tm_mon == mnum &&
		tmptr->tm_mday < dom))
		tv += 60 * 60;

	while ((tmptr=localtime(&tv))->tm_year + 1900 > y ||
	       (tmptr->tm_year + 1900 == y && tmptr->tm_mon > mnum) ||
	       (tmptr->tm_year + 1900 == y && tmptr->tm_mon == mnum &&
		tmptr->tm_mday > dom))
		tv -= 60 * 60;

	while ((tmptr=localtime(&tv))->tm_mday == dom &&
	       tmptr->tm_hour < h)
		tv += 60 * 60;

	while ((tmptr=localtime(&tv))->tm_mday == dom &&
	       tmptr->tm_hour > h)
		tv -= 60 * 60;

	tv += m * 60 + s;

	return tv;
}

/////////////////////////////////////////////////////////////////////////////
//
// scan() is where all the exciting stuff happens.  scan() reads a file with
// messages; potentially copies the messages to another file.
//
// scan() is invoked in the following situations, where it acts as follows:
//
// A.  OPENING A NEW FOLDER
//
//     saveFile=NULL, reopening=false, deleteMsgs=NULL, rewriting=false
//
//     folderMessageIndex and uidmap are created based on the messages in
//     the file.  folderDirty is set to true if there are messages in the
//     file without an existing UID (in which case each message is assigned
//     a new UID).
//
// B.  NEW MAIL CHECK FROM SYSTEM MAILBOX
//
//     saveFile=not NULL, reopening=false, deleteMsgs=NULL, rewriting=false
//
//     Previously, scan() was called in situation A, to open $HOME/Inbox.
//     This time, saveFile is $HOME/Inbox, and scanFile is the existing file
//     is the spoolfile (/var/spool/something, usually).
//
//     If any messages are found in the spoolfile, they are copied to saveFile,
//     and are added to folderMessageIndex and uidmap.  Any existing UIDs in
//     the new messages are ignored, each new message is assigned a new UID,
//     and folderDirty is set to true.
//
// C.  REOPENING A FOLDER
//
//     saveFile=NULL, reopening=true, deleteMsgs=NULL, rewriting=false
//
//     If we believe that the file has not been touched by another process
//     (timestamp and size have not been changed), everything is left alone
//     the way it is.  Otherwise;
//
//     folderMessageIndex and uidmap are created based on the messages in
//     the file.  folderDirty is set to true if there are messages in the
//     file without an existing UID (in which case each message is assigned
//     a new UID).
//
// D.  EXPUNGING THE FOLDER
//
//     saveFile=new file, reopening=true, deleteMsgs != NULL, rewriting=true
//
//     Messages are copied to saveFile, except ones that are marked as
//     deleted.  folderMessageIndex and uidmap are updated accordingly.
//
// E.  CLOSING THE FOLDER
//
//     saveFile=new file, reopening=true, deleteMsgs=NULL, rewriting=true
//
//     All messages are copied to saveFile, with any updated flags and uids.
//

bool mail::mbox::scan(mail::file &scanFile,
		      mail::file *saveFile,
		      bool reopening,
		      set<string> *deleteMsgs,
		      bool rewriting,
		      mail::callback *progress)
{
	struct stat stat_buf;

	set<string> deleted; // All deleted UIDs

	map<string, mail::messageInfo *> updatedFlags;
	// List of all updated flags

	vector<mboxMessageIndex> newMessageIndex;
	// All new UIDs that were created for messages without UIDs.

	if (reopening)
	{
		// Initialize the list of all updated message flags,
		// as well as the deleted messages that should not be copied

		vector<mail::messageInfo>::iterator b=index.begin(),
			e=index.end();

		while (b != e)
		{
			mail::messageInfo &i=*b++;

			if (deleteMsgs != NULL)
			{
				if (deleteMsgs->count(i.uid) > 0)
					deleted.insert(i.uid);
			}

			updatedFlags.insert(make_pair(i.uid, &i));
		}
	}

	// Read the folder file, line by line.
	// Keep track of the starting points of each message.


	bool skipping=false;   // Not copying the current message

	bool copying=false;    // Message copy in progress

	bool seenFrom=false;   // Previous line was a From_ line.

	string fromhdr;

	stat_buf.st_size=0;

	off_t nextUpdatePos=0;

	off_t fromPos=0;
	size_t rewriteIndex=0;

	string fromLine;

	scanFile.seeked();

	while (!feof(scanFile))
	{
		off_t pos;	// Current line starts here

		if ((pos=scanFile.tell()) < 0)
		{
			return false;
		}

		// Every 10k bytes send an update.

		if (progress && pos >= nextUpdatePos)
		{
			nextUpdatePos=pos + 10000;
			progress->reportProgress(pos, stat_buf.st_size, 0, 1);
		}

		string line=scanFile.getline();

		if (strncmp(line.c_str(), "From ", 5) == 0)
		{
			fromhdr=line;

			// copying is false if this is the first message.

			if (!copying &&
			    saveFile)	// Copying messages.
			{
				// We're copying messages, and this is the
				// first message.  A couple of things must
				// be done:

				// Remember how big the saveFile was,
				// originally.

				if (fstat(fileno(static_cast<FILE *>
						 (*saveFile)),
					  // fileno may be a macro
					  &stat_buf) < 0)
				{
					return false;
				}

				// Make sure From of the first new
				// message begins on a new line

				if (stat_buf.st_size > 0)
				{
					if (fseek(*saveFile, -1L, SEEK_END)
					    < 0)
					{
						return false;
					}

					int c=getc(*saveFile);

					if (fseek(*saveFile, 0L, SEEK_END) < 0)
					{
						return false;
					}

					if (c != '\n')
					{
						stat_buf.st_size++;
						putc('\n', *saveFile);
					}
				}
			}

			if (!copying && // First message

			    !saveFile && reopening && !rewriting) // REOPENING
			{
				// Potential short cut.

				if (fstat(fileno(static_cast<FILE *>(scanFile)),
					  &stat_buf) < 0)
				{
					return false;
				}

				if (stat_buf.st_size == folderSavedSize &&
				    stat_buf.st_mtime ==
				    folderSavedTimestamp)
				{
					return true;
				}
			}

			if (!copying &&	// First message
			    !saveFile)	// NOT READING NEW MAIL
				resetFolder();

			copying=true;
			fromLine=line;

			if (saveFile)
			{
				if ((pos=ftell(*saveFile)) < 0)
				{
					return false;
				}
			}

			fromPos=pos;
			seenFrom=true;
			continue;
		}

		if (seenFrom) // Previous line was the From_ line.
		{
			seenFrom=false;

			skipping=false;

			// Does the first line of the message has our magic
			// tag?

			mail::mboxMagicTag tag(line, keywordHashtable);

			if (tag.good()) // Yes.
			{
				string uid=tag.getMessageInfo().uid;

				if (deleted.count(uid) > 0)
					// This one's deleted.
				{
					skipping=true;
					rewriteIndex++;
					continue;
				}

				if (saveFile)
					// Flags in the file might be stale,
					// make sure the current flags are
					// written out later.
				{
					if (updatedFlags.count(uid) > 0)
					{
						mail::messageInfo *i=
							updatedFlags
							.find(uid)->second;

						map<string, size_t>
							::iterator kw=
							uidmap.find(uid);

						tag=mail::mboxMagicTag(uid,
								       *i,
								       kw ==
								       uidmap.end() ?
								       tag.getKeywords():
								       folderMessageIndex[kw->second].tag.getKeywords());
					}
					else
						// READING NEW MAIL -- ignore
						// existing tags.

						tag=mail::mboxMagicTag();

					fprintf(*saveFile, "%s\n%s\n",
						fromLine.c_str(),
						tag.toString().c_str());
				}
			}
			else if (rewriteIndex < folderMessageIndex.size() &&
				 deleted.count(folderMessageIndex[rewriteIndex]
					       .tag.getMessageInfo().uid) > 0)
			{
				// The folder was opened, this was a new
				// message without a tag.  Subsequently the
				// message was marked deleted.  We can assume
				// that folderMessageIndex is valid at this
				// point in time because the mailbox file is
				// locked.

				skipping=true;
				rewriteIndex++;
				continue;
			}
			else
			{
				folderDirty=true;

				if (rewriting &&
				    rewriteIndex < folderMessageIndex.size())
				{
					// See the previous comment, except
					// for the part where the message was
					// marked as deleted.

					tag=folderMessageIndex[rewriteIndex]
						.tag;

					string uid=tag.getMessageInfo().uid;

					if (updatedFlags.count(uid) > 0)
					{
						mail::messageInfo *i=
							updatedFlags
							.find(uid)->second;

						tag=mail::mboxMagicTag(uid,
								       *i,
								       tag.getKeywords());
					}
				}
				else	// Yes, it's really a new message.

					tag=mail::mboxMagicTag();

				if (saveFile)
				{
					fprintf(*saveFile, "%s\n%s\n%s\n",
						fromLine.c_str(),
						tag.toString().c_str(),
						line.c_str());
				}
			}

			rewriteIndex++;

			mail::messageInfo info=tag.getMessageInfo();


			mboxMessageIndex newIndex;

			newIndex.startingPos=fromPos;
			newIndex.tag=tag;
			if ((newIndex.internalDate=fromCtime(fromhdr)) == 0)
				newIndex.internalDate=time(NULL);

			newMessageIndex.push_back(newIndex);
		}
		else // Message contents
		{
			if (!copying)
			{
				// If we get here, this is the first
				// non-empty line in the file, and it is
				// not a From_ line.

				if (line.size() == 0)
					continue;

				errno=EINVAL;
				return false;
			}

			if (line.size() == 0 && feof(scanFile))
				break;

			if (!skipping && saveFile)
				fprintf(*saveFile, "%s\n", line.c_str());
		}
	}

	// Ok, we've created newMessageIndex, and we have an existing index
	// what now?
	//
	// Blow away the existing index if:
	//    1. Rewriting or closing the folder
	//    2. If we're opening or reopening the folder, an the file did
	//       not have any messages (resetFolder() was never called inside
	//       the loop - see above).
	//
	if ((!copying && !saveFile) || rewriting)
		resetFolder();

	// At this point now we'll append newMessageIndex to whatever's in
	// folderMessageIndex, and update uidmap accordingly.

	vector<mboxMessageIndex>::iterator b, e;

	b=newMessageIndex.begin();
	e=newMessageIndex.end();

	size_t n=folderMessageIndex.size();

	while (b != e)
	{
		mboxMessageIndex &i= *b++;

		mail::messageInfo info=i.tag.getMessageInfo();

		if (uidmap.count(info.uid) > 0) // SHOULD NOT HAPPEN
		{
			info.uid=mail::mboxMagicTag().getMessageInfo().uid;
			i.tag=mail::mboxMagicTag(info.uid, info,
						 i.tag.getKeywords());
			folderDirty=true;
		}

		folderMessageIndex.insert(folderMessageIndex.end(), i);

		try {
			uidmap.insert(make_pair(info.uid, n));
		} catch (...) {
			folderMessageIndex.erase(folderMessageIndex.begin()+n);
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}
		n++;
	}

	// Cleanup

	if (progress)
		progress->reportProgress(stat_buf.st_size, stat_buf.st_size,
					 0, 1);

	if (saveFile && (ferror(*saveFile) || fflush(*saveFile) < 0))
	{
		return false;
	}

	if (ferror(scanFile))
	{
		return false;
	}

	return true;
}

string mail::mbox::translatePath(string path)
{
	return translatePathCommon(path, "./~:", "/");
}
