/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "myfolderfilter.H"
#include "myserver.H"
#include "gettext.H"
#include "tags.H"
#include <sstream>
#include <errno.h>

using namespace std;

// A Filter object is created whenever there's a mail filter defined
// for the folder.  This occurs at the time the folder is opened,
// and whenever new mail arrives in this folder.  A pointer to
// the Filter object is saved here, together with the message range
// that's being filtered (which is automatically updated by
// messagesRemoved, if necessary).
//
// The Filter object maintains a back pointer to myFolder.
//
// If myFolder is destroyed, the Filter object is NOT destroyed, but
// the back pointer is NULLed out (because the Filter object may have
// outstanding callback we can't do that).

myFolder::FolderFilter::FolderFilter(myFolder *folderArg,
				     size_t filterStartArg,
				     size_t filterEndArg)
	: sc(*this), folder(folderArg), filterStart(filterStartArg),
	  filterEnd(filterEndArg), currentFolder(NULL)
{
}

myFolder::FolderFilter::~FolderFilter()
{
	if (currentFolder)
		delete currentFolder;
	if (folder)
		folder->currentFilter=NULL;
}

// When the folder is opened, run() goes through all the
// steps.

void myFolder::FolderFilter::run()
{
	currentStep=steps.begin();

	while (folder && currentStep != steps.end())
	{
		myServer::Callback cb;

		doStep(cb);

		if (!myServer::eventloop(cb))
			break;
	}
}

void myFolder::FolderFilter::start()
{
	currentStep=steps.begin();
	success_func=&myFolder::FolderFilter::stepCompleted; // Reset

	stepCompleted(_("Ok."));
}

void myFolder::FolderFilter::getMessageNums(std::vector<size_t> &v)
{
	size_t n;

	mail::account *server=folder->getServer()->server;

	for (n=filterStart; n<filterEnd; n++)
		if (currentSelection
		    .count(server->getFolderIndexInfo(n).uid) > 0)
			v.push_back(n);
}


void myFolder::FolderFilter::doStep(myServer::Callback &cb)
{
	vector< Filter::Step >::iterator step=currentStep;

	cb=myServer::Callback();

	mail::account *server;

	if (!folder || !(server=folder->getServer()->server))
	{
		cb.fail(_("Filtering aborted."));
		return;
	}

	if (step->type == Filter::Step::filter_step_tag)
	{
		vector<size_t> n;
		set<string> allTags;

		size_t i;

		for (i=1; i<Tags::tags.names.size(); i++)
			allTags.insert(Tags::tags.getTagName(i));

		origCallback= &cb;
		((myServer::Callback &)*this)=myServer::Callback();
		success_func=&myFolder::FolderFilter::setNewTags;

		getMessageNums(n);

		if (n.size() == 0)
			setNewTags(_("Ok."));
		else
			server->updateKeywords(n, allTags, true, false, *this);
		return;
	}

	if (step->type != Filter::Step::filter_step_copy &&
	    step->type != Filter::Step::filter_step_move)
	{
		doStep2(cb, NULL);
		return;
	}

	if (currentStep->toserver.size() > 0 ||
	    folder->getServer() == NULL)
	{
		cb.fail(_("Destination mail account is not open."));
		return;
	}

	foundFolder=myReadFolders();

	origCallback= &cb;
	((myServer::Callback &)*this)=myServer::Callback();
	success_func=&myFolder::FolderFilter::foundFolderFunc;
	server->findFolder(currentStep->tofolder, foundFolder, *this);
}

void myFolder::FolderFilter::setNewTags(string msg)
{
	success_func=&myFolder::FolderFilter::stepCompleted; // Reset
	doStep2(*origCallback, NULL);
}

void myFolder::FolderFilter::foundFolderFunc(string msg)
{
	success_func=&myFolder::FolderFilter::stepCompleted; // Reset

	mail::account *server;

	if (foundFolder.size() > 0 && (server=folder->getServer()->server)
	    != NULL)
	{
		if (currentFolder)
			delete currentFolder;

		if ((currentFolder=server->folderFromString(foundFolder[0]))
		    == NULL)
		{
			origCallback->fail(strerror(errno));
			return;
		}

		doStep2(*origCallback, currentFolder);
		return;
	}

	origCallback->fail(_("Destination folder does not exist."));
}


void myFolder::FolderFilter::doStep2(myServer::Callback &cb,
				     mail::folder *destFolder)
{
	vector< Filter::Step >::iterator step=currentStep;

	++currentStep;

	cb=myServer::Callback();

	mail::account *server;

	if (!folder || !(server=folder->getServer()->server))
	{
		cb.fail(_("Filtering aborted."));
		return;
	}
	vector<size_t> msgNums;

	if (filterStart < filterEnd) switch (step->type) {
	case Filter::Step::filter_step_selectall:

		currentSelection.clear();

		{
			size_t n;

			for (n=filterStart; n<filterEnd; n++)
				currentSelection
					.insert(server->getFolderIndexInfo(n)
						.uid);
		}
		cb.success(_("Ok"));
		break;
	case Filter::Step::filter_step_selectsrch:

		origCallback= &cb;
		success_func= &myFolder::FolderFilter::searchCompleted;
		currentSelection.clear();

		step->searchType.scope=mail::searchParams::search_range;
		step->searchType.rangeLo=filterStart;
		step->searchType.rangeHi=filterEnd;
		server->searchMessages(step->searchType, sc);
		return;

	case Filter::Step::filter_step_delete:

		getMessageNums(msgNums);

		if (msgNums.size() > 0)
		{
			mail::messageInfo flags;

			flags.deleted=true;

			server->updateFolderIndexFlags(msgNums, false,
						       true, flags, cb);
			return;
		}
		break;

	case Filter::Step::filter_step_undelete:

		getMessageNums(msgNums);

		if (msgNums.size() > 0)
		{
			mail::messageInfo flags;

			flags.deleted=true;

			server->updateFolderIndexFlags(msgNums, false,
						       false, flags, cb);
			return;
		}
		break;
	case Filter::Step::filter_step_delexpunge:

		getMessageNums(msgNums);
		currentSelection.clear();

		if (msgNums.size() > 0)
		{
			server->removeMessages(msgNums, cb);
			return;
		}
		break;
	case Filter::Step::filter_step_mark:

		getMessageNums(msgNums);

		if (msgNums.size() > 0)
		{
			mail::messageInfo flags;

			flags.marked=true;

			server->updateFolderIndexFlags(msgNums, false,
						       true, flags, cb);
			return;
		}
		break;
	case Filter::Step::filter_step_unmark:

		getMessageNums(msgNums);

		if (msgNums.size() > 0)
		{
			mail::messageInfo flags;

			flags.marked=true;

			server->updateFolderIndexFlags(msgNums, false,
						       false, flags, cb);
			return;
		}
		break;

	case Filter::Step::filter_step_tag:
		getMessageNums(msgNums);

		if (msgNums.size() > 0)
		{
			size_t n=0;

			istringstream i(step->name_utf8);

			i >> n;

			if (n == 0)
			{
				cb.success(_("Ok."));
				return;
			}

			set<string> keywords;

			keywords.insert(Tags::tags.getTagName(n));

			server->updateKeywords(msgNums,
					       keywords,
					       true,
					       true, cb);
			return;
		}
		break;

	case Filter::Step::filter_step_copy:

		getMessageNums(msgNums);

		if (msgNums.size() > 0)
		{
			server->copyMessagesTo(msgNums, destFolder, cb);
			return;
		}
		break;
	case Filter::Step::filter_step_move:
		getMessageNums(msgNums);
		currentSelection.clear();

		if (msgNums.size() > 0)
		{
			server->moveMessagesTo(msgNums, destFolder, cb);
			return;
		}
		break;

	case Filter::Step::filter_step_watch:

		getMessageNums(msgNums);

		{
			bool watchSet=false;

			size_t n;

			for (n=0; n<msgNums.size(); n++)
			{
				if (folder->serverIndex[msgNums[n]]
				    .watchLevel > 0)
					continue; // Already watching.

				folder->setWatch(msgNums[n],
						 Watch::defaultWatchDays,
						 Watch::defaultWatchDepth);
				watchSet=true;
			}
			if (watchSet)
				folder->watchUpdated();
		}
		break;
	case Filter::Step::filter_step_unwatch:
		{
			size_t n;

			for (n=0; n<msgNums.size(); n++)
			{
				folder->unwatch(msgNums[n]);
			}
		}
		folder->watchUpdated();
		break;
	}

	cb.success(_("Ok"));
}

void myFolder::FolderFilter::success(std::string message)
{
	(this->*success_func)(message);
}

void myFolder::FolderFilter::fail(std::string message)
{
	myServer::Callback::success(message);
}

void myFolder::FolderFilter::searchCompleted(std::string message)
{
	success_func=&myFolder::FolderFilter::stepCompleted; // Reset

	vector<size_t>::iterator b=sc.messageNumbers.begin(),
		e=sc.messageNumbers.end();

	mail::account *server;

	if (!folder || !(server=folder->getServer()->server))
	{
		origCallback->fail(_("Filtering aborted."));
		return;
	}

	while (b != e)
	{
		currentSelection.insert(server->getFolderIndexInfo(*b).uid);
		++b;
	}

	origCallback->success(message);
}

myFolder::FolderFilter *myFolder::installFilter()
{
	string filter=server->getFolderConfiguration(folder->getPath(),
						     "FILTER");

	if (filter.size() == 0)
		return NULL;

	if ((currentFilter=new myFolder::FolderFilter(this,
						      0,
						      server->server
						      ->getFolderIndexSize()))
	    == NULL)
		LIBMAIL_THROW(strerror(errno));

	try {
		string::iterator b=filter.begin(), e=filter.end();
		string::iterator s=b;

		while (b != e)
		{
			if (*b != '\n')
			{
				++b;
				continue;
			}

			currentFilter->steps
				.push_back(Filter::Step(string(s, b)));

			s=++b;
		}
	} catch (...) {
		delete currentFilter;
		currentFilter=NULL;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	return currentFilter;
}

void myFolder::FolderFilter::stepCompleted(std::string message)
{
	if (folder && currentStep != steps.end())
	{
		doStep(*this);
		return;
	}

	folder->messagesFiltered();
}
