/*
** Copyright 2003-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "config.h"
#include "myfolder.H"
#include "myfolderfilter.H"
#include "myserver.H"
#include "mymessage.H"
#include "tags.H"
#include "cursesmessage.H"
#include "myservercallback.H"
#include "gettext.H"
#include "curses/cursesstatusbar.H"
#include "rfc822/rfc822.h"
#include "rfc2045/rfc2045.h"
#include "libmail/rfc2047decode.H"
#include "unicode/unicode.h"
#include "libmail/envelope.H"
#include "libmail/rfcaddr.H"
#include "libmail/autodecoder.H"

#include <errno.h>

#include <algorithm>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <iostream>

using namespace std;

extern CursesStatusBar *statusBar;
extern void folderIndexScreen(void *);
extern void showCurrentMessage(void *);
extern void showAttachments(void *);
extern void editScreen(void *);

#define SORT_THREAD 'T'
#define SORT_THREAD_S "T"
#define SNAPSHOTFORMAT "1" // Current format for saved indexes.


//////////////////////////////////////////////////////////////////////////////
//
// Helper class for threading.

class myFolder::thread {

public:

	static void resort(myFolder &f);

	static void resort(myFolder &f,	struct imap_refmsgtable *t);

	static void resort_layout(myFolder &,
				  struct imap_refmsg *,
				  struct imap_refmsg *&,

				  vector<size_t> &,
				  size_t);

	static void resort_layout(myFolder &,
				  struct imap_refmsg *,
				  vector<size_t> &,
				  size_t, bool);

};



/////////////////////////////////////////////////////////////////////
//
// Helper class for restoring saved snapshots


myFolder::RestoreSnapshot::RestoreSnapshot(myFolder *mf)
	: snapshotId(""), nmessages(0)
{
	cachefile=mf->getServer()
		->getFolderConfiguration(mf->getFolder(), "SNAPSHOT");

	if (cachefile.size() == 0) // No cache file defined.
		return;

	cachefile=myServer::getConfigDir() + "/" + cachefile;

	i.open(cachefile.c_str());

	if (!i.is_open())
		return;

	string line;

	if (getline(i, line).fail())
	{
		i.close();
		return;
	}

	if (line.substr(0, sizeof(SNAPSHOTFORMAT ":")-1) != SNAPSHOTFORMAT ":")
	{
		i.close();
		return;
	}

	line=line.substr(sizeof(SNAPSHOTFORMAT ":")-1);

	size_t n=line.find(':');

	if (n == std::string::npos)
	{
		i.close();
		return;
	}

	snapshotId=line.substr(n+1);

	istringstream ii(line);

	ii >> nmessages;

	if (ii.fail() || ii.bad())
	{
		i.close();
		return;
	}

}

myFolder::RestoreSnapshot::~RestoreSnapshot()
{
}

void myFolder::RestoreSnapshot::getSnapshotInfo(string &snapshotIdArg,
						size_t &nMessagesArg)
{
	if (i.is_open())
	{
		snapshotIdArg=snapshotId;
		nMessagesArg=nmessages;
	}
	else
	{
		snapshotIdArg="";
		nMessagesArg=0;
	}
}

void myFolder::RestoreSnapshot::restoreSnapshot(mail::snapshot::restore &r)
{
	if (!i.is_open())
	{
		r.abortRestore();
		return;
	}

	size_t n;

	string l;

	for (n=0; n<nmessages; n++)
	{
		if (getline(i, l).fail())
		{
			cerr << (string)
				( Gettext(_("%1%: unable to read snapshot."))
				  << cachefile) << endl;
			r.abortRestore();
			return;
		}

		r.restoreIndex(n, mail::messageInfo(l));
	}

	while (!getline(i, l).fail())
	{
		{
			istringstream is(l);

			is >> n;

			if (is.fail() || n >= nmessages)
			{
				cerr << (string)
					( Gettext(_("%1%: unable to read"
						    " snapshot."))
					  << cachefile) << endl;
				r.abortRestore();
				return;
			}
		}

		set<string> kwset;

		for (;;)
		{
			if (getline(i, l).fail())
			{
				cerr << (string)
					( Gettext(_("%1%: unable to read"
						    " snapshot."))
					  << cachefile) << endl;
				r.abortRestore();
				return;
			}

			if (l.size() == 0)
				break;

			kwset.insert(l);
		}

		if (kwset.empty())
			continue;

		r.restoreKeywords(n, kwset);
	}
}

/////////////////////////////////////////////////////////////////////
//
// Helper class for reading the message index.
//
// When newMessages() notification is received, this class is started,
// and a request is made to download the headers from all new messages.
// When the request completes, the new messages are shown on the screen.

class myFolder::FolderIndexUpdate : public mail::callback::message {

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);

public:
	mail::ptr<myFolder> folder;
	mail::callback &successOrFail;
	size_t totalCount;
	size_t doneCount;

	FolderIndexUpdate(myFolder &folderArg,
			  mail::callback &successOrFailArg);
	~FolderIndexUpdate();

	// Inherited from mail::callback::message

	void messageEnvelopeCallback(size_t messageNumber,
				     const class mail::envelope
				     &envelope);

	void messageReferencesCallback(size_t messageNumber,
				       const std::vector<std::string>
				       &references);

	void messageArrivalDateCallback(size_t messageNumber,
					time_t datetime);

	void messageSizeCallback(size_t messageNumber,
				 unsigned long size);


	// Inherited from mail::callback

	void success(string);
	void fail(string);
};

/////////////////////////////////////////////////////////////////////
//
// Helper class for updating the message index.

class myFolder::NewMailUpdate : public mail::callback {

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);

public:
	myFolder::FolderIndexUpdate *update_struct;

	NewMailUpdate();
	~NewMailUpdate();

	void success(string);
	void fail(string);
};


myFolder::myFolder(myServer *serverArg,
		   const mail::folder *folderArg)
	: PreviousScreen(&folderIndexScreen, this),
	  folder(NULL), server(serverArg),
	  isClosing(false),
	  saveFirstRowShown(0),
	  isExpungingDrafts(false),
	  mustReopen(false),
	  currentFilter(NULL),
	  currentMessage(0), mymessage(0),
	  currentDisplay(NULL), expunge_count(0), newMailCheckCount(0),
	  sort_function(&myFolder::sortByArrival)
{
	expungedTimer=this;
	expungedTimer= &myFolder::unsolicitedExpunge;
	setSortFunctionNoSort("A");
	if ((folder=folderArg->clone()) == NULL)
		outofmemory();
}

myFolder::~myFolder()
{
	if (currentDisplay)
		currentDisplay->f=NULL;

	if (currentFilter)
		currentFilter->folder=NULL;

	if (mymessage)
	{
		CursesMessage *m=mymessage;

		mymessage=NULL;

		m->myfolder=NULL;
		delete m;
	}

	if (folder)
		delete folder;
}


bool myFolder::isCheckingNewMail()
{
	return newMailCheckCount > 0 || currentFilter != NULL;
}

void myFolder::quiesce()
{
	bool showingStatus=false;

	mail::ptr<myFolder> folderPtr=this;

	while (!folderPtr.isDestroyed() && isCheckingNewMail())
	{
		if (!showingStatus)
		{
			showingStatus=true;
			statusBar->clearstatus();
			statusBar->status(_("Closing folder..."));
		}

		myServer::Callback cb;

		checkNewMail(cb);

		myServer::eventloop(cb);
	}
}

// New messages arrived in folder.

void myFolder::newMessages()
{
	if (isClosing)
		return;

	size_t currentMessageCount=
		server->server->getFolderIndexSize();

	if (serverIndex.size() < currentMessageCount)
	{
		vector<size_t> newmessages;

		size_t n;

		for (n=serverIndex.size(); n<currentMessageCount; n++)
			newmessages.push_back(n);

		serverIndex.insert(serverIndex.end(),
				   currentMessageCount - serverIndex.size(),
				   Index());

		NewMailUpdate *new_mail_update=new NewMailUpdate();

		if (!new_mail_update)
			outofmemory();

		try {
			FolderIndexUpdate *update_index=
				new FolderIndexUpdate(*this,
						      *new_mail_update);

			if (!update_index)
				outofmemory();

			new_mail_update->update_struct=update_index;

			try {
				++newMailCheckCount;

				update_index->totalCount=newmessages.size();

				server->server->
					readMessageAttributes(newmessages,
							      mail::account::ENVELOPE |
							      mail::account::MESSAGESIZE |
							      mail::account::ARRIVALDATE,
							       *update_index);
			} catch (...) {
				--newMailCheckCount;
				delete update_index;
				LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
			}
		} catch (...) {
			delete new_mail_update;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}
	}

}

//
// When the folder update request completes, newMessagesReceived() is invoked
// to process the new messages.

void myFolder::newMessagesReceived()
{
	if (--newMailCheckCount)
		return; // More updates in progress.

	if (currentFilter)
		return; // Filtering is in progress

	if (installFilter() != NULL) // This folder has a filter
	{
		currentFilter->filterStart=sorted_index.size();
		currentFilter->filterEnd=serverIndex.size();
		currentFilter->start();
		return;
	}

	newMessagesReceivedAndFiltered(serverIndex.size());
}

// myFolderFilter completed its task.
//
// A) No new messages received in the meantime.
// B) New messages received, header fetching is in progress
// C) New messages received, all header fetching is complete

void myFolder::messagesFiltered()
{
	if (newMailCheckCount)
	{
		// B).  Just show what's been filtered so far,
		// newMailReceived() will be called again, and we'll resume
		// at that time.

		size_t newMessages=currentFilter->filterEnd;

		delete currentFilter;

		newMessagesReceivedAndFiltered(newMessages);
		return;
	}

	if (currentFilter->filterEnd == serverIndex.size())
	{
		// A).

		delete currentFilter;
		newMessagesReceivedAndFiltered(serverIndex.size());
		return;
	}

	// C).

	currentFilter->filterStart=currentFilter->filterEnd;
	currentFilter->filterEnd=serverIndex.size();
	currentFilter->start();
}

void myFolder::newMessagesReceivedAndFiltered(size_t newMessages)
{
	size_t oldMessages=sorted_index.size();

	if (oldMessages > newMessages)
		LIBMAIL_THROW("Internal error in newMessagesReceivedAndFiltered");
	size_t newMessageCount = newMessages - oldMessages;

	if (sort_function_name.find(SORT_THREAD)
	    != std::string::npos)
	{
		while (oldMessages < newMessages)
		{
			mail::messageInfo flags=server->server->
				getFolderIndexInfo(oldMessages);

			Index &n=serverIndex[oldMessages];

			n.uid=flags.uid;
			n.setStatusCode(flags);

			{
				set<string> keywords;

				server->server->
					getFolderKeywordInfo(oldMessages,
							     keywords);

				n.setTag(keywords);
			}
			
			sorted_index.push_back(oldMessages);
			++oldMessages;
		}

		// No clean way to do it, except another sort.
		setSortFunctionNoSave(sort_function_name);
	}
	else
	{
		IndexSort cmpFunc(*this);


		while (oldMessages < newMessages)
		{
			mail::messageInfo flags=server->server->
				getFolderIndexInfo(oldMessages);

			Index &n=serverIndex[oldMessages];

			n.uid=flags.uid;
			n.setStatusCode(flags);

			{
				set<string> keywords;

				server->server->
					getFolderKeywordInfo(oldMessages,
							     keywords);

				n.setTag(keywords);
			}

			vector<size_t>::iterator insertAt
				=lower_bound(sorted_index.begin(),
					     sorted_index.end(),
					     oldMessages, cmpFunc);

			if (currentMessage >=
			    (size_t)(insertAt - sorted_index.begin()) &&
			    currentMessage < sorted_index.size())
				++currentMessage;

			sorted_index.insert(insertAt, oldMessages);
			++oldMessages;
		}
	}

	if (mymessage)
		mymessage->folderResorted();

	if (currentDisplay)
		currentDisplay->draw();

	if (newMessageCount > 0)
	{
		getServer()->saveFolderIndex( this );

		if (!isExpungingDrafts) // Suppress message
		{
			statusBar->status(Gettext(_("%1% new message(s) in %2%"))
					  << newMessageCount
					  << folder->getName());
			statusBar->beepError();
		}
	}
}

//
// After messages are removed from the folder, a number of things must
// be recomputed.  Here we go...
//

void myFolder::messagesRemoved(vector< pair<size_t, size_t> > &removedList)
{
	vector< pair<size_t, size_t> >::iterator rb, re;

	rb=removedList.begin();
	re=removedList.end();

	while (rb != re)
	{
		--re;

		if (re->first >= serverIndex.size())
			continue; /* Huh??? */

		size_t nfrom=re->first;
		size_t nto=re->second;

		if (nfrom >= serverIndex.size())
			continue; // Huh?

		if (nto >= serverIndex.size())
			nto=serverIndex.size()-1;

		// Erase native messages nfrom-nto

		serverIndex.erase(serverIndex.begin()+nfrom,
				  serverIndex.begin()+nto+1);

		// Now go through the index of messages in sorted order,
		// and make the necessary adjustments.

		size_t i;

		for (i=0; i<sorted_index.size(); )
		{
			if (sorted_index[i] >= nfrom &&
			    sorted_index[i] <= nto) // This msg is gone.
			{
				sorted_index.erase(sorted_index.begin()+i,
						   sorted_index.begin()+i+1);

				if (currentMessage == i && mymessage != NULL)
				{
					statusBar->status(_("Currently opened "
							    "message deleted "
							    "on the server."),
							  statusBar
							  ->SERVERERROR);
					statusBar->beepError();

					if (Curses::keepgoing)
					{
						Curses::keepgoing=false;
						myServer::nextScreen=
							&folderIndexScreen;
						myServer::nextScreenArg=this;
					}

					delete mymessage;
				}

				if (currentMessage >= i)
				{
					if (currentMessage > 0)
					{
						--currentMessage;
						if (mymessage)
							mymessage->messagesortednum
								= currentMessage;
					}
				}
				continue;
			}

			if (sorted_index[i] > nto)
				sorted_index[i] -= nto - nfrom + 1;
			i++;
		}

		if (mymessage && nto <= mymessage->messagenum)
			mymessage->messagenum -= nto - nfrom + 1;

		size_t expunge_count_temp=nto - nfrom + 1;

		// If there's a filter running, update the filtering range.

		if (currentFilter)
		{
			size_t t= nto+1;

			if (t > currentFilter->filterEnd)
				t=currentFilter->filterEnd;

			if (t > currentFilter->filterStart &&
			    nfrom < currentFilter->filterEnd)
			{
				t -= currentFilter->filterStart;
				if (nfrom > currentFilter->filterStart)
					t -= nfrom
						- currentFilter->filterStart;

				currentFilter->filterEnd -= t;

				expunge_count_temp -= t;
				// DO NOT COUNT filtered messages that were
				// expunged!  We don't need to know about them.
			}

			t=nto+1;

			if (t > currentFilter->filterStart)
				t=currentFilter->filterStart;

			if (nfrom < currentFilter->filterStart)
			{
				t -= nfrom;

				currentFilter->filterStart -= t;
				currentFilter->filterEnd -= t;
			}
		}

		expunge_count += expunge_count_temp;
	}

	if (mymessage)
		mymessage->folderResorted();

	if (mymessage && myMessage::FolderUpdateCallback::currentCallback)
		myMessage::FolderUpdateCallback::currentCallback
			->folderUpdated();

	if (myServer::cmdcount == 0 // Not in command, must be unsolicited
	    && expunge_count)
		// Do not start the timer if only filtered msgs were expunged
	{
		struct timeval tv;

		tv.tv_sec=0;
		tv.tv_usec=100000;
		expungedTimer.setTimer(tv);
	}
}

//
// When a timer after an unsolicited expunge msg goes off (timer is used
// to combine multiple unsolicited events into one event), unsolicitedExpunge()
// gets invoked.

void myFolder::unsolicitedExpunge()
{
	checkExpunged();
}

void myFolder::setTag(size_t n, size_t tag)
{
	if (n >= size())
		return;

	vector<size_t> v;

	v.push_back(getServerIndex(n));
	setTag(v, tag);
}

void myFolder::setTag(vector<size_t> &v, size_t tag)
{
	myServer *s=getServer();

	if (s && s->server && tag < Tags::tags.names.size())
	{
		set<string> uids;
		size_t i;

		for (i=0; i<v.size(); i++)
		{
			if (v[i] >= s->server->getFolderIndexSize())
				continue;

			uids.insert(s->server->getFolderIndexInfo(v[i]).uid);
		}

		set<string> kSet;

		for (i=1; i<Tags::tags.names.size(); i++)
			if (i != tag)
				kSet.insert(Tags::tags.getTagName(i));

		{
			myServer::Callback cb;

			s->server->updateKeywords(v, kSet, true, false, cb);

			if (!myServer::eventloop(cb) ||
			    !s->server)
				return;
		}

		if (tag == 0)
			return;

		kSet.clear();
		kSet.insert(Tags::tags.getTagName(tag));
		v.clear();

		size_t n=s->server->getFolderIndexSize();

		for (i=0; i<n; i++)
			if (uids.count(s->server->getFolderIndexInfo(i).uid)
			    > 0)
				v.push_back(i);

		if (v.size() > 0)
		{
			myServer::Callback cb;

			s->server->updateKeywords(v, kSet, true, true, cb);

			myServer::eventloop(cb);
		}
	}
}

void myFolder::markDeleted(size_t n, bool isDeleted, bool setStatusFlag)
{
	if (n >= size())
		return;

	size_t sIndex=getServerIndex(n);

	myServer *s=getServer();

	if (s && s->server)
	{
		mail::messageInfo msgInfo=
			s->server->getFolderIndexInfo(sIndex);

		if (!isDeleted && !msgInfo.deleted)
			msgInfo.unread=true;

		msgInfo.deleted=isDeleted;

		statusBar->clearstatus();
		if (setStatusFlag)
		{
			statusBar->status(isDeleted
					  ? _("Deleting message...")
					  : _("Undeleting message..."));
		}

		DelUndelCallback *cb= new DelUndelCallback;

		if (!cb)
			outofmemory();

		try {
			s->server->saveFolderIndexInfo(sIndex, msgInfo, *cb);
		} catch (...) {
			delete cb;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}
	}
}

void myFolder::watch(size_t n, unsigned nDays, unsigned nLevels)
{
	if (n >= size())
		return;

	setWatch(sorted_index[n], nDays, nLevels);
	watchUpdated();
	if (currentDisplay)
		currentDisplay->draw(n);
}

void myFolder::setWatch(size_t n, unsigned nDays, unsigned nLevels)
{
	Index &i=serverIndex[n];

	i.watchLevel=nLevels;
	watchList(i.messageid, time(NULL) + nDays * 24 * 60 * 60, nLevels);
}

void myFolder::unwatch(size_t n)
{
	if (n >= size())
		return;

	setUnwatch(sorted_index[n]);
	watchUpdated();
	if (currentDisplay)
		currentDisplay->draw(n);
}

void myFolder::setUnwatch(size_t n)
{
	Index &i=serverIndex[n];

	i.watchLevel=0;
	watchList.unWatch(i.messageid);
}

void myFolder::watchUpdated()
{
	statusBar->clearstatus();
	statusBar->status(_("Updating watch list..."));
	statusBar->flush();
	getServer()->saveFolderIndex(this);

	statusBar->status(_("... done"));
}

void myFolder::toggleMark(size_t n)
{
	if (n >= size())
		return;

	size_t sIndex=getServerIndex(n);

	myServer *s=getServer();

	if (s && s->server)
	{
		mail::messageInfo msgInfo=
			s->server->getFolderIndexInfo(sIndex);

		msgInfo.marked= !msgInfo.marked;

		statusBar->clearstatus();
		statusBar->status(msgInfo.marked
				  ? _("Marking message...")
				  : _("Unmarking message..."));

		DelUndelCallback *cb= new DelUndelCallback;

		if (!cb)
			outofmemory();

		try {
			s->server->saveFolderIndexInfo(sIndex, msgInfo, *cb);
		} catch (...) {
			delete cb;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}
	}
}

void myFolder::checkExpunged()
{
	size_t save_expunge_count;

	if ((save_expunge_count=expunge_count) == 0)
		return;

	expunge_count=0;

	if (currentDisplay == NULL || isExpungingDrafts)
		return;

	if (save_expunge_count > 0 &&
	    sort_function_name.find(SORT_THREAD)
	    != std::string::npos)
	{
		// No clean way to do it, except another sort.
		setSortFunctionNoSave(sort_function_name);
	}

	currentDisplay->draw();
	statusBar->status( Gettext(_("%1% message(s) deleted in %2%"))
			   << save_expunge_count << folder->getName(),
			   statusBar->EXPUNGED);
	statusBar->beepError();
}

void myFolder::messageChanged(size_t n)
{
	size_t i;

	if (n >= serverIndex.size())
		return; // Huh?

	mail::messageInfo flags=server->server->getFolderIndexInfo(n);

	serverIndex[n].setStatusCode(flags);

	set<string> keywords;

	server->server->getFolderKeywordInfo(n, keywords);
	serverIndex[n].setTag(keywords);

	for (i=0; i<sorted_index.size(); i++)
		if (sorted_index[i] == n)
		{
			if (currentDisplay)
				currentDisplay->draw(i);
			break;
		}

	if (mymessage)
	{
		mymessage->folderResorted();
		if (myMessage::FolderUpdateCallback::currentCallback)
			myMessage::FolderUpdateCallback::currentCallback
				->folderUpdated();
	}
}

void myFolder::saveSnapshot(string snapshotId)
{
	if (!server->server)
		return; // Sanity check

	string filename=server->getCachedFilename(this, "SNAPSHOT");
	string tmpfilename=filename + ".tmp";

	unlink(tmpfilename.c_str());

	ofstream o(tmpfilename.c_str());

	if (o.is_open())
	{
		size_t n=server->server->getFolderIndexSize();

		o << SNAPSHOTFORMAT ":" << n << ":" << snapshotId << endl;

		size_t i;

		for (i=0; i<n; i++)
			o << (string)server->server->getFolderIndexInfo(i)
			  << endl;

		for (i=0; i<n; i++)
		{
			set<string> keywSet;

			server->server->getFolderKeywordInfo(i, keywSet);

			set<string>::iterator b=keywSet.begin(),
				e=keywSet.end();
			bool hasKw=false;

			while (b != e)
			{
				if ( b->find('\n') == b->npos )
				{
					if (!hasKw)
						o << i << endl;
					hasKw=true;

					o << *b << endl;
				}
				++b;
			}

			if (hasKw)
				o << endl;
		}

		o << flush;

		o.close();

		if (!o.fail() && !o.bad() && rename(tmpfilename.c_str(),
						    filename.c_str()) == 0)
			return;
		unlink(tmpfilename.c_str());
	}
	statusBar->clearstatus();
	statusBar->status(Gettext(_("%1%: %2%")) << tmpfilename
			  << strerror(errno));
	statusBar->beepError();
}

/////////////////////////////////////////////////////////////////////
//
// The folder is opened
//

bool myFolder::init()
{
	vector<size_t> msgindex;

	size_t n=server->server->getFolderIndexSize();

	size_t i;

	sorted_index.resize(n);

	for (i=0; i<n; i++)
		sorted_index[i]=i;

	statusBar->clearstatus();
	statusBar->status(_("Reading headers..."));

	// See what we can grab from the cache.

	bool save_flag;

	serverIndex.clear();
	serverIndex.insert(serverIndex.end(), n, Index());

	{
		map<string, size_t> uid_list;

		for (i=0; i<n; i++)
		{
			mail::messageInfo flags=server->server->
				getFolderIndexInfo(i);

			Index &n=serverIndex[i];

			n.uid=flags.uid;
			n.setStatusCode(flags);

			{
				set<string> keywords;

				server->server->getFolderKeywordInfo(i,
								     keywords);
				n.setTag(keywords);
			}

			uid_list.insert(make_pair(server->server
						  ->getFolderIndexInfo(i)
						  .uid, i));
		}

		save_flag=loadFolderIndex(uid_list);

		map<string, size_t>::iterator
			b=uid_list.begin(), e=uid_list.end();

		while (b != e)
			msgindex.push_back( (*b++).second);
	}


	myServer::Callback callback;

	FolderIndexUpdate index_update(*this, callback);

	if (msgindex.size() > 0)
	{
		index_update.totalCount=msgindex.size();
		server->server->readMessageAttributes(msgindex,
						      mail::account::ENVELOPE |
						      mail::account::MESSAGESIZE |
						      mail::account::ARRIVALDATE,
						      index_update);

		if (!myServer::eventloop(callback))
			return false;

		save_flag=true;
	}

	statusBar->clearstatus();
	statusBar->status(Gettext(_("%1%: %2% messages"))
			  << folder->getName() << n);

	// Remember previous sort order.

	string sortBy=getServer()
		->getFolderConfiguration(getFolder(), "SORT");

	if (sortBy.size() > 0)
		setSortFunctionNoSort(sortBy);
	resort();

	currentMessage=0;
	for (i=0; i<n; i++)
		if (server->server->getFolderIndexInfo(sorted_index[i])
		    .unread)
		{
			currentMessage=i;
			break;
		}

	if (save_flag)
		getServer()->saveFolderIndex( this );


	myFolder::FolderFilter *filter=installFilter();

	if (filter)
	{
		statusBar->clearstatus();
		statusBar->status(_("Filtering messages..."));

		mail::ptr<myFolder> folderPtr=this;

		filter->filterStart=0;
		filter->filterEnd=getServer()->server
			->getFolderIndexSize();

		filter->run();
		if (folderPtr.isDestroyed() ||
		    getServer()->server == NULL)
			return true;
		// Server connection broke in mid-filtering.


		// If new mail arrived while filtering took place,
		// filter the remaining messages in the background.

		if (currentFilter && currentFilter->filterEnd
		    < getServer()->server->getFolderIndexSize()

		    && newMailCheckCount == 0)
			// newMailCheckCount > 0: we're still gathering
			// headers of new messages, this can be done later.
			// See myFolder::newMessagesReceived().
		{
			currentFilter->filterStart=currentFilter->filterEnd;
			currentFilter->filterEnd=getServer()->server
				->getFolderIndexSize();
			currentFilter->start();
		}
		else if (currentFilter)
			delete currentFilter;
	}
	return true;
}

bool myFolder::loadFolderIndex(map<string, size_t> &uid_list)
{
	string cachefile=getServer()
		->getFolderConfiguration(getFolder(), "INDEX");

	if (cachefile.size() == 0)
		return true;

	cachefile=myServer::getConfigDir() + "/" + cachefile;

	return myServer::loadFolderIndex(cachefile, uid_list, this);
}

void myFolder::setSortFunction(string sortBy)
{
	setSortFunctionNoSave(sortBy);

	myServer *s=getServer();

	s->updateFolderConfiguration(getFolder(),
				     "SORT", getSortFunction());
	s->saveFolderConfiguration();
}

void myFolder::setSortFunctionNoSave(string sortBy)
{
	// Try to keep the cursor on the same message, after resorting.

	size_t cm=currentMessage;

	if (cm < sorted_index.size())
		cm=sorted_index[cm];

	setSortFunctionNoSort(sortBy);
	resort();

	vector<size_t>::iterator p=
		find(sorted_index.begin(), sorted_index.end(), cm);

	currentMessage=p != sorted_index.end()
		? p - sorted_index.begin():0;

	if (mymessage)
		mymessage->folderResorted();

	if (currentDisplay)
		currentDisplay->draw();
}

void myFolder::setSortFunctionNoSort(string sortBy)
{
	sort_function_not=false;
	sort_function_name="";

	if (strncmp(sortBy.c_str(), "!", 1) == 0)
	{
		sort_function_not=true;
		sortBy=sortBy.substr(1);
		sort_function_name="!";
	}

	if (sortBy == "D")
		sort_function= &myFolder::sortByDate;
	else if (sortBy == "S")
		sort_function= &myFolder::sortBySubject;
	else if (sortBy == "N")
		sort_function= &myFolder::sortByName;
	else if (sortBy == SORT_THREAD_S)
		sort_function= &myFolder::sortByArrival; // Fixed up later.
	else
	{
		sortBy="A";

		sort_function= &myFolder::sortByArrival;
	}
	sort_function_name += sortBy;
}

string myFolder::getSortFunction() const
{
	return sort_function_name;
}

mail::messageInfo myFolder::getFlags(size_t n) const
{
	return server->server ?
		server->server->getFolderIndexInfo(sorted_index[n])
		: mail::messageInfo();
}

myServer *myFolder::getServer() const
{
	return (server);
}

////////////////////////////////////////////////////////////////////////////
//
// Navigation, used by CursesMessageDisplay

// Find next unread message

bool myFolder::getNextUnreadMessage(size_t &messageNum)
{
	messageNum=currentMessage;

	size_t n;

	for (n=messageNum+1; n<sorted_index.size(); n++)
		if (serverIndex[sorted_index[n]].status_code == 'N')
		{
			messageNum=n;
			return true;
		}

	for (n=0; n < messageNum && n < sorted_index.size(); n++)
		if (serverIndex[sorted_index[n]].status_code == 'N')
		{
			messageNum=n;
			return true;
		}

	return false;
}

// Find next message

bool myFolder::getNextMessage(size_t &messageNum)
{
	messageNum=currentMessage;

	if (currentMessage+1 < sorted_index.size())
	{
		++messageNum;
		return true;
	}

	return false;
}

// Find previous message

bool myFolder::getPrevMessage(size_t &messageNum)
{
	messageNum=currentMessage;

	if (currentMessage > 0)
	{
		messageNum--;
		return true;
	}

	return false;
}

// Shortcut to view attachment screen

void myFolder::viewAttachments(size_t messageNum)
{
	openMessage(messageNum, "", true, NULL);

}

//////////////////////////////////////////////////////////////////////////
//
// Download a draft.
//
// The current implementation just downloads the message, then parses it.

class OpenDraftCallback : public mail::callback::message,
			  public myServer::Callback {

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);

public:
	FILE *fp;
	struct rfc2045 *rfc2045p;

	OpenDraftCallback();
	~OpenDraftCallback();

	void messageTextCallback(size_t n, string text);
	void success(string message);
	void fail(string message);
};

OpenDraftCallback::OpenDraftCallback()
{
}

OpenDraftCallback::~OpenDraftCallback()
{
}

void OpenDraftCallback::reportProgress(size_t bytesCompleted,
				       size_t bytesEstimatedTotal,

				       size_t messagesCompleted,
				       size_t messagesEstimatedTotal)
{
	myServer::reportProgress(bytesCompleted,
				 bytesEstimatedTotal,
				 messagesCompleted,
				 messagesEstimatedTotal);
}

void OpenDraftCallback::messageTextCallback(size_t n, string text)
{
	if (fwrite(&*text.begin(), text.size(), 1, fp) != 1)
		; // Ignore gcc warning
	rfc2045_parse(rfc2045p, &*text.begin(), text.size());
}

void OpenDraftCallback::success(string message)
{
	myServer::Callback::success(message);
}

void OpenDraftCallback::fail(string message)
{
	myServer::Callback::fail(message);
}

static bool prepareDraft(FILE *, struct rfc2045 *);

void myFolder::goDraft()
{
	size_t messageNum=getCurrentMessage();

	messageNum=sorted_index[messageNum];

	if (getServer()->server == NULL)
		return;

	mail::ptr<mail::account> server=getServer()->server;

	statusBar->clearstatus();
	statusBar->status(Gettext(_("Downloading message from %1%..."))
			  << getFolder()->getName());

	OpenDraftCallback openDraft;

	if ((openDraft.rfc2045p=rfc2045_alloc()) == NULL ||
	    (openDraft.fp=tmpfile()) == NULL)
	{
		if (openDraft.rfc2045p)
			rfc2045_free(openDraft.rfc2045p);
		statusBar->status(strerror(errno), statusBar->SYSERROR);
		statusBar->beepError();
		return;
	}

	mail::messageInfo oldInfo=server->getFolderIndexInfo(messageNum);

	try {
		vector<size_t> messageVec;

		messageVec.push_back(messageNum);

		server->readMessageContent(messageVec, false,
					   mail::readBoth, openDraft);

		if (!myServer::eventloop(openDraft))
		{
			fclose(openDraft.fp);
			rfc2045_free(openDraft.rfc2045p);
			return;
		}

		statusBar->clearstatus();
		statusBar->status(_("Extracting MIME attachments..."));

		if (fflush(openDraft.fp) < 0 || ferror(openDraft.fp) < 0 ||
		    !prepareDraft(openDraft.fp, openDraft.rfc2045p))
		{
			return;
		}

	} catch (...) {
		fclose(openDraft.fp);
		rfc2045_free(openDraft.rfc2045p);
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
	fclose(openDraft.fp);
	rfc2045_free(openDraft.rfc2045p);

	if (server.isDestroyed())
		return;

	// After we have the message, remove it from the Draft folder.

	if (server->getFolderIndexSize() > messageNum &&
	    server->getFolderIndexInfo(messageNum).uid == oldInfo.uid)
	{
		try
		{
			isExpungingDrafts=true;

			vector<size_t> msgList;

			msgList.push_back(messageNum);

			myServer::Callback removeCallback;

			server->removeMessages(msgList, removeCallback);

			myServer::eventloop(removeCallback);

			isExpungingDrafts=false;
		} catch (...) {
			isExpungingDrafts=false;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}
	}

	Curses::keepgoing=false;
        myServer::nextScreen= &editScreen;
        myServer::nextScreenArg=NULL;
}

static bool copyTo(FILE *, string, string, off_t, off_t, struct rfc2045 *,
		   size_t, size_t);
static bool copyTo(FILE *, ostream &, off_t, off_t, struct rfc2045 *,
		   mail::autodecoder *d, size_t, size_t);

// Extract the main body of the message, and its attachments.

static bool prepareDraft(FILE *fp, struct rfc2045 *rfcp)
{
	struct rfc2045 *p;

	string messagetmp=myServer::getConfigDir() + "/message.tmp";
	string messagetxt=myServer::getConfigDir() + "/message.txt";

	if (rfcp->firstpart == NULL)
	{
		off_t start_pos, end_pos, start_body, dummy;

		rfc2045_mimepos(rfcp, &start_pos, &end_pos,
				&start_body, &dummy, &dummy);

		return copyTo(fp, messagetmp, messagetxt,
			      start_pos, start_body, rfcp, 0, 1);
	}

	struct rfc2045 *firstSect=NULL;

	size_t done=0;
	size_t estTotal=0;

	for (p=rfcp->firstpart; p; p=p->next)
	{
		if (p->isdummy)
			continue;

		++estTotal;
	}

	size_t cnt=0;

	for (p=rfcp->firstpart; p; p=p->next)
	{
		if (p->isdummy)
			continue;

		if (!firstSect)
		{
			firstSect=p;
			continue;
		}

		off_t start_pos, end_pos, dummy;

		rfc2045_mimepos(p, &start_pos, &end_pos,
				&dummy, &dummy, &dummy);

		string tmpname, attname;

		string buffer;

		{
			ostringstream o;

			o << "-" << setw(4) << setfill('0') << cnt++;
			buffer=o.str();
		}

		myMessage::createAttFilename(tmpname, attname, buffer);

		if (!copyTo(fp, tmpname, attname, start_pos, end_pos, NULL,
			    done, estTotal))
			return false;

		++done;
	}

	off_t start_pos, end_pos, start_body, dummy;

	rfc2045_mimepos(rfcp, &start_pos, &end_pos, &start_body,
			&dummy, &dummy);

	ofstream o(messagetmp.c_str());

	if (fseek(fp, start_pos, SEEK_SET) < 0)
	{
		statusBar->status(strerror(errno), statusBar->SYSERROR);
		statusBar->beepError();
		return false;
	}

	char lastCh='\n';

	while (start_pos < start_body)
	{
		int ch=getc(fp);

		if (ch == EOF)
			break;
		++start_pos;

		if (ch == '\r')
			continue;

		if (ch == '\n' && lastCh == '\n')
			break;
		o << (char)ch;

		lastCh=(char)ch;
	}

	if (lastCh != '\n')
		o << '\n';

	clearerr(fp);

	if (firstSect)
	{
		rfc2045_mimepos(firstSect, &start_pos, &end_pos, &start_body,
				&dummy, &dummy);
		if (!copyTo(fp, o, start_pos, start_body, firstSect, NULL,
			    done, estTotal))
			return false;
	}

	o.flush();
	if (o.bad() || o.fail() ||
	    (o.close(), rename(messagetmp.c_str(), messagetxt.c_str())) < 0)
	{
		statusBar->status(strerror(errno), statusBar->SYSERROR);
		statusBar->beepError();
		return false;
	}
	return true;
}

static bool copyTo(FILE *fp, string tmpname, string txtname,
		   off_t from, off_t to, struct rfc2045 *contentPart,
		   size_t messagesCompleted,
		   size_t messagesEstimatedTotal)
{
	ofstream o(tmpname.c_str());

	if (!copyTo(fp, o, from, to, contentPart, NULL,
		    messagesCompleted,
		    messagesEstimatedTotal) ||
	    (o.flush(), o.bad()) || o.fail() ||
	    (o.close(), rename(tmpname.c_str(), txtname.c_str())) < 0)
	{
		statusBar->status(strerror(errno), statusBar->SYSERROR);
		statusBar->beepError();
		return false;
	}
	return true;
}

class myFolderWriteDecoded : public mail::autodecoder {

	ostream &o;

public:
	myFolderWriteDecoded(ostream &oArg, string cte);
	~myFolderWriteDecoded();

	void decoded(string);
};

myFolderWriteDecoded::myFolderWriteDecoded(ostream &oArg, string cte)
	: mail::autodecoder(cte), o(oArg)
{
}

myFolderWriteDecoded::~myFolderWriteDecoded()
{
}

void myFolderWriteDecoded::decoded(string s)
{
	o << s;
}

static bool copyTo(FILE *fp, ostream &o, off_t from, off_t to,
		   struct rfc2045 *contentPart,
		   mail::autodecoder *decoderArg,
		   size_t messagesCompleted,
		   size_t messagesEstimatedTotal)
{
	if (fseek(fp, from, SEEK_SET) >= 0)
	{
		char buffer[BUFSIZ];
		off_t startedPos=from;

		int n;

		while (from < to)
		{
			n=sizeof(buffer);

			if (to - from < n)
				n=to-from;

			errno=EIO;
			n=fread(buffer, 1, n, fp);

			if (n <= 0)
			{
				statusBar->status(strerror(errno),
						  statusBar->SYSERROR);
				statusBar->beepError();
				return false;
			}

			string ss(buffer, buffer+n);

			if (decoderArg)
				decoderArg->decode(ss);
			else
				o << ss;

			from += n;
			myServer::reportProgress(from - startedPos,
						 to - startedPos,
						 messagesCompleted,
						 messagesEstimatedTotal);

			if (o.bad() || o.fail())
			{
				statusBar->status(strerror(errno),
						  statusBar->SYSERROR);
				statusBar->beepError();
				return false;
			}
		}

		if (contentPart)
		{
			const char *content_type;
                        const char *content_transfer_encoding;
                        const char *charset;

                        rfc2045_mimeinfo(contentPart, &content_type,
                                         &content_transfer_encoding,
                                         &charset);

			off_t start_pos, end_pos, start_body, dummy;

			rfc2045_mimepos(contentPart, &start_pos, &end_pos,
					&start_body, &dummy, &dummy);

			myFolderWriteDecoded
				decoder(o, content_transfer_encoding);

			return (copyTo(fp, o, start_body, end_pos, NULL,
				       &decoder,
				       messagesCompleted,
				       messagesEstimatedTotal));
		}
		return true;
	}

	statusBar->status(strerror(errno), statusBar->SYSERROR);
	statusBar->beepError();
	return false;
}

// Open the indicated message

void myFolder::goMessage(size_t messageNum,
			 string mimeid,
			 void (myMessage::*completedFuncArg)())
{
	openMessage(messageNum, mimeid, false, completedFuncArg);
}

void myFolder::openMessage(size_t messageNum,
			   string mimeid,
			   bool attachmentsOnly,
			   void (myMessage::*completedFuncArg)())
{
	if (messageNum < size())
		currentMessage=messageNum;

	// If there's an error, fall back to the folder index screen

	Curses::keepgoing=false;
	myServer::nextScreen= &folderIndexScreen;
	myServer::nextScreenArg=this;

	// If we already have this message loaded, save the trouble of
	// loading it again.

	if (mymessage == NULL || mymessage->messagesortednum != messageNum
	    || (!attachmentsOnly && !mymessage->isShownMimeId(mimeid)))
	{
		statusBar->clearstatus();
		statusBar->status(_("Reading message..."),
				  statusBar->INPROGRESS);

		CursesMessage *cm=new CursesMessage(getServer()->server,
						    completedFuncArg);

		if (!cm)
			outofmemory();

		try {
			mail::ptr<CursesMessage> cmptr(cm);

			cm->myfolder=this;
			cm->messagesortednum=messageNum;

			messageNum=getServerIndex(messageNum);
			cm->messagenum=messageNum;

			myServer::nextScreen=NULL;

			if (!cm->readAttributes() ||
			    (!attachmentsOnly && !cm->init(mimeid, false)) ||
			    cmptr.isDestroyed())
			{
				if (!cmptr.isDestroyed())
				{
					cm->myfolder=NULL;
					delete cm;
				}

				Curses::keepgoing=false;

				if (myServer::nextScreen == NULL)
					// If we got disconnected while
					// opening the message, don't mess
					// with nextScreen
				{
					myServer::nextScreen=
						&folderIndexScreen;
					myServer::nextScreenArg=this;
				}
				return;
			}

			mymessage=cm;
		} catch (...) {
			delete cm;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}

		if (completedFuncArg)
			cm->beginReformat(80);
	}
	else if (completedFuncArg)
		(mymessage->*completedFuncArg)();

	if (completedFuncArg)
		return;

	Curses::keepgoing=false;
	myServer::nextScreen= &folderIndexScreen;
	myServer::nextScreenArg=this;

	if (mymessage != NULL) // Expected
	{
		if (attachmentsOnly)
		{
			statusBar->clearstatus();
			myServer::nextScreen= &showAttachments;
		}
		else
		{
			myServer::nextScreen= &showCurrentMessage;
		}
		myServer::nextScreenArg=mymessage;
	}
}

////////////////////////////////////////////////////////////////////////////

myFolder::FolderIndexUpdate::FolderIndexUpdate(myFolder &folderArg,
					       mail::callback &callbackArg)
	: folder(&folderArg), successOrFail(callbackArg),
	  totalCount(0), doneCount(0)
{
}

myFolder::FolderIndexUpdate::~FolderIndexUpdate()
{
}

void myFolder::FolderIndexUpdate::success(string errmsg)
{
	successOrFail.success(errmsg);
}

void myFolder::FolderIndexUpdate::fail(string errmsg)
{
	successOrFail.fail(errmsg);
}

void myFolder::FolderIndexUpdate::reportProgress(size_t bytesCompleted,
						 size_t bytesEstimatedTotal,

						 size_t messagesCompleted,
						 size_t messagesEstimatedTotal)
{
	myServer::reportProgress(0, 0,
				 messagesCompleted,
				 messagesEstimatedTotal);
}

void myFolder::FolderIndexUpdate
::messageEnvelopeCallback(size_t messageNumber,
			  const class mail::envelope
			  &envelope)
{
	if (folder.isDestroyed())
		return;

	if (messageNumber < folder->serverIndex.size())
	{
		Index &i=folder->serverIndex[messageNumber];

		// Save headers of message in the folder index.

		i.messageid=messageId(folder->msgIds, envelope.messageid);
		i.messageDate=envelope.date;

		if (envelope.references.size() > 0)
			messageReferencesCallback(messageNumber,
						  envelope.references);

		if (i.references.size() == 0)
		{
			// Try to find something in in-reply-to

			string s=envelope.inreplyto;

			size_t n=s.find('<');

			if (n != std::string::npos)
				s=s.substr(0, n-1);

			n=s.find('>');

			if (n != std::string::npos)
				s=s.substr(n+1);

			vector<string> v;

			v.push_back(s);

			messageReferencesCallback(messageNumber, v);
		}

		mail::rfc2047::decoder header_decode;

		i.subject_utf8=header_decode.decode(envelope.subject,
						    "utf-8");

		bool err=false;

		{
			vector<unicode_char> uc;

			if (!mail::iconvert::convert(i.subject_utf8,
						     "utf-8", uc))
				err=true;
		}

		if (err)
			i.subject_utf8=Gettext(_("ERROR: "))
				<< strerror(errno);

		string::iterator bs=i.subject_utf8.begin(),
			es=i.subject_utf8.end();

		while (bs != es)
		{
			if ( (int)(unsigned char)*bs < ' ')
				*bs=' '; // Zap control chars
			bs++;
		}

		i.name_utf8="";

		vector<mail::address>::const_iterator b, e;

		b=envelope.from.begin();
		e=envelope.from.end();

		mail::emailAddress fallback_addr;
		bool has_fallback_addr=false;

		string fallback_pfix;

		// NetNews posts only have a From: header.  If it's my post,
		// don't leave the summary blank.

		while (b != e)
		{
			mail::emailAddress a(*b++);

			if (myServer::isMyAddress(a))
			{
				fallback_addr=a;
				has_fallback_addr=true;
				continue;
			}

			if (i.name_utf8.size() == 0)
			{
				i.name_utf8=a.getDisplayName("utf-8");
				if (i.name_utf8.size() == 0)
					i.name_utf8=a.getDisplayAddr("utf-8");
			}
		}

		b=envelope.to.begin();
		e=envelope.to.end();

		const char *addr_pfix=_("To: ");

		while (b != e)
		{
			mail::emailAddress a(*b++);

			if (myServer::isMyAddress(a))
			{
				fallback_addr=a;
				has_fallback_addr=true;
				fallback_pfix=addr_pfix;
				continue;
			}

			if (i.name_utf8.size() == 0)
			{
				i.name_utf8=a.getDisplayName("utf-8");
				if (i.name_utf8.size() == 0)
					i.name_utf8=a.getDisplayAddr("utf-8");

				if (i.name_utf8.size() > 0)
					i.name_utf8=addr_pfix + i.name_utf8;
			}
		}

		b=envelope.cc.begin();
		e=envelope.cc.end();

		addr_pfix=_("Cc: ");

		while (b != e)
		{
			mail::emailAddress a(*b++);

			if (myServer::isMyAddress(a))
			{
				fallback_addr=a;
				has_fallback_addr=true;
				fallback_pfix=addr_pfix;
				continue;
			}

			if (i.name_utf8.size() == 0)
			{
				i.name_utf8=a.getDisplayName("utf-8");
				if (i.name_utf8.size() == 0)
					i.name_utf8=a.getDisplayAddr("utf-8");
				if (i.name_utf8.size() > 0)
					i.name_utf8=addr_pfix + i.name_utf8;
			}
		}

		if (i.name_utf8.size() == 0 && has_fallback_addr)
		{
			i.name_utf8= fallback_addr.getDisplayName("utf-8");
			if (i.name_utf8.size() == 0)
				i.name_utf8=fallback_addr.getDisplayAddr("utf-8");

			if (i.name_utf8.size() > 0)
				i.name_utf8=fallback_pfix + i.name_utf8;
		}

		bs=i.name_utf8.begin();
		es=i.name_utf8.end();

		while (bs != es)
		{
			if ( (int)(unsigned char)*bs < ' ')
				*bs=' '; // Zap control chars
			bs++;
		}

		i.toupper();
		i.checkwatch(*folder);
	}

	if (++doneCount > totalCount*3)
		totalCount=doneCount / 3;
	reportProgress(0, 0, doneCount / 3, totalCount);
}

void myFolder::FolderIndexUpdate
::messageReferencesCallback(size_t messageNumber,
			    const vector<string> &references)
{
	if (folder.isDestroyed())
		return;

	if (messageNumber >= folder->serverIndex.size())
		return;

	Index &i=folder->serverIndex[messageNumber];

	i.references.clear();
	i.references.reserve(references.size());
	i.watchLevel=0;
	i.expires=0;

	if (references.size() > 0)
	{
		vector<string>::const_iterator rb=references.begin(),
			re=references.end();

		while (rb != re)
		{
			string s=*rb;

			if (s.size() > 0 && s[0] == '<')
				s=s.substr(1);

			if (s.size() > 0 && s[s.size()-1] == '>')
				s=s.substr(0, s.size()-1);

			messageId mid(folder->msgIds, s);
			i.references.push_back(mid);

			if (!folder->watchList.watching(messageId(folder
								  ->msgIds,
								  *rb),
							i.expires,
							i.watchLevel))
				if (i.watchLevel)
				{
					if (--i.watchLevel == 0)
						i.expires=0;
				}
			++rb;
		}
	}

	if (((string)i.messageid).size() > 0) // Already seen env
	{
		if (i.watchLevel)
		{
			--i.watchLevel;
			folder->watchList(i.messageid, i.expires,
					  i.watchLevel);
		}
		i.checkwatch(*folder);
	}
}

void myFolder::FolderIndexUpdate
::messageArrivalDateCallback(size_t messageNumber,
			     time_t datetime)
{
	if (folder.isDestroyed())
		return;

	if (messageNumber < folder->serverIndex.size())
		folder->serverIndex[messageNumber].arrivalDate=datetime;

	if (++doneCount > totalCount*3)
		totalCount=doneCount / 3;
	reportProgress(0, 0, doneCount / 3, totalCount);
}

void myFolder::FolderIndexUpdate
::messageSizeCallback(size_t messageNumber,
		      unsigned long size)
{
	if (folder.isDestroyed())
		return;

	if (messageNumber < folder->serverIndex.size())
		folder->serverIndex[messageNumber].messageSize=size;

	if (++doneCount > totalCount*3)
		totalCount=doneCount / 3;
	reportProgress(0, 0, doneCount / 3, totalCount);
}

////////////////////////////////////////////////////////////////////////////

myFolder::Index::Index() : arrivalDate(0), messageDate(0), messageSize(0),
			   status_code(' '),
			   tag(0), threadLevel(0), watchLevel(0),
			   expires(0)
{
}

myFolder::Index::~Index()
{
}

// Pre-convert sort fields to uppercase

void myFolder::Index::toupper()
{
	upperSubject_utf8=mail::iconvert::convert_tocase(subject_utf8,
							 "utf-8",
							 unicode_uc);
	upperName_utf8=mail::iconvert::convert_tocase(name_utf8,
						      "utf-8",
						      unicode_uc);
}

void myFolder::Index::checkwatch(myFolder &f)
{
	if (watchLevel == 0)
		f.watchList.watching(messageid, expires, watchLevel);
}

void myFolder::Index::setStatusCode(const class mail::messageInfo &flags)
{
	status_code=flags.deleted ? 'D':
		flags.unread ? 'N':
		flags.replied ? 'R':' ';
}

void myFolder::Index::setTag(set<string> &keywords)
{
	set<string>::iterator b=keywords.begin(), e=keywords.end();

	tag=0;

	while (b != e)
	{
		size_t n;

		if (Tags::tags.isTagName(*b, n) && n > tag)
			tag=n;
		++b;
	}
}

////////////////////////////////////////////////////////////////////////////

myFolder::NewMailUpdate::NewMailUpdate() : update_struct(NULL)
{
}

myFolder::NewMailUpdate::~NewMailUpdate()
{
}

void myFolder::NewMailUpdate::reportProgress(size_t bytesCompleted,
					     size_t bytesEstimatedTotal,

					     size_t messagesCompleted,
					     size_t messagesEstimatedTotal)
{
	myServer::reportProgress(bytesCompleted,
				 bytesEstimatedTotal,
				 messagesCompleted,
				 messagesEstimatedTotal);
}

void myFolder::NewMailUpdate::success(string message)
{
	if (update_struct)
	{
		if (!update_struct->folder.isDestroyed())
			update_struct->folder->newMessagesReceived();
		delete update_struct;
	}
	delete this;
}

void myFolder::NewMailUpdate::fail(string message)
{
	if (update_struct)
	{
		if (!update_struct->folder.isDestroyed())
			update_struct->folder->newMessagesReceived();
		delete update_struct;
	}
	delete this;
}

void myFolder::checkNewMail(mail::callback &cb)
{
	if (server->server == NULL)
	{
		cb.success("OK");
		return;
	}

	server->server->checkNewMail(cb);
}

bool myFolder::IndexSort::operator()(size_t a, size_t b)
{
	if (f.sort_function_not)
	{
		size_t c=a;

		a=b;
		b=c;
	}

	return (f.*(f.sort_function))(a, b);
}

bool myFolder::sortByArrival(size_t a, size_t b)
{
	return a < b;
}

bool myFolder::sortByDate(size_t a, size_t b)
{
	time_t ad=serverIndex[a].messageDate;
	time_t bd=serverIndex[b].messageDate;

	if (ad == 0)
		ad=serverIndex[a].arrivalDate;
	if (bd == 0)
		bd=serverIndex[b].arrivalDate;

	if (ad == 0 || bd == 0)
		return a < b;

	return ad < bd;
}

bool myFolder::sortBySubject(size_t a, size_t b)
{
	string sa=serverIndex[a].upperSubject_utf8;
	string sb=serverIndex[b].upperSubject_utf8;

	int arefwd, brefwd;

	char *as=rfc822_coresubj_nouc(sa.c_str(), &arefwd);

	if (!as)
		outofmemory();

	try {
		sa=as;
		free(as);
	} catch (...) {
		free(as);
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	char *bs=rfc822_coresubj_nouc(sb.c_str(), &brefwd);

	if (!bs)
		outofmemory();

	try {
		sb=bs;
		free(bs);
	} catch (...) {
		free(bs);
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	if (sa == sb)
	{
		if (arefwd == brefwd)
			return sortByDate(a, b);

		return (arefwd < brefwd);
	}

	return sa < sb;
}

bool myFolder::sortByName(size_t a, size_t b)
{
	string sa=serverIndex[a].upperName_utf8;
	string sb=serverIndex[b].upperName_utf8;

	if (sa == sb)
		return sortByDate(a, b);

	return sa < sb;
}

//////////////////////////////////////////////////////////////////////////
//
// Interface for displaying the folder index on the screen
//

myFolder::IndexDisplay::IndexDisplay(myFolder *fArg)
	: f(fArg)
{
	f->currentDisplay=this;
}

myFolder::IndexDisplay::~IndexDisplay()
{
	if (f)
		f->currentDisplay=NULL;
}

//////////////////////////////////////////////////////////////////////////////

myFolder::DelUndelCallback::DelUndelCallback()
{
}

myFolder::DelUndelCallback::~DelUndelCallback()
{
}

void myFolder::DelUndelCallback::success(string message)
{
	statusBar->status(message, statusBar->NORMAL);
	delete this;
}

void myFolder::DelUndelCallback::fail(string message)
{
	statusBar->status(message, statusBar->SERVERERROR);
	statusBar->beepError();
	delete this;
}

void myFolder::DelUndelCallback::reportProgress(size_t bytesCompleted,
						size_t bytesEstimatedTotal,

						size_t messagesCompleted,
						size_t messagesEstimatedTotal)
{
	myServer::reportProgress(bytesCompleted,
				 bytesEstimatedTotal,
				 messagesCompleted,
				 messagesEstimatedTotal);
}

//
// Re-sort sorted_index according to the current sort order
//

void myFolder::resort()
{
	if (sort_function_name.find(SORT_THREAD) != std::string::npos)
	{
		thread::resort(*this);
		return;
	}

	vector<myFolder::Index>::iterator b=serverIndex.begin(),
		e=serverIndex.end();

	while (b != e)
	{
		b->threadLevel=0;
		b->active_threads.clear();
		b++;
	}

	sort(sorted_index.begin(), sorted_index.end(), IndexSort(*this));
}

/////////////////////////////////////////////////////////////////////////////
//
// Threading implementation

#include "rfc822/imaprefs.h"

void myFolder::thread::resort(myFolder &f)
{
	struct imap_refmsgtable *t=rfc822_threadalloc();

	if (!t)
		LIBMAIL_THROW((strerror(errno)));

	try {
		resort(f, t);
		rfc822_threadfree(t);
	} catch (...) {
		rfc822_threadfree(t);
		throw;
	}
}

void myFolder::thread::resort(myFolder &f, struct imap_refmsgtable *t)
{
	vector<myFolder::Index>::iterator ib, ie;

	size_t n=0;

	size_t cnt=f.sorted_index.size();

	for (ib=f.serverIndex.begin(), ie=ib+cnt; ib != ie; ib++)
	{
		vector<const char *> refArray;

		refArray.reserve(ib->references.size()+1);

		vector<messageId>::iterator b=ib->references.begin(),
			e=ib->references.end();

		while (b != e)
		{
			refArray.push_back( b->c_str() );
			++b;
		}

		refArray.push_back(0);

		if (!rfc822_threadmsgrefs(t, ib->messageid.c_str(),
					  &refArray[0],
					  ib->subject_utf8.c_str(),
					  NULL, ib->messageDate, n++))
			LIBMAIL_THROW((strerror(errno)));
	}

	struct imap_refmsg *root=rfc822_thread(t);

	if (!root)
		LIBMAIL_THROW((strerror(errno)));

	vector<size_t> active_threads;
	size_t threadLevel=0;

	struct imap_refmsg *lastMsg=NULL;

	f.sorted_index.clear();
	f.sorted_index.reserve(cnt);

	resort_layout(f, root, lastMsg, active_threads, threadLevel);

	if (lastMsg)
		resort_layout(f, lastMsg, active_threads, threadLevel, false);
}

// Unwrap thread tree into flat layout


// Visit all messages at the same thread level.  The thread tree may have
// dummy entries, we can't just visit each node just like that.
// If this function receives a dummy node, it recursively invokes itself
// for all of its children nodes.  Otherwise:

//  We may need to do some special processing for the last visited message
// (namely, pop off the last active thread).  Therefore, do not visit each
// node right away.  Instead, keep a "last seen" message pointer, which is
// initialized to NULL.  When this function visits this node, it sets the
// last visited ptr to nodePtr.  If last visited ptr was not null, the
// last visited ptr is officially visited.  On exited, last visited ptr
// contains a ptr to the last message at the same thread level, so it can
// be properly serviced.


#define MAXTHREADLEVEL 10

	// Beyond the 10th thread level, there's not much point in
	// keeping the score.


void myFolder::thread::resort_layout(myFolder &f,
				     struct imap_refmsg *nodePtr,
				     struct imap_refmsg *&lastPtr,

				     vector<size_t> &active_threads,
				     size_t threadLevel)
{
	if (!nodePtr->isdummy)
	{
		if (lastPtr)
			resort_layout(f, lastPtr, active_threads, threadLevel,
				      false);
			// Visit the last seen node, officially.
		lastPtr=nodePtr;

		if (threadLevel < MAXTHREADLEVEL)
			return;
	}

	for (nodePtr=nodePtr->firstchild; nodePtr;
	     nodePtr=nodePtr->nextsib)
		resort_layout(f, nodePtr, lastPtr, active_threads,
			      threadLevel);
}

void myFolder::thread::resort_layout(myFolder &f,
				     struct imap_refmsg *node,
				     vector<size_t> &active_threads,
				     size_t threadLevel,
				     bool popLast)
{
	myFolder::Index &i=f.serverIndex[node->seqnum];

#if 0
	cerr << "Thread: msg " << f.sorted_index.size() // << node->seqnum
	     << "(" << i.subject_utf8 << "), level " << threadLevel << ":";

	{
		vector<size_t>::iterator a=active_threads.begin(),
			b=active_threads.end();

		while (a != b)
			cerr << " " << *a++;
		cerr << endl << flush;
	}
#endif

	i.threadLevel=threadLevel;
	i.active_threads=active_threads;

	f.sorted_index.push_back(node->seqnum);

	if (popLast)
		active_threads.pop_back();

	active_threads.push_back(threadLevel);

	struct imap_refmsg *last=NULL;

	if (threadLevel < MAXTHREADLEVEL)
		for (node=node->firstchild; node; node=node->nextsib)
			resort_layout(f, node, last, active_threads,
				      threadLevel+1);

	if (last)
		resort_layout(f, last, active_threads, threadLevel+1, true);
	else
		active_threads.pop_back();
}
