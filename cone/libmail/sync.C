/*
** Copyright 2002-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "mail.H"
#include "addmessage.H"
#include "imap.H"
#include "imaplogin.H"
#include "sync.H"
#include "envelope.H"
#include "structure.H"
#include "rfcaddr.H"
#include "search.H"
#include "smtpinfo.H"
#include <cstring>

#include <iostream>
#include <map>
#include <errno.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

class mail::ACCOUNT::disconnectCallback : public mail::callback::disconnect {

	mail::ACCOUNT &account;
public:
	disconnectCallback(mail::ACCOUNT &p);
	~disconnectCallback();

	void disconnected(const char *errmsg);
	void servererror(const char *errmsg);
};

mail::ACCOUNT::disconnectCallback::disconnectCallback(mail::ACCOUNT &p)
	: account(p)
{
}

mail::ACCOUNT::disconnectCallback::~disconnectCallback()
{
}

void mail::ACCOUNT::disconnectCallback::disconnected(const char *errmsg)
{
	if (errmsg == NULL || *errmsg==0)
		errmsg="Connection closed by remote host.";

	account.disconnected(errmsg);
}

void mail::ACCOUNT::disconnectCallback::servererror(const char *errmsg)
{
	if (account.errmsg.size() == 0)
		account.errmsg=errmsg;
}

///////////////////////////////////////////////////////////////////////////
//
// Internally-maintained folder list.
//

mail::ACCOUNT::FolderList::FolderList()
{
}

mail::ACCOUNT::FolderList::~FolderList()
{
	vector<mail::folder *>::iterator b=folders.begin(), e=folders.end();

	while (b != e)
	{
		delete *b;
		b++;
	}
}

mail::ACCOUNT::FolderList::FolderList(const FolderList &l)
{
	(*this)=l;
}

mail::ACCOUNT::FolderList &mail::ACCOUNT::FolderList::operator=(const mail::ACCOUNT::FolderList &l)
{
	vector<mail::folder *>::const_iterator b=l.folders.begin(),
		e=l.folders.end();

	while (b != e)
		append(*b++);
	return *this;
}

void mail::ACCOUNT::FolderList::append(const mail::folder *f)
{
	mail::folder *g=f->clone(); // Create my own copy

	if (g == NULL)
		LIBMAIL_THROW("Out of memory.");

	try {
		folders.push_back(g);
	} catch (...)
	{
		delete g;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

mail::ACCOUNT::Store::Store()
{
}

mail::ACCOUNT::Store::~Store()
{
}

class mail::ACCOUNT::callback : public mail::callback {

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);
public:
	bool failed;
	bool succeeded;

	mail::ACCOUNT &me;

	callback(mail::ACCOUNT &mePtr);
	~callback();

	void success(string message);
	void fail(string message);
};

mail::ACCOUNT::callback::callback(mail::ACCOUNT &mePtr) : failed(false),
							  succeeded(false),
							  me(mePtr)
{
	me.errmsg=""; // Convenient place to clear the errmsg
}

mail::ACCOUNT::callback::~callback()
{
}

void mail::ACCOUNT::callback::reportProgress(size_t bytesCompleted,
					     size_t bytesEstimatedTotal,

					     size_t messagesCompleted,
					     size_t messagesEstimatedTotal)
{
	if (me.currentProgressReporter)
		me.currentProgressReporter->operator()
			(bytesCompleted, bytesEstimatedTotal,
			 messagesCompleted,
			 messagesEstimatedTotal);
}

void mail::ACCOUNT::callback::success(string message)
{
	me.errmsg=message;
	succeeded=true;
}

void mail::ACCOUNT::callback::fail(string message)
{
	me.errmsg=message;
	failed=true;
}

//////////////////////////////////////////////////////////////////////////////

mail::ACCOUNT::Progress::Progress(mail::ACCOUNT *acct)
	: reportingProgress(false), myAccount(acct)
{
	if (acct)
	{
		if (acct->currentProgressReporter)
		{
			acct->currentProgressReporter->reportingProgress
				=false;
			// Get rid of previous progress
			// reporter
		}
		acct->currentProgressReporter=this;
		reportingProgress=true;
	}
}

mail::ACCOUNT::Progress::~Progress()
{
	if (reportingProgress)
	{
		myAccount->currentProgressReporter=NULL;
	}
}

//////////////////////////////////////////////////////////////////////////////

mail::ACCOUNT::ACCOUNT() : ptr(NULL), currentProgressReporter(NULL),
			   imap_disconnect(NULL)
{
	folderCallback.account=this;
}

mail::ACCOUNT::~ACCOUNT()
{
	if (currentProgressReporter)
	{
		currentProgressReporter->reportingProgress=false;
		currentProgressReporter=NULL;
	}

	logout();
}

mail::ACCOUNT::FolderCallback::FolderCallback() : account(NULL)
{
}

mail::ACCOUNT::FolderCallback::~FolderCallback()
{
}

void mail::ACCOUNT::FolderCallback::newMessages()
{
	account->folderModifiedFlag=true;
}

void mail::ACCOUNT::FolderCallback::messagesRemoved(vector< pair<size_t,
						    size_t> > &dummy)
{
	account->folderModifiedFlag=true;
}

void mail::ACCOUNT::FolderCallback::messageChanged(size_t dummy)
{
	account->folderModifiedFlag=true;
}

////////////////////////////////////////////////////////////////////////
//
// All method calls eventually wind up here.  They create a callback
// object, call mail::account, then wait for the callback object to fire back,
// here.

bool mail::ACCOUNT::eventloop(mail::ACCOUNT::callback &callback)
{
	for (;;)
	{
		vector<pollfd> fds;
		int ioTimeout;

		mail::account::process(fds, ioTimeout);

		if (callback.succeeded || callback.failed)
			break;

		if (mail::account::poll(fds, ioTimeout) < 0)
		{
			if (errno != EINTR)
				return false;
		}
	}

	return callback.succeeded;
}

////////////////////////////////////////////////////////////////////////
//
// Invoked by mail::ACCOUNT::disconnectCallback when mail::account reports a disconnection
//

void mail::ACCOUNT::disconnected(string e)
{
	if (errmsg.size() == 0)
		errmsg=e;

	if (ptr != NULL)
	{
		mail::account *s=ptr;

		
		ptr=NULL;
		delete s;
	}
}

bool mail::ACCOUNT::disconnected()
{
	if (ptr == NULL)
	{
		if (errmsg.size() == 0)
			errmsg="Lost server connection.";
		return true;
	}
	return false;
}

bool mail::ACCOUNT::login(mail::account::openInfo openInfoArg)
{
	if (ptr)
	{
		mail::account *s=ptr;
		ptr=NULL;
		delete s;
	}

	if (imap_disconnect)
		delete imap_disconnect;

	ptr=NULL;
	imap_disconnect=NULL;

	imap_disconnect=new mail::ACCOUNT::disconnectCallback(*this);

	mail::ACCOUNT::callback callback( *this );

	if (!imap_disconnect ||
	    !(ptr=mail::account::open(openInfoArg,
				      callback, *imap_disconnect)))
	{
		return false;
	}

	return (eventloop(callback));
}

void mail::ACCOUNT::logout()
{
	errmsg="";

	if (ptr) // Still logged on.
	{
		mail::ACCOUNT::callback callback( *this );
		ptr->logout(callback);
		eventloop(callback);
	}

	if (ptr)
	{
		mail::account *p=ptr;
		ptr=NULL;
		delete p;
	}

	if (imap_disconnect)
		delete imap_disconnect;
	imap_disconnect=NULL;
}

///////////////////////////////////////////////////////////////////////////
//
// Handle mail::account calls that return a folder list.  Copy the list to
// our object.
//

class mail::ACCOUNT::readFoldersCallback : public mail::callback::folderList {
public:
	mail::ACCOUNT::FolderList &list;

	readFoldersCallback(mail::ACCOUNT::FolderList &myList);
	~readFoldersCallback();

	void success(const vector<const mail::folder *> &folders);
};



mail::ACCOUNT::readFoldersCallback
::readFoldersCallback(mail::ACCOUNT::FolderList &myList) : list(myList)
{
}

mail::ACCOUNT::readFoldersCallback
::~readFoldersCallback()
{
}

void mail::ACCOUNT::readFoldersCallback
::success(const vector<const mail::folder *> &folders)
{
	vector<const mail::folder *>::const_iterator
		b=folders.begin(), e=folders.end();

	while (b != e)
	{
		list.append( *b++ );
	}
}

bool mail::ACCOUNT::getTopLevelFolders(mail::ACCOUNT::FolderList &list)
{
	if (disconnected())
		return false;

	mail::ACCOUNT::readFoldersCallback callback1(list);
	mail::ACCOUNT::callback callback2 (*this);

	ptr->readTopLevelFolders(callback1, callback2);

	return eventloop(callback2);
}

bool mail::ACCOUNT::getSubFolders(const mail::folder *folder,
			   mail::ACCOUNT::FolderList &list)
{
	if (disconnected())
		return false;

	mail::ACCOUNT::readFoldersCallback callback1(list);
	mail::ACCOUNT::callback callback2 (*this);

	folder->readSubFolders(callback1, callback2);

	return eventloop(callback2);
}

bool mail::ACCOUNT::getParentFolder(const mail::folder *folder,
				    mail::ACCOUNT::FolderList &list)
{
	if (disconnected())
		return false;

	mail::ACCOUNT::readFoldersCallback callback1(list);
	mail::ACCOUNT::callback callback2 (*this);

	folder->getParentFolder(callback1, callback2);

	return eventloop(callback2);
}

mail::folder *mail::ACCOUNT::getFolderFromString(string name)
{
	if (disconnected())
		return NULL;
	return ptr->folderFromString(name);
}

mail::folder *mail::ACCOUNT::getFolderFromPath(string path)
{
	if (disconnected())
		return NULL;

	mail::ACCOUNT::FolderList list;
	mail::ACCOUNT::readFoldersCallback callback1(list);
	mail::ACCOUNT::callback callback2 (*this);

	ptr->findFolder(path, callback1, callback2);

	if (!eventloop(callback2))
		return NULL;

	if (list.size() != 1)
	{
		errmsg="Internal protocol error.";
		return NULL;
	}

	mail::folder *f=list[0]->clone();

	if (!f)
		errmsg=strerror(errno);
	return f;
}

std::string mail::ACCOUNT::translatePath(std::string path)
{
	if (disconnected())
		return "";

	errno=0;

	path=ptr->translatePath(path);

	if (path.size() == 0)
		errmsg=errno ? strerror(errno):"";
	return path;
}

bool mail::ACCOUNT::folderModified()
{
	bool f=folderModifiedFlag;

	folderModifiedFlag=false;
	return f;
}

mail::folder *mail::ACCOUNT::createFolder(const mail::folder *folder,
				   string name, bool isdir)
{
	if (disconnected())
		return NULL;

	mail::ACCOUNT::FolderList list;
	mail::ACCOUNT::readFoldersCallback callback1(list);
	mail::ACCOUNT::callback callback2 (*this);

	folder->createSubFolder(name, isdir, callback1, callback2);

	if (!eventloop(callback2) || list.size() == 0)
		return NULL;

	return list[0]->clone();
}

mail::folder *mail::ACCOUNT::renameFolder(const mail::folder *folder,
					  const mail::folder *newParent,
					  string name)
{
	if (disconnected())
		return NULL;

	mail::ACCOUNT::FolderList list;
	mail::ACCOUNT::readFoldersCallback callback1(list);
	mail::ACCOUNT::callback callback2 (*this);

	folder->renameFolder(newParent, name, callback1, callback2);

	if (!eventloop(callback2) || list.size() == 0)
		return NULL;

	return list[0]->clone();
}

bool mail::ACCOUNT::createFolder(const mail::folder *folder,  bool isdir)
{
	if (disconnected())
		return false;

	mail::ACCOUNT::callback callback2 (*this);

	folder->create(isdir, callback2);

	return eventloop(callback2);
}

bool mail::ACCOUNT::deleteFolder(const mail::folder *folder, bool deleteDir)
{
	if (disconnected())
		return false;

	mail::ACCOUNT::callback callback(*this);

	folder->destroy(callback, deleteDir);

	return eventloop(callback);
}

bool mail::ACCOUNT::checkNewMail()
{
	if (disconnected())
		return false;

	mail::ACCOUNT::callback callback(*this);
	ptr->checkNewMail(callback);
	eventloop(callback);
	return folderModified();
}

bool mail::ACCOUNT::saveFolderIndexInfo(size_t messageNum,
				 const mail::messageInfo &newInfo)
{
	if (disconnected())
		return false;

	mail::ACCOUNT::callback callback(*this);

	ptr->saveFolderIndexInfo(messageNum, newInfo, callback);
	return eventloop(callback);
}

bool mail::ACCOUNT::updateFolderIndexInfo()
{
	if (disconnected())
		return false;

	mail::ACCOUNT::callback callback(*this);

	ptr->updateFolderIndexInfo(callback);

	return 	eventloop(callback);
}

bool mail::ACCOUNT::removeMessages(const vector<size_t> &messages)
{
	if (disconnected())
		return false;

	mail::ACCOUNT::callback callback(*this);

	ptr->removeMessages(messages, callback);

	return 	eventloop(callback);
}

bool mail::ACCOUNT::updateFolderIndexFlags(const vector<size_t> &messages,
				    bool doFlip,
				    bool enableDisable,
				    const mail::messageInfo &flags)
{
	if (disconnected())
		return false;

	mail::ACCOUNT::callback callback(*this);

	ptr->updateFolderIndexFlags(messages, doFlip, enableDisable, flags,
				    callback);

	return eventloop(callback);
}

bool mail::ACCOUNT::copyMessagesTo(const vector<size_t> &messages,
				   mail::folder *copyTo)
{
	if (disconnected())
		return false;

	mail::ACCOUNT::callback callback(*this);

	ptr->copyMessagesTo(messages, copyTo, callback);
	return eventloop(callback);
}

bool mail::ACCOUNT::moveMessagesTo(const vector<size_t> &messages,
				   mail::folder *copyTo)
{
	if (disconnected())
		return false;

	mail::ACCOUNT::callback callback(*this);

	ptr->moveMessagesTo(messages, copyTo, callback);
	return eventloop(callback);
}

mail::ACCOUNT::FolderInfo::FolderInfo()
	: messageCount(0), unreadCount(0), fastInfo(false)
{
}

mail::ACCOUNT::FolderInfo::~FolderInfo()
{
}

bool mail::ACCOUNT::readFolderInfo(const mail::folder *folder, FolderInfo &info)
{
	if (disconnected())
		return false;

	mail::ACCOUNT::callback callback(*this);
	mail::callback::folderInfo callback2;

	callback2.fastInfo=info.fastInfo;

	folder->readFolderInfo(callback2, callback);

	bool rc=eventloop(callback);

	info.unreadCount=callback2.unreadCount;
	info.messageCount=callback2.messageCount;
	return rc;
}

bool mail::ACCOUNT::openFolder(const mail::folder *folder,
			       mail::snapshot *restoreSnapshot)
{
	if (disconnected())
		return false;

	mail::ACCOUNT::callback callback(*this);

	folder->open(callback, restoreSnapshot, folderCallback);
	bool f=eventloop(callback);

	if (f)
		folderModifiedFlag=false; // Clean slate, now.
	return f;	
}

size_t mail::ACCOUNT::getFolderIndexSize()
{
	if (disconnected())
		return 0;

	return ptr->getFolderIndexSize();
}

mail::messageInfo mail::ACCOUNT::getFolderIndexInfo(size_t n)
{
	if (n >= getFolderIndexSize())
		return mail::messageInfo();

	return ptr->getFolderIndexInfo(n);
}

void mail::ACCOUNT::getFolderKeywordInfo(size_t msgNum,
					 set<string> &keywords)
{
	if (msgNum >= getFolderIndexSize())
	{
		keywords.clear();
		return;
	}
	return ptr->getFolderKeywordInfo(msgNum, keywords);
}

bool mail::ACCOUNT::updateKeywords(const std::vector<size_t> &messages,
				   const std::set<std::string> &keywords,
				   bool setOrChange,
				   // false: set, true: see changeTo
				   bool changeTo)
{
	if (disconnected())
		return false;

	mail::ACCOUNT::callback callback(*this);

	ptr->updateKeywords(messages, keywords, setOrChange, changeTo,
			    callback);
	return eventloop(callback);
}

bool mail::ACCOUNT::updateKeywords(const std::vector<size_t> &messages,
				   const std::vector<std::string> &keywords,
				   bool setOrChange,
				   // false: set, true: see changeTo
				   bool changeTo)
{
	set<string> keySet;

	keySet.insert(keywords.begin(), keywords.end());
	return updateKeywords(messages, keySet, setOrChange, changeTo);
}

bool mail::ACCOUNT::updateKeywords(const std::vector<size_t> &messages,
				   const std::list<std::string> &keywords,
				   bool setOrChange,
				   // false: set, true: see changeTo
				   bool changeTo)
{
	set<string> keySet;

	keySet.insert(keywords.begin(), keywords.end());
	return updateKeywords(messages, keySet, setOrChange, changeTo);
}

////////////////////////////////////////////////////////////////////////////
//
// Combo callback object handles whatever mail::callback::message throws
// at it, and sorts it based on the original message number set.
//

class mail::ACCOUNT::readMessageCallback : public mail::callback::message ,
				  public mail::ACCOUNT::callback {

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);
public:
	map<size_t, mail::envelope> envelopeList;
	map<size_t, vector<string> > referenceList;

	map<size_t, mail::mimestruct> structureList;
	map<size_t, time_t> arrivalList;
	map<size_t, unsigned long> sizeList;

	mail::ACCOUNT::Store *store;

	readMessageCallback(mail::ACCOUNT &me,
			    mail::ACCOUNT::Store *storeArg=NULL);
	~readMessageCallback();

	void success(string message);
	void fail(string message);

	void messageEnvelopeCallback(size_t messageNumber,
				     const mail::envelope &envelope);
	void messageReferencesCallback(size_t messageNumber,
				       const vector<std::string> &references);

	void messageArrivalDateCallback(size_t messageNumber,
					time_t datetime);

	void messageSizeCallback(size_t messageNumber,
				 unsigned long size);

	void messageStructureCallback(size_t messageNumber,
				      const mail::mimestruct
				      &messageStructure);

	void messageTextCallback(size_t n, string text);
};


mail::ACCOUNT::readMessageCallback
::readMessageCallback(mail::ACCOUNT &me,
		      mail::ACCOUNT::Store *storeArg)
	: mail::ACCOUNT::callback(me), store(storeArg)
{
}

mail::ACCOUNT::readMessageCallback::~readMessageCallback()
{
}

void mail::ACCOUNT
::readMessageCallback::reportProgress(size_t bytesCompleted,
				      size_t bytesEstimatedTotal,

				      size_t messagesCompleted,
				      size_t messagesEstimatedTotal)
{
}

void mail::ACCOUNT::readMessageCallback::success(string message)
{
	mail::ACCOUNT::callback::success(message);
}

void mail::ACCOUNT::readMessageCallback::fail(string message)
{
	mail::ACCOUNT::callback::fail(message);
}

void mail::ACCOUNT::readMessageCallback
::messageEnvelopeCallback(size_t messageNumber,
			  const mail::envelope &envelope)
{
	envelopeList.insert(make_pair(messageNumber, envelope));
}


void mail::ACCOUNT::readMessageCallback
::messageReferencesCallback(size_t messageNumber,
			    const vector<std::string> &references)
{
	referenceList.insert(make_pair(messageNumber, references));
}

void mail::ACCOUNT::readMessageCallback
::messageArrivalDateCallback(size_t messageNumber,
			     time_t datetime)
{
	arrivalList.insert(make_pair(messageNumber, datetime));
}

void mail::ACCOUNT::readMessageCallback
::messageSizeCallback(size_t messageNumber,
		      unsigned long size)
{
	sizeList.insert(make_pair(messageNumber, size));
}

void mail::ACCOUNT::readMessageCallback
::messageStructureCallback(size_t messageNumber,
			   const mail::mimestruct &messageStructure)
{
	structureList.insert(make_pair(messageNumber, messageStructure));
}

void mail::ACCOUNT::readMessageCallback::messageTextCallback(size_t n,
							     string text)
{
	if (store)
		store->store(n, text);
}

///////////////////////////////////////////////////////////////////////////

bool mail::ACCOUNT::getMessageEnvelope(const vector<size_t> &messages,
				vector<mail::xenvelope> &envelopes)
{
	envelopes.clear();

	if (disconnected())
		return false;

	mail::ACCOUNT::readMessageCallback callback(*this);

	ptr->readMessageAttributes(messages,
				   mail::account::ARRIVALDATE |
				   mail::account::ENVELOPE |
				   mail::account::MESSAGESIZE,
				   callback);

	if (!eventloop(callback))
		return false;

	vector<size_t>::const_iterator b=messages.begin(), e=messages.end();

	while (b != e)
	{
		size_t n= *b++;

		mail::xenvelope x;

		if (callback.envelopeList.count(n) > 0)
			x=callback.envelopeList.find(n)->second;
		if (callback.referenceList.count(n) > 0)
			x.references=callback.referenceList.find(n)->second;
		if (callback.arrivalList.count(n) > 0)
			x.arrivalDate=callback.arrivalList.find(n)->second;
		if (callback.sizeList.count(n) > 0)
			x.messageSize=callback.sizeList.find(n)->second;
		envelopes.push_back(x);
	}

	return true;
}

bool mail::ACCOUNT::getMessageStructure(const vector<size_t> &messages,
				 vector<mail::mimestruct> &structures)
{
	structures.clear();

	if (disconnected())
		return false;

	mail::ACCOUNT::readMessageCallback callback(*this);

	ptr->readMessageAttributes(messages, mail::account::MIMESTRUCTURE, callback);

	if (!eventloop(callback))
		return false;

	vector<size_t>::const_iterator b=messages.begin(), e=messages.end();

	while (b != e)
	{
		size_t n= *b++;

		if (callback.structureList.count(n) == 0)
			structures.push_back(mail::mimestruct());
		else
			structures.push_back(callback.structureList.find(n)
					    ->second);
	}

	return true;
}

mail::xenvelope mail::ACCOUNT::getMessageEnvelope(size_t messageNum)
{
	vector<size_t> vec;
	vector<mail::xenvelope> envs;

	vec.push_back(messageNum);

	if (getMessageEnvelope(vec, envs))
		return envs[0];
	return mail::xenvelope();
}

mail::mimestruct mail::ACCOUNT::getMessageStructure(size_t messageNum)
{
	vector<size_t> vec;
	vector<mail::mimestruct> strs;

	vec.push_back(messageNum);

	if (getMessageStructure(vec, strs))
		return strs[0];
	return mail::mimestruct();
}

bool mail::ACCOUNT::getMessageContent(const vector<size_t> &messages,
			       bool peek,
			       enum mail::readMode getType,
			       mail::ACCOUNT::Store &contentCallback)
{
	if (disconnected())
		return false;

	mail::ACCOUNT::readMessageCallback callback(*this, &contentCallback);

	ptr->readMessageContent(messages, peek, getType, callback);

	return (eventloop(callback));
}

bool mail::ACCOUNT::addMessage(const mail::folder *folder,
			mail::addMessagePull &message)
{
	if (disconnected())
		return false;

	mail::ACCOUNT::callback callback(*this);

	mail::addMessage *ptr=folder->addMessage(callback);

	if (!ptr)
		return false;

	try {
		ptr->messageInfo=message.messageInfo;
		ptr->messageDate=message.messageDate;

		string s;

		while ((s=message.getMessageContents()).size() > 0)
			ptr->saveMessageContents(s);
		ptr->go();
	} catch (...) {
		ptr->fail("APPEND interrupted.");
	}

	return eventloop(callback);
}

//
// Collect mail::searchCallback results.
//

class mail::ACCOUNT::SearchCallback : public mail::searchCallback {

	mail::ACCOUNT::callback &callback;

	vector<size_t> &found;

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);

public:
	SearchCallback(mail::ACCOUNT::callback &callbackArg,
		       vector<size_t> &foundArg);

	~SearchCallback();

	void success(const vector<size_t> &found);
	void fail(string);
};

mail::ACCOUNT::SearchCallback
::SearchCallback(mail::ACCOUNT::callback &callbackArg,
		 vector<size_t> &foundArg)
	: callback(callbackArg), found(foundArg)
{
}

mail::ACCOUNT::SearchCallback::~SearchCallback()
{
}

void mail::ACCOUNT
::SearchCallback::reportProgress(size_t bytesCompleted,
				 size_t bytesEstimatedTotal,

				 size_t messagesCompleted,
				 size_t messagesEstimatedTotal)
{
}

void mail::ACCOUNT::SearchCallback::success(const vector<size_t> &foundArg)
{
	found.insert(found.end(), foundArg.begin(), foundArg.end());
	callback.success("SEARCH completed.");
}

void mail::ACCOUNT::SearchCallback::fail(string message)
{
	callback.fail(message);
}


bool mail::ACCOUNT::searchMessages(const mail::searchParams &searchInfo,
			    vector<size_t> &messageList)
{
	if (disconnected())
		return false;

	mail::ACCOUNT::callback callback(*this);

	mail::ACCOUNT::SearchCallback search_callback(callback, messageList);

	ptr->searchMessages(searchInfo, search_callback);

	return eventloop(callback);
}

bool mail::ACCOUNT::send(const mail::smtpInfo &messageInfo,
			 const mail::folder *saveFolder,
			 mail::addMessagePull &message)
{
	if (disconnected())
		return false;

	bool rc;

	mail::folder *sendFolder=
		ptr->getSendFolder(messageInfo, saveFolder, errmsg);

	if (!sendFolder)
		return false;

	try {
		rc=addMessage(sendFolder, message);
		delete sendFolder;
	} catch (...) {
		delete sendFolder;
		errmsg="An exception occurred while sending the message";
		rc=false;
	}
	return rc;
}

bool mail::ACCOUNT::getMyRights(const mail::folder *folder, string &rights)
{
	if (disconnected())
		return false;

	mail::ACCOUNT::callback callback (*this);

	folder->getMyRights(callback, rights);

	return eventloop(callback);
}

bool mail::ACCOUNT::getRights(const mail::folder *folder,
			      list<pair<string, string> > &rights)
{
	if (disconnected())
		return false;

	mail::ACCOUNT::callback callback (*this);

	folder->getRights(callback, rights);

	return eventloop(callback);
}

bool mail::ACCOUNT::setRights(const mail::folder *folder,
			      string &errorIdentifier,
			      vector<string> &errorRights,
			      string identifier,
			      string rights)
{
	if (disconnected())
		return false;

	mail::ACCOUNT::callback callback (*this);

	folder->setRights(callback, errorIdentifier,
			  errorRights,
			  identifier,
			  rights);

	return eventloop(callback);
}

bool mail::ACCOUNT::delRights(const mail::folder *folder,
			      string &errorIdentifier,
			      vector<string> &errorRights,
			      string identifier)
{
	if (disconnected())
		return false;

	mail::ACCOUNT::callback callback (*this);

	folder->delRights(callback, errorIdentifier,
			  errorRights,
			  identifier);

	return eventloop(callback);
}
