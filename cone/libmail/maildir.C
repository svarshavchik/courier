/*
** Copyright 2002-2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>

#include "expungelist.H"
#include "misc.H"
#include "maildir.H"
#include "driver.H"
#include "search.H"
#include "copymessage.H"
#include "file.H"

#include "maildir/config.h"
#include "maildir/maildirmisc.h"
#include "maildir/maildirquota.h"
#include "rfc2045/rfc2045.h"

#include <fcntl.h>
#include <unistd.h>

#include <list>
#include <map>

using namespace std;

/////////////////////////////////////////////////////////////////////////////

LIBMAIL_START

static bool open_maildir(mail::account *&accountRet,
			 mail::account::openInfo &oi,
			 mail::callback &callback,
			 mail::callback::disconnect &disconnectCallback)
{
	if (oi.url.substr(0, 8) != "maildir:")
		return false;

	accountRet=new mail::maildir(disconnectCallback,
				     callback, oi.url.substr(8));
	return true;
}

static bool maildir_remote(string url, bool &flag)
{
	if (url.substr(0, 8) != "maildir:")
		return false;

	flag=false;
	return true;
}

driver maildir_driver= { &open_maildir, &maildir_remote };

LIBMAIL_END

/////////////////////////////////////////////////////////////////////////////

mail::maildir::maildirMessageInfo::maildirMessageInfo()
	: lastKnownFilename(""), changed(false)
{
}

mail::maildir::maildirMessageInfo::~maildirMessageInfo()
{
}

#define CONSTRUCTOR \
	calledDisconnected(false), \
	sameServerFolderPtr(NULL), \
	folderCallback(NULL), \
	cacheRfcp(NULL), cachefd(-1), \
	watchFolder(NULL), lockFolder(NULL), watchStarting(false)

mail::maildir::maildir(mail::callback::disconnect &disconnect_callback,
		       mail::callback &callback,
		       string pathArg)
	: mail::account(disconnect_callback), CONSTRUCTOR
{
	if (init(callback, pathArg))
		callback.success("Mail folders opened");
}

mail::maildir::maildir(mail::callback::disconnect &disconnect_callback)
	: mail::account(disconnect_callback), CONSTRUCTOR
{
	// Used by pop3maildrop subclass.
}

bool mail::maildir::init(mail::callback &callback,
			 string pathArg)
{
	ispop3maildrop=false;

	if (pathArg.size() == 0)
		pathArg="Maildir";

	string h=mail::homedir();

	if (h.size() == 0)
	{
		callback.fail("Cannot find my home directory!");
		return false;
	}

	if (pathArg[0] != '/')
		pathArg= h + "/" + pathArg;

	static const char * const subdirs[]={"tmp","cur","new", 0};

	int i;

	for (i=0; subdirs[i]; i++)
	{
		struct stat stat_buf;

		string s= pathArg + "/" + subdirs[i] + "/.";

		if (stat(s.c_str(), &stat_buf) < 0)
		{
			s="Cannot open maildirAccount: ";
			s += strerror(errno);
			callback.fail(s);
			return false;
		}
	}

	path=pathArg;
	return true;
}

mail::maildir::~maildir()
{
	updateNotify(false);

	if (lockFolder)
		maildirwatch_free(lockFolder);
	if (cacheRfcp)
		rfc2045_free(cacheRfcp);
	if (cachefd >= 0)
		close(cachefd);
	if (!calledDisconnected)
	{
		calledDisconnected=true;
		disconnect_callback.disconnected("");
	}

	index.clear();
}

void mail::maildir::resumed()
{
}

void mail::maildir::handler(vector<pollfd> &fds, int &ioTimeout)
{
	int fd;
	int rc;
	int changed, timeout;

	if (!watchFolder)
		return;

	if (watchStarting)
	{
		rc=maildirwatch_started(&watchFolderContents, &fd);

		if (rc < 0)
		{
			updateNotify(false);
			return;
		}
		if (rc > 0)
		{
			watchStarting=false;
			updateFolderIndexInfo(NULL, false);
		}
		if (fd < 0)
			return;

		pollfd pfd;

		pfd.fd=fd;
		pfd.events=pollread;
		pfd.revents=0;
		fds.push_back(pfd);
		return;
	}

	if (maildirwatch_check(&watchFolderContents, &changed, &fd, &timeout)
	    < 0)
	{
		updateNotify(false);
		return;
	}
	if (changed)
	{
		ioTimeout=0;

		MONITOR(mail::maildir);

		updateNotify(false);
		updateFolderIndexInfo(NULL, false);

		if (!DESTROYED())
			updateNotify(true);
		updateFolderIndexInfo(NULL, false);
		return;
	}

	timeout *= 1000;

	if (timeout < ioTimeout)
	{
		ioTimeout=timeout;
	}
	if (fd < 0)
		return;

	pollfd pfd;

	pfd.fd=fd;
	pfd.events=pollread;
	pfd.revents=0;
	fds.push_back(pfd);
}

void mail::maildir::logout(mail::callback &callback)
{
	updateNotify(false);

	if (!calledDisconnected)
	{
		calledDisconnected=true;
		disconnect_callback.disconnected("");
	}
	callback.success("OK");
}

//
// Callback collects filenames moved from new to cur on this maildirAccount scan
//
static void recent_callback_func(const char *c, void *vp)
{
	set<string> *setPtr=(set<string> *)vp;

	try {
		setPtr->insert( string(c));
	} catch (...) {
	}
}

// Update mail::messageInfo with the current message flags
// (read from the message's filename)
// Returns true if flags have been modified

bool mail::maildir::updateFlags(const char *filename,
				mail::messageInfo &info)
{
	bool flag;
	bool changed=false;

#define DOFLAG(name, NOT, theChar) \
	if ((flag= NOT !!maildir_hasflag(filename, theChar)) != info.name) \
		{ info.name=flag; changed=true; }
#define DOFLAG_EMPTY

	DOFLAG(draft, DOFLAG_EMPTY, 'D');
	DOFLAG(replied, DOFLAG_EMPTY, 'R');
	DOFLAG(marked, DOFLAG_EMPTY, 'F');
	DOFLAG(deleted, DOFLAG_EMPTY, 'T');
	DOFLAG(unread, !, 'S');

#undef DOFLAG
#undef DOFLAG_EMPTY

	return changed;
}

//
// Get the filename for message #n
//

string mail::maildir::getfilename(size_t i)
{
	string n("");

	if (i >= index.size())
		return n;

	char *fn;

	char *dir=maildir_name2dir(path.c_str(), folderPath.c_str());

	if (!dir)
		return n;

	try {
		fn=maildir_filename(dir, NULL,
				    index[i].lastKnownFilename.c_str());
		free(dir);
	} catch (...) {
		free(dir);
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	if (fn)
		try {
			n=fn;
			free(fn);
		} catch (...) {
			free(fn);
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}

	return n;
}

void mail::maildir::checkNewMail(callback &callback)
{
	checkNewMail(&callback);
}

class mail::maildir::readKeywordHelper {

	struct maildir_kwReadInfo kri;

	mail::maildir *md;

	static struct libmail_kwMessage
	**findMessageByFilename(const char *filename,
				int autocreate,
				size_t *indexNum,
				void *voidarg);

	static size_t getMessageCount(void *voidarg);
	static struct libmail_kwMessage
	**findMessageByIndex(size_t indexNum,
			     int autocreate,
			     void *voidarg);
	static const char *getMessageFilename(size_t n, void *voidarg);
	static struct libmail_kwHashtable *getKeywordHashtable(void *voidarg);
	static void updateKeywords(size_t n, struct libmail_kwMessage *kw,
				   void *voidarg);

	struct libmail_kwMessage**findMessageByFilename(const char *filename,
							int autocreate,
							size_t *indexNum);

	size_t getMessageCount();
	struct libmail_kwMessage **findMessageByIndex(size_t indexNum,
						      int autocreate);
	const char *getMessageFilename(size_t n);
	struct libmail_kwHashtable *getKeywordHashtable();
	void updateKeywords(size_t n, struct libmail_kwMessage *kw);

	map<string, size_t> filenameMap;

	vector<struct libmail_kwMessage *> kwArray;


public:
	readKeywordHelper(mail::maildir *mdArg);
	~readKeywordHelper();

	bool go(string maildir, bool &rc);
};


mail::maildir::readKeywordHelper::readKeywordHelper(mail::maildir *mdArg)
	: md(mdArg)
{
	size_t n=md->index.size();
	size_t i;

	for (i=0; i<n; i++)
	{
		string f=md->index[i].lastKnownFilename;
		size_t sep=f.rfind(MDIRSEP[0]);

		if (sep != std::string::npos)
			f=f.substr(0, sep);

		filenameMap.insert(make_pair(f, i));
	}

	kwArray.resize(n);
	for (i=0; i<n; i++)
		kwArray[i]=NULL;
}

bool mail::maildir::readKeywordHelper::go(string maildir, bool &rc)
{
	memset(&kri, 0, sizeof(kri));

	kri.findMessageByFilename= &readKeywordHelper::findMessageByFilename;
	kri.getMessageCount= &readKeywordHelper::getMessageCount;
	kri.findMessageByIndex= &readKeywordHelper::findMessageByIndex;

	kri.getMessageFilename= &readKeywordHelper::getMessageFilename;
	kri.getKeywordHashtable= &readKeywordHelper::getKeywordHashtable;
	kri.updateKeywords=&readKeywordHelper::updateKeywords;
	kri.voidarg=this;

	size_t i;

	for (i=0; i<kwArray.size(); i++)
	{
		if (kwArray[i])
			libmail_kwmDestroy(kwArray[i]);
		kwArray[i]=NULL;
	}

	char *imap_lock=NULL;

	if (md->lockFolder)
	{
		int flag;

		imap_lock=maildir_lock(maildir.c_str(), md->lockFolder,
				       &flag);

		if (!imap_lock)
		{
			if (!flag)
			{
				rc=false;
				return false;
			}
		}
	}

	if (maildir_kwRead(maildir.c_str(), &kri) < 0)
	{
		if (imap_lock)
		{
			unlink(imap_lock);
			free(imap_lock);
		}

		rc=false;
		return false;
	}

	if (imap_lock)
	{
		unlink(imap_lock);
		free(imap_lock);
	}

	if (kri.tryagain)
		return true;

	for (i=0; i<kwArray.size(); i++)
	{
		if (kwArray[i] == NULL)
		{
			if (!md->index[i].keywords.empty())
			{
				md->index[i].changed=true;
				md->index[i].keywords=
					mail::keywords::Message();
			}
			continue;
		}

		if (md->index[i].keywords != kwArray[i])
		{
			md->index[i].changed=true;
			md->index[i].keywords.replace(kwArray[i]);
			kwArray[i]=NULL;
		}
	}

	rc=true;
	return false;
}

mail::maildir::readKeywordHelper::~readKeywordHelper()
{
	size_t i;

	for (i=0; i<kwArray.size(); i++)
	{
		if (kwArray[i])
			libmail_kwmDestroy(kwArray[i]);
		kwArray[i]=NULL;
	}
}

struct libmail_kwMessage **
mail::maildir::readKeywordHelper::findMessageByFilename(const char *filename,
							int autocreate,
							size_t *indexNum,
							void *voidarg)
{

	return ( (mail::maildir::readKeywordHelper *)voidarg)
		->findMessageByFilename(filename, autocreate, indexNum);
}

size_t mail::maildir::readKeywordHelper::getMessageCount(void *voidarg)
{
	return ( (mail::maildir::readKeywordHelper *)voidarg)
		->getMessageCount();
}

struct libmail_kwMessage
**mail::maildir::readKeywordHelper::findMessageByIndex(size_t indexNum,
						       int autocreate,
						       void *voidarg)
{
	return ( (mail::maildir::readKeywordHelper *)voidarg)
		->findMessageByIndex(indexNum, autocreate);
}

const char *mail::maildir::readKeywordHelper::getMessageFilename(size_t n,
								 void *voidarg)
{
	return ( (mail::maildir::readKeywordHelper *)voidarg)
		->getMessageFilename(n);
}

struct libmail_kwHashtable *
mail::maildir::readKeywordHelper::getKeywordHashtable(void *voidarg)
{
	return ( (mail::maildir::readKeywordHelper *)voidarg)
		->getKeywordHashtable();
}

void mail::maildir::readKeywordHelper::updateKeywords(size_t n,
						      struct libmail_kwMessage *kw,
						      void *voidarg)
{
	return ( (mail::maildir::readKeywordHelper *)voidarg)
		->updateKeywords(n, kw);
}




struct libmail_kwMessage **
mail::maildir::readKeywordHelper::findMessageByFilename(const char *filename,
							int autocreate,
							size_t *indexNum)
{
	map<string, size_t>::iterator i=filenameMap.find(filename);

	if (i == filenameMap.end())
		return NULL;

	size_t n=i->second;

	if (indexNum)
		*indexNum=n;

	if (kwArray[n] == NULL && autocreate)
		if ((kwArray[n]=libmail_kwmCreate()) == NULL)
			return NULL;
	return &kwArray[n];
}

size_t mail::maildir::readKeywordHelper::getMessageCount()
{
	return kwArray.size();
}

struct libmail_kwMessage
**mail::maildir::readKeywordHelper::findMessageByIndex(size_t n,
						       int autocreate)
{
	if (n >= kwArray.size())
		return NULL;

	if (kwArray[n] == NULL && autocreate)
		if ((kwArray[n]=libmail_kwmCreate()) == NULL)
			return NULL;
	return &kwArray[n];
}

const char *mail::maildir::readKeywordHelper::getMessageFilename(size_t n)
{
	if (n >= kwArray.size())
		return NULL;

	return md->index[n].lastKnownFilename.c_str();
}

struct libmail_kwHashtable *
mail::maildir::readKeywordHelper::getKeywordHashtable()
{
	return &md->keywordHashtable.kwh;
}

void mail::maildir::readKeywordHelper::updateKeywords(size_t n,
						      struct libmail_kwMessage *kw)
{
	if (n >= kwArray.size())
		return;

	if (kwArray[n])
		libmail_kwmDestroy(kwArray[n]);

	kwArray[n]=kw;
}

void mail::maildir::checkNewMail(callback *callback)
{
	if (folderPath.size() == 0)
	{
		if (callback)
			callback->success("OK");
		return;
	}

	set<string> recentMessages;
	vector<maildirMessageInfo> newIndex;

	string md;

	char *d=maildir_name2dir(path.c_str(), folderPath.c_str());

	if (!d)
	{
		callback->fail("Invalid folder");
		return;
	}

	try {
		md=d;
		free(d);
	} catch (...) {
		free(d);
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	// Move messages from new to cur

	maildir_getnew(md.c_str(), NULL,
		       &recent_callback_func, &recentMessages);

	// Now, rescan the cur directory

	scan(folderPath, newIndex);

	// Now, mark messages from from new to cur as recent

	set<string> existingMessages;

	size_t i, n=index.size();

	for (i=0; i<n; i++)
		existingMessages.insert(index[i].uid);

	bool exists=false;

	for (i=0; i<newIndex.size(); i++)
	{
		newIndex[i].recent=
			recentMessages.count(newIndex[i].lastKnownFilename)>0;

		if (existingMessages.count(newIndex[i].uid) == 0)
		{
			exists=true;
			index.push_back(newIndex[i]);
		}
	}

	// Invoke callbacks to reflect deleted, or changed, mail

	MONITOR(mail::maildir);

	mail::expungeList removedList;
	list<size_t> changedList;
	while (n > 0)
	{
		--n;

		string messageFn=getfilename(n);

		if (messageFn.size() == 0)
		{
			index.erase(index.begin() + n);

			removedList << n;
			continue;
		}
		else
		{
			index[n].changed=
				updateFlags(messageFn.c_str(), index[n]);

			index[n].lastKnownFilename=
				strrchr(messageFn.c_str(), '/')+1;
		}
	}

	readKeywordHelper rkh(this);

	bool rc;

	while (rkh.go(md, rc))
		;

	if (!rc)
	{
		if (callback)
			callback->fail(strerror(errno));
		return;
	}

	n=index.size();

	while (n > 0)
	{
		--n;

		if (index[n].changed)
			changedList.insert(changedList.begin(), n);
	}


	removedList >> folderCallback;

	while (!DESTROYED() && !changedList.empty())
	{
		list<size_t>::iterator b=changedList.begin();
		
		size_t n= *b;

		changedList.erase(b);

		if (folderCallback)
			folderCallback->messageChanged(n);
	}

	if (!DESTROYED() && exists && folderCallback)
		folderCallback->newMessages();

	if (callback)
		callback->success("OK");
}




void mail::maildir::genericMarkRead(size_t messageNumber)
{
	if (messageNumber < index.size() && index[messageNumber].unread)
	{
		index[messageNumber].unread=false;
		updateFlags(messageNumber);
	}
}

bool mail::maildir::hasCapability(string capability)
{
	return false;
}

string mail::maildir::getCapability(string name)
{
	upper(name);

	if (name == LIBMAIL_SERVERTYPE)
	{
		return "maildir";
	}

	if (name == LIBMAIL_SERVERDESCR)
	{
		return "Local maildir";
	}

	return "";
}

void mail::maildir::readMessageAttributes(const vector<size_t> &messages,
					  MessageAttributes attributes,
					  mail::callback::message &callback)
{
	genericAttributes(this, this, messages, attributes, callback);
}

void mail::maildir::readMessageContent(const vector<size_t> &messages,
				       bool peek,
				       enum mail::readMode readType,
				       mail::callback::message &callback)
{
	genericReadMessageContent(this, this, messages, peek, readType,
				  callback);
}

void mail::maildir::readMessageContent(size_t messageNum,
				       bool peek,
				       const class mail::mimestruct &msginfo,
				       enum mail::readMode readType,
				       mail::callback::message &callback)
{
	genericReadMessageContent(this, this, messageNum, peek, msginfo,
				  readType, callback);
}

void mail::maildir::readMessageContentDecoded(size_t messageNum,
					      bool peek,
					      const mail::mimestruct &msginfo,
					      mail::callback::message
					      &callback)
{
	genericReadMessageContentDecoded(this, this, messageNum, peek,
					 msginfo, callback);
}

size_t mail::maildir::getFolderIndexSize()
{
	return index.size();
}

mail::messageInfo mail::maildir::getFolderIndexInfo(size_t msgNum)
{
	if (msgNum < index.size())
		return index[msgNum];

	return mail::messageInfo();
}

void mail::maildir::saveFolderIndexInfo(size_t msgNum,
					const mail::messageInfo &info,
					mail::callback &callback)
{
	if (msgNum >= index.size())
	{
		callback.success("OK");
		return;
	}

	mail::messageInfo &newFlags=index[msgNum];

#define DOFLAG(dummy, field, dummy2) \
		newFlags.field=info.field;

	LIBMAIL_MSGFLAGS;

#undef DOFLAG

	string errmsg="Message updated";

	if (!updateFlags(msgNum))
		errmsg="Folder opened in read-only mode.";

	callback.success(errmsg);
}

bool mail::maildir::updateFlags(size_t msgNum)
{
	bool changed=true;

	string messageFn=getfilename(msgNum);

	if (messageFn.size() > 0)
	{
		string s(strrchr(messageFn.c_str(), '/')+1);

		size_t i=s.find(MDIRSEP[0]);

		if (i != std::string::npos)
			s=s.substr(0, i);

		s += MDIRSEP "2,";

		s += getMaildirFlags(index[msgNum]);

		const char *fnP=messageFn.c_str();

		string newName=string(fnP, (const char *)strrchr(fnP, '/')+1)
			+ s;

		if (newName != messageFn)
		{
			if (rename(messageFn.c_str(), newName.c_str()) == 0)
			{
				index[msgNum].lastKnownFilename=s;
			}
			else
				changed=false;
		}

		messageFn=getfilename(msgNum);

		if (folderCallback)
			folderCallback->messageChanged(msgNum);
	}

	return changed;
}

string mail::maildir::getMaildirFlags(const mail::messageInfo &flags)
{
	string s="";

	if (flags.draft)
		s += "D";

	if (flags.replied)
		s += "R";

	if (flags.marked)
		s += "F";

	if (!flags.unread)
		s += "S";

	if (flags.deleted)
		s += "T";
	return s;
}

void mail::maildir::updateFolderIndexFlags(const vector<size_t> &messages,
					   bool doFlip,
					   bool enableDisable,
					   const mail::messageInfo &flags,
					   mail::callback &callback)
{
	string errmsg="Message updated";

	vector<size_t>::const_iterator b, e;

	b=messages.begin();
	e=messages.end();

	size_t n=index.size();

	MONITOR(mail::maildir);

	while (!DESTROYED() && b != e)
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

		if (!updateFlags(i))
			errmsg="Folder opened in read-only mode.";

	}

	callback.success(errmsg);
}

void mail::maildir::removeMessages(const std::vector<size_t> &messages,
				callback &cb)
{
	vector<size_t>::const_iterator b=messages.begin(), e=messages.end();

	while (b != e)
	{
		size_t n=*b++;

		string messageFn=getfilename(n);

		if (messageFn.size() > 0)
			unlink(messageFn.c_str());
	}
	updateFolderIndexInfo(&cb, false);
}

void mail::maildir::updateFolderIndexInfo(mail::callback &callback)
{
	updateFolderIndexInfo(&callback, true);
}

void mail::maildir::updateFolderIndexInfo(mail::callback *callback,
					  bool doExpunge)
{
	if (folderPath.size() == 0)
	{
		if (callback)
			callback->success("OK");
		return;
	}

	struct stat stat_buf;

	size_t n=index.size();

	mail::expungeList removedList;

	while (n > 0)
	{
		--n;

		string messageFn=getfilename(n);

		if (doExpunge)
		{
			if (!index[n].deleted && messageFn.size() > 0)
				continue;

			if (messageFn.size() > 0)
			{
				unlink(messageFn.c_str());
				messageFn="";
			}
		}

		if (messageFn.size() == 0 ||
		    stat(messageFn.c_str(), &stat_buf))
		{
			index.erase(index.begin() + n);

			removedList << n;
		}
	}

	removedList >> folderCallback;

	checkNewMail(callback);
}

void mail::maildir::copyMessagesTo(const vector<size_t> &messages,
				   mail::folder *copyTo,
				   mail::callback &callback)
{
	mail::copyMessages::copy(this, messages, copyTo, callback);
}

void mail::maildir::searchMessages(const mail::searchParams &searchInfo,
				   mail::searchCallback &callback)
{
	mail::searchMessages::search(callback, searchInfo, this);
}

// Open a new folder.

void mail::maildir::open(string pathStr, mail::callback &callback,
			 mail::callback::folder &folderCallbackArg)
{
	index.clear();
	updateNotify(false);

	folderCallback=NULL;
	if (cacheRfcp)
	{
		rfc2045_free(cacheRfcp);
		cacheRfcp=NULL;
	}

	if (cachefd >= 0)
	{
		close(cachefd);
		cachefd=-1;
	}

	if (path == "")
	{
		callback.fail(strerror(errno));
		return;
	}

	set<string> recentMessages;

	string md;

	char *d=maildir_name2dir(path.c_str(), pathStr.c_str());

	if (!d)
	{
		callback.fail(strerror(errno));
		return;
	}

	try {
		md=d;
		free(d);
	} catch (...) {
		free(d);
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	{
		struct stat stat_buf;

		string d=md + "/" KEYWORDDIR;
		/* New Courier-IMAP maildirwatch code */

		mkdir(d.c_str(), 0700);

		if (stat(md.c_str(), &stat_buf) == 0)
			chmod(d.c_str(), stat_buf.st_mode & 0777);
	}

	maildir_purgetmp(md.c_str());
	maildir_getnew(md.c_str(), NULL,
		       &recent_callback_func, &recentMessages);

	scan(pathStr, index);

	folderCallback= &folderCallbackArg;
	folderPath=pathStr;

	vector<maildirMessageInfo>::iterator b=index.begin(), e=index.end();

	while (b != e)
	{
		b->recent=recentMessages.count(b->lastKnownFilename) > 0;
		b++;
	}

	if (lockFolder)
		maildirwatch_free(lockFolder);

	lockFolder=maildirwatch_alloc(md.c_str());

	readKeywordHelper rkh(this);

	bool rc;

	while (rkh.go(md, rc))
		;

	callback.success("Mail folder opened");
}

/*-------------------------------------------------------------------------*/

void mail::maildir::genericMessageRead(string uid,
				       size_t messageNumber,
				       bool peek,
				       mail::readMode readType,
				       mail::callback::message &callback)
{
	if (!fixMessageNumber(this, uid, messageNumber))
	{
		callback.success("OK");
		return;
	}

	MONITOR(mail::maildir);

	string messageFn=getfilename(messageNumber);

	if (messageFn.size() == 0)
	{
		callback.success("OK");
		return;
	}

	int fd=maildir_safeopen(messageFn.c_str(), O_RDONLY, 0);

	if (fd < 0)
	{
		callback.success("OK");
		return;
	}

	try {
		mail::file f(fd, "r");

		f.genericMessageRead(this, messageNumber, readType, callback);

		if (ferror(f))
		{
			callback.fail(strerror(errno));
			close(fd);
			return;
		}

	} catch (...) {
		close(fd);
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	close(fd);

	if (!peek && index[messageNumber].unread)
	{
		index[messageNumber].unread=false;
		saveFolderIndexInfo(messageNumber,
				    index[messageNumber],
				    callback);
		return;
	}

	callback.success("OK");
}

void mail::maildir::genericMessageSize(string uid,
				       size_t messageNumber,
				       mail::callback::message &callback)
{
	if (!fixMessageNumber(this, uid, messageNumber))
	{
		callback.messageSizeCallback(messageNumber, 0);
		callback.success("OK");
		return;
	}

	string messageFn=getfilename(messageNumber);

	if (messageFn.size() == 0)
	{
		callback.messageSizeCallback(messageNumber,0);
		callback.success("OK");
		return;
	}

	unsigned long ul;

	if (maildir_parsequota(messageFn.c_str(), &ul) == 0)
	{
		callback.messageSizeCallback(messageNumber, ul);
		callback.success("OK");
		return;
	}

	struct stat stat_buf;

	int rc=stat(messageFn.c_str(), &stat_buf);

	callback.messageSizeCallback(messageNumber,
				     rc == 0 ? stat_buf.st_size:0);
	callback.success("OK");
}

void mail::maildir::genericGetMessageFd(string uid,
					size_t messageNumber,
					bool peek,
					int &fdRet,
					mail::callback &callback)
{
	struct rfc2045 *dummy;

	genericGetMessageFdStruct(uid, messageNumber, peek, fdRet, dummy,
				  callback);
}

void mail::maildir::genericGetMessageStruct(string uid,
					    size_t messageNumber,
					    struct rfc2045 *&structRet,
					    mail::callback &callback)
{
	int dummy;

	genericGetMessageFdStruct(uid, messageNumber, true, dummy, structRet,
				  callback);
}

bool mail::maildir::genericCachedUid(string uid)
{
	return uid == cacheUID && cachefd >= 0 && cacheRfcp != 0;
}

void mail::maildir::genericGetMessageFdStruct(string uid,
					      size_t messageNumber,
					      bool peek,
					      int &fdRet,
					      struct rfc2045 *&structret,
					      mail::callback &callback)
{
	if (uid == cacheUID && cachefd >= 0 && cacheRfcp != 0)
	{
		fdRet=cachefd;
		structret=cacheRfcp;
		callback.success("OK");
		return;
	}

	if (!fixMessageNumber(this, uid, messageNumber))
	{
		callback.fail("Message removed on the server");
		return;
	}

	if (cacheRfcp)
	{
		rfc2045_free(cacheRfcp);
		cacheRfcp=NULL;
	}

	if (cachefd >= 0)
	{
		close(cachefd);
		cachefd= -1;
	}

	if (!fixMessageNumber(this, uid, messageNumber))
	{
		callback.fail("Message removed on the server");
		return;
	}

	string messageFn=getfilename(messageNumber);

	if (messageFn.size() == 0)
	{
		callback.fail("Message removed on the server");
		return;
	}

	int fd=maildir_safeopen(messageFn.c_str(), O_RDONLY, 0);

	struct rfc2045 *rfcp;

	if (fd < 0 || fcntl(fd, F_SETFD, FD_CLOEXEC) < 0 ||
	    (rfcp=rfc2045_alloc()) == NULL)
	{
		if (fd >= 0)
			close(fd);
		callback.fail("Message removed on the server");
		return;
	}

	fcntl(fd, F_SETFD, FD_CLOEXEC);

	cachefd=fd;
	cacheRfcp=rfcp;
	cacheUID="";

	vector<char> buffer;

	buffer.insert(buffer.end(), BUFSIZ, 0);

	int n;

	while ((n=read(fd, &buffer[0], buffer.size())) > 0)
		rfc2045_parse(rfcp, &buffer[0], n);
	rfc2045_parse_partial(rfcp);

	if (n < 0)
	{
		callback.fail(strerror(errno));
		return;
	}

	fdRet=cachefd;
	structret=cacheRfcp;
	if (!peek && index[messageNumber].unread)
	{
		index[messageNumber].unread=false;
		saveFolderIndexInfo(messageNumber, index[messageNumber],
				    callback);
		return;
	}
	callback.success("OK");
}

void mail::maildir::updateNotify(bool enableDisable,
				 mail::callback &callback)
{
	updateNotify(enableDisable);
	if (!enableDisable)
		updateFolderIndexInfo(&callback, false);
	else
		callback.success("OK");
}

void mail::maildir::updateNotify(bool enableDisable)
{
	if (!enableDisable)
	{
		if (watchFolder)
		{
			maildirwatch_end(&watchFolderContents);
			maildirwatch_free(watchFolder);
			watchFolder=NULL;
		}
		return;
	}

	if (folderPath.size() == 0 || watchFolder)
		return;

	char *dir=maildir_name2dir(path.c_str(), folderPath.c_str());

	if (!dir)
		return;

	watchFolder=maildirwatch_alloc(dir);

	free(dir);

	if (!watchFolder)
		return;

	if (maildirwatch_start(watchFolder, &watchFolderContents) < 0)
	{
		maildirwatch_free(watchFolder);
		return;
	}
	watchStarting=true;
}


void mail::maildir::getFolderKeywordInfo(size_t n, std::set<std::string> &s)
{
	string messageFn=getfilename(n);

	if (messageFn.size() == 0)
		return;

	index[n].keywords.getFlags(s);
}

void mail::maildir::updateKeywords(const vector<size_t> &messages,
				   const set<string> &keywords,
				   bool setOrChange,
				   bool changeTo,
				   mail::callback &cb)
{
	bool keepGoing;

	if (folderPath.size() == 0)
	{
		cb.success("Ok.");
		return;
	}

	string dir;

	{
		char *dirs=maildir_name2dir(path.c_str(), folderPath.c_str());
		if (!dirs)
		{
			cb.fail(strerror(errno));
			return;
		}

		try {
			dir=dirs;
			free(dirs);
		} catch (...) {
			free(dirs);
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}
	}

	if (!setOrChange)
	{
		if (!updateKeywords(dir,
				    messages, keywords, setOrChange, changeTo,
				    NULL, NULL))
		{
			cb.fail(strerror(errno));
			return;
		}
	}
	else do {
		struct libmail_kwGeneric g;

		libmail_kwgInit(&g);

		char *imap_lock=NULL;

		if (lockFolder)
		{
			int flag;

			imap_lock=maildir_lock(dir.c_str(), lockFolder,
					       &flag);

			if (!imap_lock)
			{
				if (!flag)
				{
					libmail_kwgDestroy(&g);
					cb.fail(strerror(errno));
					return;
				}
			}
		}

		try {
			if (libmail_kwgReadMaildir(&g, dir.c_str()) < 0)
			{
				if (imap_lock)
				{
					unlink(imap_lock);
					free(imap_lock);
					imap_lock=NULL;
				}

				cb.fail(strerror(errno));
				return;
			}

			keepGoing=false;

			if (!updateKeywords(dir,
					    messages, keywords, setOrChange,
					    changeTo,
					    &g, &keepGoing))
			{
				if (imap_lock)
				{
					unlink(imap_lock);
					free(imap_lock);
					imap_lock=NULL;
				}
				libmail_kwgDestroy(&g);
				if (keepGoing)
					continue;

				cb.fail(strerror(errno));
				return;
			}
			if (imap_lock)
			{
				unlink(imap_lock);
				free(imap_lock);
				imap_lock=NULL;
			}
			libmail_kwgDestroy(&g);
		} catch (...) {
			if (imap_lock)
			{
				unlink(imap_lock);
				free(imap_lock);
				imap_lock=NULL;
			}
			libmail_kwgDestroy(&g);
			throw;
		}
	} while (keepGoing);

	cb.success("Message keywords updated.");
}


bool mail::maildir::updateKeywords(string dir,
				   const vector<size_t> &messages,
				   const set<string> &keywords,
				   bool setOrChange,
				   bool changeTo,
				   struct libmail_kwGeneric *g,
				   bool *keepGoing)
{
	mail::keywords::Message m;

	m.setFlags(keywordHashtable, keywords);

	MONITOR(mail::maildir);

	vector<size_t>::const_iterator b=messages.begin(),
		e=messages.end();

	while (!DESTROYED() && b != e)
	{
		size_t n= *b++;

		string messageFn=getfilename(n);

		if (messageFn.size() == 0)
			continue;

		if (!setOrChange)
		{
			set<string> newFlags;

			m.getFlags(newFlags);
			index[n].keywords.setFlags(keywordHashtable,
						   newFlags);
			// Can't copy, because of reference counting.

			char *tmpname, *newname;

			if (maildir_kwSave(dir.c_str(),
					   index[n].lastKnownFilename
					   .c_str(), newFlags,
					   &tmpname, &newname, 0) < 0)
				return false;

			int rc=rename(tmpname, newname);
			free(tmpname);
			free(newname);

			if (rc < 0)
				return false;
		}
		else if (changeTo)
		{
			struct libmail_kwGenericEntry *origKw=
				libmail_kwgFindByName(g, index[n]
						      .lastKnownFilename
						      .c_str());

			set<string>::iterator b=keywords.begin(),
				e=keywords.end();

			while (b != e)
			{
				if (!index[n].keywords
				    .addFlag(keywordHashtable, *b))
				{
					LIBMAIL_THROW(strerror(errno));
				}

				if (origKw && origKw->keywords &&
				    libmail_kwmSetName(&g->kwHashTable,
						       origKw->keywords,
						       b->c_str()))
					LIBMAIL_THROW(strerror(errno));

				++b;
			}

			char *tmpname, *newname;

			set<string> newFlags;

			index[n].keywords.getFlags(newFlags);

			if ((origKw && origKw->keywords ?
			     maildir_kwSave(dir.c_str(),
					    index[n].lastKnownFilename
					    .c_str(), origKw->keywords,
					    &tmpname, &newname,
					    1):
			     maildir_kwSave(dir.c_str(),
					    index[n].lastKnownFilename
					    .c_str(), newFlags,
					    &tmpname, &newname,
					    1)) < 0)
				return false;

			int rc=link(tmpname, newname);
			unlink(tmpname);
			free(tmpname);
			free(newname);

			if (rc < 0)
			{
				if (errno == EEXIST)
					*keepGoing=true;
				return false;
			}
		}
		else
		{
			struct libmail_kwGenericEntry *origKw=
				libmail_kwgFindByName(g, index[n]
						      .lastKnownFilename
						      .c_str());

			set<string>::iterator b=keywords.begin(),
				e=keywords.end();

			while (b != e)
			{
				struct libmail_keywordEntry *kef;

				if (!index[n].keywords.remFlag(*b))
					LIBMAIL_THROW(strerror(errno));

				if (origKw && origKw->keywords &&
				    (kef=libmail_kweFind(&g->kwHashTable,
							 b->c_str(),
							 0)) != NULL)
				{
					libmail_kwmClear(origKw->keywords,
							 kef);
				}
				++b;
			}
			char *tmpname, *newname;
			set<string> newFlags;

			index[n].keywords.getFlags(newFlags);

			if ((origKw && origKw->keywords
			     ? maildir_kwSave(dir.c_str(),
					      index[n].lastKnownFilename
					      .c_str(), origKw->keywords,
					      &tmpname, &newname,
					   1)
			     : maildir_kwSave(dir.c_str(),
					      index[n].lastKnownFilename
					      .c_str(), newFlags,
					      &tmpname, &newname,
					      1)) < 0)
				return false;

			int rc=link(tmpname, newname);
			unlink(tmpname);
			free(tmpname);
			free(newname);

			if (rc < 0)
			{
				if (errno == EEXIST)
					*keepGoing=true;
				return false;
			}
		}

		if (folderCallback)
			folderCallback->messageChanged(n);
	}
	return true;
}
