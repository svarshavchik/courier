/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef myfolder_H
#define myfolder_H

#include "config.h"

#include "previousscreen.H"
#include "myreferences.H"
#include "libmail/mail.H"
#include "libmail/snapshot.H"
#include "curses/timer.H"

#include <string>
#include <vector>
#include <map>
#include <fstream>

#include "unicode/unicode.h"

#include "libmail/objectmonitor.H"

//////////////////////////////////////////////////////////////////////////
//
// This object represents the index of the currently open folder.
//
// It's set up to receive folder events from libmail.a, as a subclass of
// mail::callback::folder.

class myServer;
class CursesMessage;
class myMessage;

class myFolder : public mail::callback::folder,
		 public mail::obj,
		 public PreviousScreen {

	mail::folder *folder;		// The folder that's opened
	class myServer *server;		// My server

	// A minimum subclass of mail::callback, used when marking/unmarking
	// messages.

	class DelUndelCallback : public mail::callback {

		void reportProgress(size_t bytesCompleted,
				    size_t bytesEstimatedTotal,

				    size_t messagesCompleted,
				    size_t messagesEstimatedTotal);

	public:
		DelUndelCallback();
		~DelUndelCallback();
		void success(std::string);
		void fail(std::string);
	};

public:
	bool isClosing; // The folder is being closed.

	void quiesce(); // Wait until all pending stuff is done.


	// Helper class for restoring previously saved folder index snapshots.
	// With mail account classes that support snaphots, the folder's index
	// is dumped to a temporary file and is restored the next time the
	// folder is opened, then the server sends any changes to the folder's
	// saved contents to bring the current contents up to date.
	//
	// See LibMAIL documentation for more information.


	class RestoreSnapshot : public mail::snapshot {
		std::ifstream i;
		std::string snapshotId;
		size_t nmessages;
		std::string cachefile;

	public:
		RestoreSnapshot(myFolder *);
		~RestoreSnapshot();

		void getSnapshotInfo(std::string &snapshotId,
				     size_t &nMessages);
		void restoreSnapshot(mail::snapshot::restore &);
	};

	size_t saveFirstRowShown; // Exit/reenter index screen

	bool isExpungingDrafts; // Don't announce expunges.
	bool mustReopen;
	// This folder may not really be open on the server any more, due to
	// an error when trying to open another folder.

	class Index {
	public:
		std::string uid;

		time_t arrivalDate;
		time_t messageDate;

		unsigned long messageSize;
		char	status_code;
		size_t tag;
		std::string subject_utf8;
		std::string name_utf8;

		std::string upperSubject_utf8;	// In uppercase
		std::string upperName_utf8;

		messageId messageid;
		std::vector<messageId> references;

		// For a threaded display only:

		size_t threadLevel;
		std::vector<size_t> active_threads; // Vertical bars

		// Watch level:

		size_t watchLevel;
		time_t expires;
		Index();
		~Index();
		void toupper();
		void checkwatch(myFolder &);
		void setStatusCode(const class mail::messageInfo &flags);
		void setTag(std::set<std::string> &keywords);
	};

	// Predicate for std::sort()
	class IndexSort {
		myFolder &f;
	public:
		IndexSort(myFolder &F) : f(F) {}
		bool operator()(size_t, size_t);
	};

	class FolderFilter;

	FolderFilter *currentFilter;

	FolderFilter *installFilter();
private:


	myMessageIds msgIds; // All message IDs in use.
	Watch watchList; // watched message IDs (declaration order IMPORTANT!)

	std::vector<Index> serverIndex;	// The index, in server order

	std::vector<size_t> sorted_index; // The index, in sorted order
	size_t currentMessage; // current message in the sorted index

	class FolderIndexUpdate;
	class NewMailUpdate;
public:
	CursesMessage *mymessage; // Currently open message, if any.


	// Interface for displaying the folder's index.  The index is not
	// always displayed, only when the folder index screen is actually
	// shown.  The folder index screen is subclasses from IndexDisplay,
	// whose constructor initilized the currentDisplay pointer below,
	// and the destructor NULLs it out.

	class IndexDisplay {
	public:
		myFolder *f;

		IndexDisplay(myFolder *);
		virtual ~IndexDisplay();
		virtual void draw()=0;
		virtual void draw(size_t)=0;
		myFolder *getFolderIndex() const { return f; }
	};

private:
	IndexDisplay *currentDisplay;

	size_t expunge_count; // # of messages expunges during current cmd.

public:
	// Mark msg deleted/undeleted.

	void markDeleted(size_t n, bool isDeleted, // Deleted/undeleting
			 bool setStatusFlag); // True: update status bar

	void toggleMark(size_t n); // Toggle mark/unmarked flag.

	void setTag(size_t n, size_t tag);
	void setTag(std::vector<size_t> &v, size_t tag);

	void watch(size_t, unsigned, unsigned); // Watch msg
	void unwatch(size_t); // Unwatch msg

	// Internal functions:

	void setWatch(size_t n, unsigned nDays, unsigned nLevels);
	void setUnwatch(size_t n);

private:
	void watchUpdated();

public:

	void checkExpunged(); // Check for expunged messages.

	friend class FolderIndexUpdate;
	friend class IndexDisplay;
	friend class myServer;

	myFolder(myServer *serverArg, const mail::folder *folderArg);
	~myFolder();

	bool init();

private:
	// Try to find a saved snapshot, and restore it.
	bool loadFolderIndex(std::map<std::string, size_t> &uid_list);
public:

	size_t size() const { return sorted_index.size(); }
	size_t getCurrentMessage() const { return currentMessage; }
	void setCurrentMessage(size_t n) { currentMessage=n; }

	class iterator {

		size_t n;

		myFolder *f;

	public:
		iterator(size_t N, myFolder *F) : n(N), f(F)
		{
		}

		iterator(const myFolder::iterator &o) : n(o.n), f(o.f)
		{
		}

		iterator &operator++()
		{
			++n;
			return *this;
		}

		iterator operator++(int foo)
		{
			iterator cpy(*this);

			++n;
			return cpy;
		}

		iterator &operator--()
		{
			--n;
			return *this;
		}

		iterator operator--(int foo)
		{
			iterator cpy(*this);

			--n;
			return cpy;
		}

		iterator operator+(int delta)
		{
			iterator cpy(*this);

			cpy.n += delta;

			return cpy;
		}

		Index &operator*()
		{
			return (f->serverIndex[f->sorted_index[n]]);
		}

		Index *operator->()
		{
			return (&f->serverIndex[f->sorted_index[n]]);
		}

		bool operator==(iterator &i)
		{
			return n == i.n;
		}

		bool operator!=(iterator &i)
		{
			return n != i.n;
		}

	};

	class const_iterator {

		size_t n;

		const myFolder *f;

	public:
		const_iterator(size_t N, const myFolder *F) : n(N), f(F)
		{
		}

		const_iterator(const myFolder::const_iterator &o) : n(o.n), f(o.f)
		{
		}

		const_iterator &operator++()
		{
			++n;
			return *this;
		}

		const_iterator operator++(int foo)
		{
			const_iterator cpy(*this);

			++n;
			return cpy;
		}

		const_iterator operator+(int delta)
		{
			const_iterator cpy(*this);

			cpy.n += delta;

			return cpy;
		}

		const_iterator &operator--()
		{
			--n;
			return *this;
		}

		const_iterator operator--(int foo)
		{
			const_iterator cpy(*this);

			--n;
			return cpy;
		}

		const Index &operator*()
		{
			return (f->serverIndex[f->sorted_index[n]]);
		}

		const Index *operator->()
		{
			return (&f->serverIndex[f->sorted_index[n]]);
		}

		bool operator==(const_iterator &i)
		{
			return n == i.n;
		}

		bool operator!=(const_iterator &i)
		{
			return n != i.n;
		}
	};

	iterator begin() { return iterator(0, this); }
	iterator end() { return iterator(sorted_index.size(), this); }

	const_iterator begin() const
	{
		return const_iterator(0, this);
	}

	const_iterator end() const
	{
		return const_iterator(sorted_index.size(), this);
	}

	const Index &getIndex(size_t n) const
	{
		return serverIndex[sorted_index[n]];
	}

	Index &getIndex(size_t n)
	{
		return serverIndex[sorted_index[n]];
	}

	mail::messageInfo getFlags(size_t n) const;

	size_t getServerIndex(size_t n) const { return sorted_index[n]; }

	class myServer *getServer() const;
	const mail::folder *getFolder() const { return (folder); }

	// Inherited from mail::callback::folder:

	void newMessages();
	void messagesRemoved(std::vector< std::pair<size_t, size_t> > &list);
	void messageChanged(size_t n);
	void saveSnapshot(std::string snapshotId);

private:
	// Don't pile on expunge messages.  Set a timer, if no expunge
	// events are reported within the prescribed time interval, notify
	// the user.

	TimerRedirect<myFolder> expungedTimer;

	void unsolicitedExpunge();
public:

	//////////////////////////////////////////////////////

	void checkNewMail(class mail::callback &cb);
	void newMessagesReceived();
	void newMessagesReceivedAndFiltered(size_t filteredCnt);
	void messagesFiltered();

	void resort();

	void setSortFunction(std::string sortBy);

	std::string getSortFunction() const;

	// Rather than force everything to recompile whenever a change is
	// necessary to the threading algorithm, use a class to encapsulate
	// all the needed functions to implement threading.

	class thread;

	friend class thread;

	//////////////////////////////////////////////////////
	//
	// Message navigation - used by CursesMessageDisplay

	bool getNextUnreadMessage(size_t &messageNum);
	bool getNextMessage(size_t &messageNum);
	bool getPrevMessage(size_t &messageNum);

	// Open a specific message, or its message/rfc822 attachment.
	// If goMessage is not null, the new message's indicated member
	// function is invoked.

	void goMessage(size_t messageNum,
		       std::string mimeid,
		       void (myMessage::*completedFuncArg)()=NULL);

	// Download a draft, and open it.

	void goDraft();

	// View attachments to a message.
	void viewAttachments(size_t messageNum);

private:

	// Download a message, that invoke completedFuncArg

	void openMessage(size_t messageNum, std::string mimeid,
			 bool attachmentsOnly,
			 void (myMessage::*completedFuncArg)());

	size_t newMailCheckCount;
public:
	bool isCheckingNewMail();
	// TRUE if this object is busy checking for new mail.

private:
	void setSortFunctionNoSave(std::string sortBy);
	void setSortFunctionNoSort(std::string sortBy);

	bool sort_function_not;

	bool (myFolder::*sort_function)(size_t, size_t);
	std::string sort_function_name;

	bool sortByArrival(size_t, size_t);
	bool sortByDate(size_t, size_t);
	bool sortBySubject(size_t, size_t);
	bool sortByName(size_t, size_t);

};

#endif
