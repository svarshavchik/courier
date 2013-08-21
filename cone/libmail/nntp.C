/*
** Copyright 2003-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "mail.H"
#include "misc.H"
#include "driver.H"
#include "nntp.H"
#include "nntpnewsrc.H"
#include "nntpxpat.H"
#include "copymessage.H"
#include "search.H"
#include "smtpinfo.H"
#include "objectmonitor.H"
#include "libcouriertls.h"
#include "expungelist.H"
#include <unicode/unicode.h>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <vector>
#include <signal.h>
#include <set>
#include "nntplogin2.H"
#include "nntpfolder.H"
#include "nntplogout.H"
#include "nntpfetch.H"
#include "nntpcache.H"
#include "nntpxover.H"
#include "nntpchecknew.H"
#include "rfc2045/rfc2045.h"

#define TIMEOUTINACTIVITY 15

using namespace std;

mail::nntp::nntpMessageInfo::nntpMessageInfo()
	: msgNum(0), msgFlag(0)
{
}

mail::nntp::nntpMessageInfo::~nntpMessageInfo()
{
}

/////////////////////////////////////////////////////////////////////////////

LIBMAIL_START

static bool open_nntp(mail::account *&accountRet,
		      mail::account::openInfo &oi,
		      mail::callback &callback,
		      mail::callback::disconnect &disconnectCallback)
{
	mail::loginInfo nntpLoginInfo;

	if (!mail::loginUrlDecode(oi.url, nntpLoginInfo))
		return false;

	if (nntpLoginInfo.method != "nntp" &&
	    nntpLoginInfo.method != "nntps")
		return false;

	accountRet=new mail::nntp(oi.url, oi.pwd, oi.certificates,
				  oi.extraString,
				  oi.loginCallbackObj,
				  callback,
				  disconnectCallback);
	return true;
}

static bool nntp_remote(string url, bool &flag)
{
	mail::loginInfo nntpLoginInfo;

	if (!mail::loginUrlDecode(url, nntpLoginInfo))
		return false;

	if (nntpLoginInfo.method != "nntp" &&
	    nntpLoginInfo.method != "nntps")
		return false;

	flag=true;
	return true;
}

driver nntp_driver= { &open_nntp, &nntp_remote };

LIBMAIL_END

/////////////////////////////////////////////////////////////////////////////
//
// Service this account.
//

void mail::nntp::handler(vector<pollfd> &fds, int &ioTimeout)
{
	int fd_save=getfd();

	// Disconnect from the server, after a period of inactivity.

	if (inactivityTimeout)
	{
		time_t now=time(NULL);

		if (now + autologoutSetting < inactivityTimeout)
			// Time reset??
		{
			inactivityTimeout = now + autologoutSetting;
		}

		if (now >= inactivityTimeout)
		{
			installTask(new mail::nntp::LogoutTask(NULL,
							       *this,
							       true));
		}
		else
		{
			now = inactivityTimeout - now;

			if (now * 1000 <= ioTimeout)
			{
				ioTimeout = now * 1000;
			}
		}
	}

	if (!tasks.empty())
	{
		int t=(*tasks.begin())->getTimeout();

		if (t * 1000 <= ioTimeout)
		{
			ioTimeout=t;
		}
	}

	MONITOR(mail::nntp);

	process(fds, ioTimeout);

	if (DESTROYED())
		ioTimeout=0;

	if (DESTROYED() || getfd() < 0)
	{
		size_t i;

		for (i=fds.size(); i; )
		{
			--i;

			if (fds[i].fd == fd_save)
			{
				fds.erase(fds.begin()+i, fds.begin()+i+1);
				break;
			}
		}
	}

	return;
}

void mail::nntp::resumed()
{
	if (!tasks.empty())
		(*tasks.begin())->resetTimeout();
}

//
// There's something to read.  NNTP tasks read one line at a time.

int mail::nntp::socketRead(const std::string &readbuffer)
{
	size_t n;

	if (!tasks.empty() && (n=readbuffer.find('\n')) != std::string::npos)
	{
		size_t i=n;

		if (i > 0 && readbuffer[i-1] == '\r')
			--i;

		string line=readbuffer.substr(0, i);

		Task *t= *tasks.begin();

		t->resetTimeout(); // Command is alive
		t->serverResponse(line.c_str());
		return n+1;
	}

	return 0;
}

//
// Let the first task handle disconnection events.
//

void mail::nntp::disconnect(const char *reason)
{
	string errmsg=reason ? reason:"";

	if (getfd() >= 0)
	{
		string errmsg2=socketDisconnect();

		if (errmsg2.size() > 0)
			errmsg=errmsg2;
	}

	if (!tasks.empty())
		(*tasks.begin())->disconnected(errmsg.c_str());
}

mail::nntp::nntp(string url, string passwd,
		 std::vector<std::string> &certificates,
		 string newsrcFilenameArg,
		 mail::loginCallback *loginCallbackFunc,
		 callback &callback,
		 callback::disconnect &disconnectCallbackArg)
	: mail::fd(disconnectCallbackArg, certificates),
	  inactivityTimeout(0),
	  folderCallback(NULL),
	  hasNewgroups(false), didCacheNewsrc(false),
	  disconnectCallback(&disconnectCallbackArg),
	  genericTmpFp(NULL), genericTmpRfcp(NULL)
{
	if (newsrcFilenameArg.size() == 0)
	{
		callback.fail("Invalid NNTP login (.newsrc unspecified)");
		return;
	}

	newsrcFilename=newsrcFilenameArg;

	if (!mail::loginUrlDecode(url, savedLoginInfo))
	{
		callback.fail("Invalid NNTP URL.");
		return;
	}
	timeoutSetting=getTimeoutSetting(savedLoginInfo, "timeout", 60,
					 30, 600);
	autologoutSetting=getTimeoutSetting(savedLoginInfo, "autologout", 300,
					    5, 1800);

	savedLoginInfo.loginCallbackFunc=loginCallbackFunc;
	savedLoginInfo.use_ssl=savedLoginInfo.method == "nntps";
	if (passwd.size() > 0)
		savedLoginInfo.pwd=passwd;

	try {
		installTask(new LoginTask(&callback, *this));
	} catch (...) {
		callback.fail(strerror(errno));
	}
}

void mail::nntp::cleartmp()
{
	if (genericTmpFp != NULL)
	{
		fclose(genericTmpFp);
		genericTmpFp=NULL;
	}

	if (genericTmpRfcp != NULL)
	{
		rfc2045_free(genericTmpRfcp);
		genericTmpRfcp=NULL;
	}
}

mail::nntp::~nntp()
{
	cleartmp();
        while (!tasks.empty())
		(*tasks.begin())->disconnected("NNTP connection aborted.");

	if (disconnectCallback)
	{
		callback::disconnect *d=disconnectCallback;

		disconnectCallback=NULL;

		d->disconnected("Connection aborted.");
	}
	index.clear();
}

// Cache newsrc into a map

void mail::nntp::cacheNewsrc()
{
	if (didCacheNewsrc)
		return;

	cachedNewsrc.clear();
	ifstream i(newsrcFilename.c_str());

	string line;

	while (i.is_open() && !getline(i, line).fail())
	{
		newsrc parseLine(line);

		if (parseLine.newsgroupname.size() == 0)
			continue;

		cachedNewsrc.insert(make_pair(parseLine.newsgroupname,
					      parseLine));
	}
	didCacheNewsrc=true;
}

// Save an updated newsrc line

bool mail::nntp::updateOpenedNewsrc(newsrc &n)
{
	updateCachedNewsrc();
	discardCachedNewsrc();

	bool updated=false;

	string newNewsrcFilename=newsrcFilename + ".tmp";

	ofstream o(newNewsrcFilename.c_str());

	if (o.is_open())
	{
		ifstream i(newsrcFilename.c_str());

		string line;

		while (i.is_open() && !getline(i, line).fail())
		{
			newsrc parseLine(line);

			if (parseLine.newsgroupname == n.newsgroupname)
			{
				o << (string)n << endl;
				updated=true;
			}
			else
				o << line << endl;
		}

		if (!updated)
			o << (string)n << endl;

		o << flush;
		o.close();

		if (!o.fail() && !o.bad() &&
		    rename(newNewsrcFilename.c_str(),
			   newsrcFilename.c_str()) == 0)
			return true;
	}
	return false;
}

// For speed, we sort the cachedNewsrc by ptrs

class mail::nntp::cachedNewsrcSort {
public:
	bool operator()(newsrc *a, newsrc *b)
	{
		return a->newsgroupname < b->newsgroupname;
	}
};

// Save updated newsrc cache

bool mail::nntp::updateCachedNewsrc()
{
	if (!didCacheNewsrc)
		return true;

	vector<newsrc *> vec;

	vec.reserve(cachedNewsrc.size());

	map<string, newsrc>::iterator b=cachedNewsrc.begin(),
		e=cachedNewsrc.end();

	while (b != e)
	{
		vec.push_back(&b->second);
		b++;
	}

	sort(vec.begin(), vec.end(), cachedNewsrcSort());

	string newNewsrcFilename=newsrcFilename + ".tmp";

	ofstream o(newNewsrcFilename.c_str());

	if (o.is_open())
	{
		// Copy any extra lines...

		ifstream i(newsrcFilename.c_str());

		string line;

		while (i.is_open() && !getline(i, line).fail())
		{
			if (line[0] == '#')
			{
				o << line << endl;
				continue;
			}
		}
		i.close();

		vector<newsrc *>::iterator b=vec.begin(), e=vec.end();

		while (b != e)
		{
			o << (string)**b << endl;
			b++;
		}
		o << flush;
		o.close();

		if (!o.fail() && !o.bad() &&
		    rename(newNewsrcFilename.c_str(),
			   newsrcFilename.c_str()) == 0)
			return true;
	}
	return false;
}

// Discard cached newsrc

void mail::nntp::discardCachedNewsrc()
{
	cachedNewsrc.clear();
	didCacheNewsrc=false;
}

// Create a newsrc line based on the currently open folder index

void mail::nntp::createNewsrc(newsrc &n)
{
	n.msglist.clear();

	vector<nntpMessageInfo>::iterator
		b=index.begin(), e=index.end();

	if (b != e)
	{
		bool first=true;

		msgnum_t lastNum=0;

		while (b != e)
		{
			if (first)
			{
				if (b->msgNum > 1)
					n.msglist.push_back(make_pair(1,
								      b->msgNum-1)
							    );
			}
			else if (lastNum+1 != b->msgNum)
				n.msglist.push_back(make_pair(lastNum+1,
							      b->msgNum-1));
			first=false;
			lastNum= b->msgNum;
			b++;
		}
	}
	else if (hiWatermark > 1)
	{
		n.msglist.push_back(make_pair(1, hiWatermark-1));
	}
}

void mail::nntp::installTask(Task *taskp)
{
	if (taskp == NULL)
		LIBMAIL_THROW((strerror(errno)));

	bool wasEmpty=tasks.empty();

	tasks.insert(tasks.end(), taskp);
	inactivityTimeout=0; // We're active now

	if (wasEmpty)
	{
		Task *p= *tasks.begin();

		p->resetTimeout();
		p->installedTask();
	}
}

void mail::nntp::logout(callback &callback)
{
	installTask(new mail::nntp::LogoutTask(&callback, *this, false));
}

void mail::nntp::checkNewMail(callback &callback)
{
	if (openedGroup.size() > 0)
		installTask(new mail::nntp::CheckNewTask(&callback, *this,
							 openedGroup));
	else
		callback.success("Ok");
}

bool mail::nntp::hasCapability(string capability)
{
	if (capability == LIBMAIL_CHEAPSTATUS)
		return true;

	return false;
}

string mail::nntp::getCapability(string capability)
{
	if (capability == LIBMAIL_SERVERTYPE)
		return "nntp";

	if (capability == LIBMAIL_SERVERDESCR)
		return "Usenet server";

	return "";
}

mail::folder *mail::nntp::folderFromString(string s)
{
	string words[3];

	words[0]="";
	words[1]="";
	words[2]="";

	int i=0;

	string::iterator b=s.begin(), e=s.end();

	while (b != e)
	{
		if (*b == ':')
		{
			b++;
			++i;
			continue;
		}

		if (*b == '\\')
		{
			b++;
			if (b == e)
				break;
		}

		if (i < 3)
			words[i].append(&*b, 1);
		b++;
	}

	return new mail::nntp::folder(this, words[2],
				      strchr(words[0].c_str(), 'F') != NULL,
				      strchr(words[0].c_str(), 'D') != NULL);
}

void mail::nntp::readTopLevelFolders(callback::folderList &callback1,
				     callback &callback2)
{
	mail::nntp::folder subscribed(this, FOLDER_SUBSCRIBED,
				      false, true);
	mail::nntp::folder all(this, FOLDER_NEWSRC,
			       false, true);
	mail::nntp::folder newnews(this, FOLDER_CHECKNEW,
				   false, true);
	mail::nntp::folder refresh(this, FOLDER_REFRESH,
				   false, true);

	vector<const mail::folder *> v;

	v.push_back(&subscribed);
	v.push_back(&all);
	v.push_back(&newnews);
	v.push_back(&refresh);

	callback1.success(v);
	callback2.success("OK");
}

void mail::nntp::findFolder(string folder,
			    callback::folderList &callback1,
			    callback &callback2)
{
	if (folder.size() == 0 ||
	    (folder[0] == '/' && folder.find('.') == std::string::npos))
	{
		mail::nntp::folder dummy(this, folder, false, true);

		vector<const mail::folder *> vec;

		vec.push_back(&dummy);

		callback1.success(vec);
		callback2.success("OK");
		return;
	}

	if (folder.size() > sizeof(FOLDER_SUBSCRIBED) &&
	    folder.substr(0, sizeof(FOLDER_SUBSCRIBED)) ==
	    FOLDER_SUBSCRIBED ".")
	{
		mail::nntp::folder dummy(this, folder, true, false);

		vector<const mail::folder *> vec;

		vec.push_back(&dummy);

		callback1.success(vec);
		callback2.success("OK");
		return;
	}

	bool foundFolder=false;
	bool foundSubFolders=false;

	{
		ifstream i(newsrcFilename.c_str());

		if (i.is_open())
		{
			string line;

			while (i.is_open() && !getline(i, line).fail())
			{
				newsrc parseLine(line);

				if (parseLine.newsgroupname.size() == 0)
					continue;

				string s=FOLDER_NEWSRC "." + parseLine.newsgroupname;

				if (s == folder)
					foundFolder=true;

				if (s.size() > folder.size() &&
				    s.substr(0, folder.size()) == folder &&
				    s[folder.size()] == '.')
					foundSubFolders = true;
			}
		}
	}

	if (!foundFolder)
		foundSubFolders=true;

	mail::nntp::folder dummy(this, folder, foundFolder,
				 foundSubFolders);

	vector<const mail::folder *> vec;

	vec.push_back(&dummy);

	callback1.success(vec);
	callback2.success("OK");
}

string mail::nntp::translatePath(string path)
{
	// Evil trick: "" gets as the fake top-level hierarchy,
	// readTopLevelFolders.  Otherwise, unless the path already starts with
	// a ".", prepend the SUBSCRIBED trunc.

	if (path.size() > 0 && path[0] != '/')
		return FOLDER_SUBSCRIBED "." + path;

	return path;
}

mail::folder *mail::nntp::getSendFolder(const smtpInfo &info,
					const mail::folder *folder,
					std::string &errmsg)
{
	if (info.recipients.size() == 0 &&
	    folder == NULL &&
	    info.options.find("POST") != info.options.end())
	{
		// Sanity check


		mail::folder *f=new mail::nntp::folder(this, "", false,
						       true);
		// Any of our folders will do the trick.

		if (!f)
			errmsg=strerror(errno);
		return f;
	}

	return mail::account::getSendFolder(info, folder, errmsg);
}

void mail::nntp::readMessageAttributes(const vector<size_t> &messages,
				       MessageAttributes attributes,
				       callback::message &callback)
{
	bool useXover;

	if (attributes & MESSAGESIZE)
		useXover=true;
	else if (attributes & MIMESTRUCTURE)
		useXover=false;
	else if (attributes & (ENVELOPE | ARRIVALDATE))
		useXover=true;
	else
		useXover=false;

	if (useXover)
	{
		installTask(new XoverTask(&callback, *this,
					  openedGroup, messages, attributes));
		return;
	}

	genericAttributes(this, this, messages, attributes, callback);
}

bool mail::nntp::fixGenericMessageNumber(std::string uid, size_t &msgNum)
{
	msgnum_t n;

	istringstream i(uid);

	i >> n;

	if (i.bad() || i.fail())
	{
		return false;
	}

	// Need to fix up the message #

	size_t cnt=getFolderIndexSize();

	if (cnt <= msgNum)
	{
		msgNum=cnt;
		if (msgNum == 0)
		{
			return false;
		}
		--msgNum;
	}
			
	while (n > index[msgNum].msgNum)
		if (++msgNum >= cnt)
		{
			return false;
		}

	while (msgNum > 0 && n < index[msgNum].msgNum)
		--msgNum;

	if (n != index[msgNum].msgNum)
	{
		return false;
	}

	return true;
}

void mail::nntp::readMessageContent(const vector<size_t> &messages,
				    bool peek,
				    enum mail::readMode readType,
				    callback::message &callback)
{
	genericReadMessageContent(this, this, messages, peek,
				  readType, callback);
}

void mail::nntp::readMessageContent(size_t messageNum,
				    bool peek,
				    const mimestruct &msginfo,
				    enum mail::readMode readType,
				    callback::message &callback)
{
	genericReadMessageContent(this, this, messageNum, peek, msginfo,
				  readType, callback);
}

void mail::nntp::readMessageContentDecoded(size_t messageNum,
					   bool peek,
					   const mimestruct &msginfo,
					   callback::message &callback)
{
	genericReadMessageContentDecoded(this, this, messageNum, peek,
					 msginfo, callback);
}

size_t mail::nntp::getFolderIndexSize()
{
	return openedGroup.size() == 0 ? 0:index.size();
}

mail::messageInfo mail::nntp::getFolderIndexInfo(size_t msgNum)
{
	if (msgNum < getFolderIndexSize())
	{
		messageInfo dummy;

		unsigned char n=index[msgNum].msgFlag;

		dummy.marked= !!(n & IDX_FLAGGED);
		dummy.deleted=!!(n & IDX_DELETED);

		ostringstream o;

		o << index[msgNum].msgNum;

		dummy.uid=o.str();
		return dummy;
	}

	return messageInfo();
}

void mail::nntp::saveFolderIndexInfo(size_t msgNum,
				     const messageInfo &msgInfo,
				     callback &callback)
{
	if (msgNum < getFolderIndexSize())
	{
		unsigned char n=0;

		if (msgInfo.marked)
			n |= IDX_FLAGGED;

		if (msgInfo.deleted)
			n |= IDX_DELETED;

		index[msgNum].msgFlag=n;
		if (openedGroup.size() > 0)
			folderCallback->messageChanged(msgNum);
	}
	callback.success("Message status updated.");
}

void mail::nntp::genericMarkRead(size_t messageNumber)
{
}

void mail::nntp::updateFolderIndexFlags(const vector<size_t> &messages,
					bool doFlip,
					bool enableDisable,
					const messageInfo &flags,
					callback &callback)
{
	vector<size_t>::const_iterator b, e;

	b=messages.begin();
	e=messages.end();

	size_t n=getFolderIndexSize();

	while (b != e)
	{
		size_t i= *b++;

		if (i < n)
		{
			messageInfo dummy;

			unsigned char n=index[i].msgFlag;

			dummy.marked= !!(n & IDX_FLAGGED);
			dummy.deleted=!!(n & IDX_DELETED);

#define DOFLAG(d, field, dummy2) \
			if (flags.field) \
			{ \
				dummy.field=\
					doFlip ? !dummy.field\
					       : enableDisable; \
			}

			LIBMAIL_MSGFLAGS;
#undef DOFLAG
			n=0;

			if (dummy.marked)
				n |= IDX_FLAGGED;

			if (dummy.deleted)
				n |= IDX_DELETED;
			index[i].msgFlag=n;
		}
	}

	b=messages.begin();
	e=messages.end();

	MONITOR(mail::nntp);

	while (!DESTROYED() && b != e)
	{
		size_t i= *b++;

		if (i < n && folderCallback)
			folderCallback->messageChanged(i);
	}

	saveSnapshot();
	callback.success("OK");
}

void mail::nntp::getFolderKeywordInfo(size_t msgNum,
				      set<string> &keywords)
{
	keywords.clear();
	if (msgNum >= index.size())
		return;
	index[msgNum].keywords.getFlags(keywords);
}

bool mail::nntp::genericProcessKeyword(size_t msgNum,
				       updateKeywordHelper &helper)
{
	if (msgNum >= index.size())
		return true;
	helper.doUpdateKeyword(index[msgNum].keywords, keywordHashtable);
	return true;
}


void mail::nntp::updateKeywords(const std::vector<size_t> &messages,
				const std::set<std::string> &keywords,
				bool setOrChange,
				// false: set, true: see changeTo
				bool changeTo,
				callback &cb)
{
	MONITOR(mail::nntp);

	genericUpdateKeywords(messages, keywords, setOrChange, changeTo,
			      folderCallback, this, cb);

	if (!DESTROYED())
		saveSnapshot();
}


void mail::nntp::updateFolderIndexInfo(callback &callback)
{
	vector<size_t> deletedMsgs;

	size_t n=getFolderIndexSize();
	size_t i;

	for (i=0; i<n; i++)
		if (index[i].msgFlag & IDX_DELETED)
			deletedMsgs.push_back(i);

	removeMessages(deletedMsgs, callback);
}

void mail::nntp::removeMessages(const vector<size_t> &messages,
				callback &callback)
{
	if (openedGroup.size() == 0)
	{
		callback.success("Ok.");
		return;
	}

	set<size_t> msgNumSet;

	msgNumSet.insert(messages.begin(), messages.end());

	list< pair<size_t, size_t> > removedList;
	size_t n=getFolderIndexSize();

	MONITOR(mail::nntp);

	while (!DESTROYED() && n)
	{
		--n;
		if (msgNumSet.find(n) == msgNumSet.end())
			continue;

		index.erase(index.begin() + n);

		if (!removedList.empty() &&
		    removedList.begin()->first == n+1)
		{
			removedList.begin()->first--;
			continue;
		}

		if (removedList.size() >= 100)
		{
			vector< pair<size_t, size_t> > vec;

			vec.insert(vec.end(), removedList.begin(),
				   removedList.end());
			removedList.clear();

			if (folderCallback)
				folderCallback->messagesRemoved(vec);
		}

		removedList.push_front(make_pair(n, n));
	}

	if (!DESTROYED() && removedList.size() > 0)
	{
		vector< pair<size_t, size_t> > vec;

		vec.insert(vec.end(), removedList.begin(),
			   removedList.end());
		removedList.clear();

		if (folderCallback)
			folderCallback->messagesRemoved(vec);
	}

	if (!DESTROYED())
	{
		updateCachedNewsrc();
		discardCachedNewsrc();

		newsrc newNewsrc;

		createNewsrc(newNewsrc);
		newNewsrc.newsgroupname=openedGroup;
		newNewsrc.subscribed=true;
		updateOpenedNewsrc(newNewsrc);

		saveSnapshot();
	}

	callback.success("Group updated.");
}

void mail::nntp::saveSnapshot()
{
	if (folderCallback)
	{
		ostringstream o;

		o << loWatermark << '-' << hiWatermark;

		folderCallback->saveSnapshot(o.str());
	}
}

void mail::nntp::copyMessagesTo(const vector<size_t> &messages,
				mail::folder *copyTo,
				callback &callback)
{
	copyMessages::copy(this, messages, copyTo, callback);
}

void mail::nntp::searchMessages(const searchParams &searchInfo,
				searchCallback &callback)
{
	string hdr;
	switch (searchInfo.criteria) {
	case searchParams::from:
		hdr="FROM";
		break;
	case searchParams::to:
		hdr="TO";
		break;
	case searchParams::cc:
		hdr="CC";
		break;
	case searchParams::bcc:
		hdr="BCC";
		break;
	case searchParams::subject:
		hdr="SUBJECT";
		break;
	case searchParams::header:
		hdr=searchInfo.param1;
		break;
	default:
		break;
	}

	if (hdr.size() > 0)
	{
		unicode_char *uc;
		size_t ucsize;
		libmail_u_convert_handle_t
			h(libmail_u_convert_tou_init(searchInfo.charset.c_str(),
						     &uc, &ucsize, 0));

		if (h)
		{
			libmail_u_convert(h, searchInfo.param2.c_str(),
					  searchInfo.param2.size());

			if (libmail_u_convert_deinit(h, NULL))
				uc=NULL;
		}
		else
		{
			uc=NULL;
		}

		if (uc)
		{
			size_t i;

			for (i=0; i<ucsize; i++)
				if (uc[i] < 32 || uc[i] >= 127)
					break;

			if (i < ucsize)
			{
				free(uc);
			}
			else
			{
				char *p;
				size_t psize;

				h=libmail_u_convert_fromu_init("iso-8859-1",
							       &p, &psize, 1);

				if (h)
				{
					libmail_u_convert_uc(h, uc, ucsize);
					if (libmail_u_convert_deinit(h, NULL))
						p=NULL;
				}
				else
					p=NULL;

				free(uc);

				if (!p)
				{
					callback.fail(strerror(errno));
					return;
				}

				try {
					string s=p;
					searchMessagesXpat(hdr, s,
							   searchInfo
							   .searchNot,
							   searchInfo.scope,
							   searchInfo.rangeLo,
							   searchInfo.rangeHi,
							   callback);
					free(p);
				} catch (...) {
					free(p);
					callback.fail(strerror(errno));
				}
				return;
			}
		}
	}

	switch (searchInfo.criteria) {
	case searchParams::replied:
	case searchParams::deleted:
	case searchParams::draft:
	case searchParams::unread:
		break;
	default:
		callback.fail("Not implemented.");
		return;
	}

	searchMessages::search(callback, searchInfo, this);
}

void mail::nntp::searchMessagesXpat(string hdr, string srch,
				    bool searchNot,
				    searchParams::Scope searchScope,
				    size_t rangeLo,
				    size_t rangeHi,
				    searchCallback &callback)
{

	string::iterator b, e;

	b=hdr.begin();
	e=hdr.end();

	while (b != e)
	{
		if (*b < ' ' || *b >= 127)
		{
			errno=EINVAL;
			callback.fail(strerror(errno));
		}
		++b;
	}

	b=srch.begin();
	e=srch.end();

	while (b != e)
	{
		if (*b < ' ' || *b >= 127)
		{
			errno=EINVAL;
			callback.fail(strerror(errno));
		}
		++b;
	}

	XpatTaskCallback *cb=new XpatTaskCallback(&callback);

	if (!cb)
	{
		callback.fail(strerror(errno));
		return;
	}

	try {
		installTask(new XpatTask(cb, *this,
					 openedGroup,
					 hdr, srch, searchNot,
					 searchScope, rangeLo, rangeHi));
	} catch (...) {
		delete cb;
		throw;
	}
}

void mail::nntp::genericMessageRead(string uid,
				    size_t messageNumber,
				    bool peek,
				    mail::readMode getType,
				    callback::message &callback)
{
	if (openedGroup.size() == 0)
	{
		callback.fail("Invalid message number.");
		return;
	}

	if (fixGenericMessageNumber(uid, messageNumber) &&
	    genericCachedUid(uid))
	{
		// We can optimize by taking this path

		vector<size_t> vec;

		vec.push_back(messageNumber);
		readMessageContent(vec, peek, getType, callback);
		return;
	}

	installTask(new FetchTask(&callback, *this, openedGroup,
				  messageNumber, uid, getType));
}

void mail::nntp::genericMessageSize(string uid,
				    size_t messageNumber,
				    callback::message &callback)
{
	if (!fixGenericMessageNumber(uid, messageNumber))
	{
		callback.fail("Invalid message number.");
		return;
	}

	callback.messageSizeCallback(messageNumber, 0);

	callback.success("OK"); // TODO
}

void mail::nntp::genericGetMessageFd(string uid,
				     size_t messageNumber,
				     bool peek,
				     int &fdRet,
				     callback &callback)
{
	if (openedGroup.size() == 0)
	{
		callback.fail("Invalid message number.");
		return;
	}

	if (genericTmpFp && uid == cachedUid)
	{
		fdRet=fileno(genericTmpFp);
		callback.success("OK");
		return;
	}

	installTask(new CacheTask(&callback, *this, openedGroup,
				  messageNumber, uid,
				  &fdRet, NULL));
}

bool mail::nntp::genericCachedUid(string uid)
{
	return genericTmpFp && uid == cachedUid;
}
	
void mail::nntp::genericGetMessageStruct(string uid,
					 size_t messageNumber,
					 struct rfc2045 *&structRet,
					 callback &callback)
{
	if (openedGroup.size() == 0)
	{
		callback.fail("Invalid message number.");
		return;
	}

	if (genericTmpRfcp && uid == cachedUid)
	{
		structRet=genericTmpRfcp;
		callback.success("OK");
		return;
	}

	installTask(new CacheTask(&callback, *this, openedGroup,
				  messageNumber, uid,
				  NULL, &structRet));
}

/////////////////////////////////////////////////////////////////////////////
//
// The basic NNTP task

mail::nntp::Task::Task(callback *callbackArg,
		       nntp &myserverArg)
	: callbackPtr(callbackArg), myserver( &myserverArg),
	  defaultTimeout( time(NULL) + myserver->timeoutSetting)
{
}

mail::nntp::Task::~Task()
{
	done(); // Make sure that any callback gets invoked
}

void mail::nntp::Task::resetTimeout()
{
	defaultTimeout=time(NULL) + myserver->timeoutSetting;
}

void mail::nntp::Task::done()
{
	callback *cb=callbackPtr;

	callbackPtr=NULL;

	if (myserver != NULL) // Kilroy was here
	{
		if ((*myserver->tasks.begin()) != this)
		{
			cerr << "INTERNAL ERROR: mail::nntp::Task::done()"
			     << endl;
			abort();
		}

		myserver->tasks.erase(myserver->tasks.begin());

		try {
			if (!myserver->tasks.empty())
			{
				Task *p= *myserver->tasks.begin();

				p->resetTimeout();
				p->installedTask();
			}
			else
				emptyQueue();

		} catch (...) {
			myserver=NULL;
			delete this;
			if (cb)
				cb->fail("Operation aborted.");
			throw;
		}

		
		myserver=NULL;
		delete this;
	}

	if (cb)
		cb->fail("Operation aborted.");
}

void mail::nntp::Task::emptyQueue()
{
	myserver->inactivityTimeout=time(NULL) + myserver->autologoutSetting;
}

// Subclass succeeded

void mail::nntp::Task::success(string message)
{
	callback *cb=callbackPtr;

	callbackPtr=NULL;
	done();
	if (cb)
		cb->success(message);
}

// Subclass failed

void mail::nntp::Task::fail(string message)
{
	callback *cb=callbackPtr;

	callbackPtr=NULL;
	done();
	if (cb)
		cb->fail(message);
}

// Check for task timeout.

int mail::nntp::Task::getTimeout()
{
	time_t now;

	time(&now);

	if (now < defaultTimeout)
		return defaultTimeout - now;

	string errmsg=myserver->socketDisconnect();

	if (errmsg.size() == 0)
		errmsg="Server timed out.";

	disconnected(errmsg.c_str());
	return 0;
}

// Server has disconnected.
// The default implementation takes this task off the queue,
// calls the next Task's disconnect method, then deletes
// itself.

void mail::nntp::Task::disconnected(const char *reason)
{
	callback *cb=callbackPtr;

	callbackPtr=NULL;

	if ((*myserver->tasks.begin()) != this)
	{
		cerr << "INTERNAL ERROR: mail::nntp::Task::done()" << endl;
		abort();
	}

	nntp *s=myserver;

	myserver=NULL;

	s->tasks.erase(s->tasks.begin());
	delete this;

	if (!s->tasks.empty())
		(*s->tasks.begin())->disconnected(reason);

	if (cb)
		cb->fail(reason);
}

void mail::nntp::Task::installedTask()
{
}
