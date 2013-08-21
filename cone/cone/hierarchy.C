/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "hierarchy.H"
#include "gettext.H"
#include "libmail/mail.H"
#include "curses/cursesstatusbar.H"
#include "myserver.H"
#include "myservertask.H"
#include "gettext.H"
#include "libmail/mail.H"

#include <queue>
#include <algorithm>

using namespace std;

extern CursesStatusBar *statusBar;
extern void hierarchyScreen(void *);

Hierarchy::EntryIterator::EntryIterator()
{
}

Hierarchy::EntryIterator::~EntryIterator()
{
}

bool Hierarchy::EntryIterator::visit(Folder *dummy)
{
	return true;
}

bool Hierarchy::EntryIterator::visit(Server *dummy)
{
	return true;
}

//////////////////////////////////////////////////////////////////////////////

HierarchyDisplay::HierarchyDisplay(Hierarchy *h)
	: hierarchyPtr(h)
{
	h->display=this;
}

HierarchyDisplay::~HierarchyDisplay()
{
	hierarchyPtr->display=NULL;
}

//////////////////////////////////////////////////////////////////////////////

Hierarchy::Hierarchy()
	: PreviousScreen(hierarchyScreen, NULL),
	  root(*this), display(NULL)
{
}

Hierarchy::~Hierarchy()
{
}

size_t Hierarchy::Entry::size() const
{
	return children.size();
}

Hierarchy::Entry *Hierarchy::getFirstRow()
{
	Entry *n= &root; // Its first child is the first row shown

	vector<Entry *>::iterator b, e;

	b=n->begin();
	e=n->end();

	if (b != e)
		n= *b;

	return n;
}

Hierarchy::Entry *Hierarchy::getLastRow()
{
	Entry *n= &root;

	vector<Entry *>::iterator b, e;

	while ((b=n->begin()) != (e=n->end()))
		n= *--e;

	return n;
}

Hierarchy::Entry::Entry(Hierarchy &h, Hierarchy::Entry *p)
	: hierarchy(h), parent(p), rowNum(-1), nestingLevel(0)
{
	if (p != NULL)
	{
		p->children.push_back(this);
		nestingLevel=p->nestingLevel+1;
	}
}

Hierarchy::Entry::~Entry()
{
	clear();
	setRow(-1);

	if (parent != NULL) // Unlink from parent.
	{
		vector<Entry *>::iterator b=parent->children.begin(),
			e=parent->children.end();

		while (b != e)
		{
			if (*b == this)
			{
				parent->children.erase(b, b+1);
				break;
			}
			b++;
		}
	}
}

//
// Each time this object is repositioned, update the rowmap.

void Hierarchy::Entry::setRow(int newRowNum)
{
	if (rowNum >= 0)
	{
		hierarchy.rowmap.erase(rowNum);
		rowNum= -1;
	}

	// Demap whatever other entry was on this row.

	if (hierarchy.rowmap.count(newRowNum) > 0)
		hierarchy.rowmap.find(newRowNum)->second->setRow(-1);

	if (newRowNum >= 0)
	{
		hierarchy.rowmap.insert(make_pair(newRowNum, this));
		rowNum=newRowNum;
	}
}

void Hierarchy::Entry::MoveUp()
{
	Entry *p=getParent();

	// Find myself in my parent's children list.

	vector<Entry *>::iterator b=p->begin(), e=p->end(),
		last=p->end();

	while (b != e)
	{
		if (*b == this)
		{
			if (last != p->end())
			{
				// Swap with last.

				int r=(*last)->getRow();

				getHierarchy().drawEraseBelow(*last, true);

				Entry *swap= *last;
				*last=*b;
				*b=swap;

				// After swapping, reassign everyone's row.

				EntryIteratorAssignRow iterator(r);

				prefixIterateAfter(iterator);

				getHierarchy().drawEraseBelow(this, false);
				break;
			}
		}
		last=b;
		b++;
	}
}

void Hierarchy::Entry::MoveDown()
{
	Entry *p=getParent();

	vector<Entry *>::iterator b=p->begin(), e=p->end(),
		last=p->end();

	while (b != e)
	{
		--e;
		if (*e == this)
		{
			if (last != p->end())
			{
				int r=getRow();

				getHierarchy().drawEraseBelow(this, true);

				Entry *swap= *last;
				*last=*e;
				*e=swap;

				EntryIteratorAssignRow iterator(r);
				swap->prefixIterateAfter(iterator);

				getHierarchy().drawEraseBelow(swap, false);
				break;
			}
		}
		last=e;
	}
}

void Hierarchy::Entry::clone(const Hierarchy::Entry &n)
{
	clear();

	vector<Entry *>::const_iterator
		b=n.begin(), e=n.end();

	while (b != e)
	{
		const Entry *c= *b++;

		if (c != NULL)
		{
			Entry *nc=c->clone();

			if (nc == NULL)
				outofmemory();

			try {
				children.push_back(nc);
				nc->nestingLevel=nestingLevel+1;
			} catch (...)
			{
				delete nc;
				LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
			}

			nc->parent=this;
		}
	}
}

void Hierarchy::Entry::clear()
{
	vector<Entry *>::iterator b=children.begin(), e=children.end();

	while (b != e)
	{
		Entry *c= *b;

		*b++ = NULL;

		if (c)
		{
			c->parent=NULL;
			delete c;
		}
	}

	children.clear();
}

// Iterate this node and its children in prefix order.

bool Hierarchy::Entry::prefixIterate(EntryIterator &callback)
{
	if (!visit(callback))
		return false;

	vector<Entry *>::iterator b=begin(), e=end();

	while (b != e)
		if (!(*b++)->prefixIterate(callback))
			return false;
	return true;
}

// Iterate this node and its children in prefix order, then iterate
// remaining nodes of its parent, and parent's parent, in prefix order.
//
// Essentially this is the same as traversing the entry hierarchy tree in
// prefix order, but not visiting any node until this node.

bool Hierarchy::Entry::prefixIterateAfter(Hierarchy::EntryIterator &callback)
{
	if (!prefixIterate(callback))
		return false;

	Entry *child=this;

	Entry *parent;

	while ((parent=child->getParent()) != NULL)
	{
		vector<Entry *>::iterator b=parent->begin(), e=parent->end();

		while (b != e)
			if ( *b++ == child)
				break;

		while (b != e)
			if (!(*b++)->prefixIterate(callback))
				return false;

		child=parent;
	}
	return true;
}

Hierarchy::Server::Server(Hierarchy &hierarchy, Entry *parent,
			  class myServer *serverArg)
	: Hierarchy::Entry(hierarchy, parent), server(serverArg)
{
}

Hierarchy::Server::~Server()
{
}

Hierarchy::Entry *Hierarchy::Server::clone() const
{
	Hierarchy::Server *miniMe=new Hierarchy::Server(getHierarchy(), NULL,
							server);

	if (!miniMe)
		return NULL;

	try {
		miniMe->Entry::clone(*this);
	} catch (...)
	{
		delete miniMe;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	return miniMe;
}

bool Hierarchy::Server::visit(Hierarchy::EntryIterator &callback)
{
	return callback.visit(this);
}

/////////////////////////////////////////////////////////////////////////////
//
// Safe casting from Entry to the desired object.
//

Hierarchy::CastIterator::CastIterator() : folder(NULL), server(NULL)
{
}

Hierarchy::CastIterator::~CastIterator()
{
}

bool Hierarchy::CastIterator::visit(Hierarchy::Folder *f)
{
	folder=f;
	return true;
}

bool Hierarchy::CastIterator::visit(Hierarchy::Server *s)
{
	server=s;
	return true;
}

//

Hierarchy::Folder *Hierarchy::Entry::toFolder()
{
	Hierarchy::CastIterator iterator;

	visit(iterator);

	return iterator.folder;
}

Hierarchy::Server *Hierarchy::Entry::toServer()
{
	Hierarchy::CastIterator iterator;

	visit(iterator);

	return iterator.server;
}

Hierarchy::Server *Hierarchy::Entry::getServer()
{
	Hierarchy::Entry *e=this;

	while (e)
	{
		Hierarchy::CastIterator iterator;

		e->visit(iterator);

		if (iterator.server)
			return iterator.server;

		e=e->getParent();
	}
	return NULL;
}


////////////

Hierarchy::EntryIteratorAssignRow::EntryIteratorAssignRow(int r)
	:  separate_servers(false), row(r)
{
}

Hierarchy::EntryIteratorAssignRow::~EntryIteratorAssignRow()
{
}

bool Hierarchy::EntryIteratorAssignRow::visit(Folder *f)
{
	separate_servers=true;
	f->setRow(row++);
	return true;
}

bool Hierarchy::EntryIteratorAssignRow::visit(Server *s)
{
	if (separate_servers)
		++row;
	separate_servers=true;
	s->setRow(row++);
	return true;
}

////////////

Hierarchy::Root::Root(Hierarchy &hierarchy)
	: Hierarchy::Entry(hierarchy, NULL)
{
}

Hierarchy::Root::~Root()
{
}

Hierarchy::Entry *Hierarchy::Root::clone() const
{
	Hierarchy::Root *miniMe=new Hierarchy::Root(getHierarchy());

	if (!miniMe)
		return NULL;

	try {
		miniMe->Entry::clone(*this);
	} catch (...)
	{
		delete miniMe;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	return miniMe;
}

bool Hierarchy::Root::visit(class EntryIterator &h)
{
	return true;
}

////////////////////////////////////////////////////////////////////////
//
// Background task to update message counts in this folder.

class HierarchyFolderUpdate : public myServer::Task,
			      public mail::callback {

	mail::callback::folderInfo folder_info;

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);

public:
	Hierarchy::Folder *myfolder;
	// What we're updating.  If this objects gets destroyed its
	// destructor will set myfolder to NULL.

	HierarchyFolderUpdate(Hierarchy::Folder *myfolderArg,
			      myServer *myserver, bool doCheaply);
	~HierarchyFolderUpdate();

	// Inherited from myServer::Task

	void start();

	// Inherited from mail::callback

	void success(string message);
	void fail(string message);
};

Hierarchy::Folder::Folder(Hierarchy &hierarchy, Entry *parent,
			  const mail::folder *folderArg)
	: Hierarchy::Entry(hierarchy, parent), folder(NULL),
	  messageCount(0), unreadCount(0), countsInitialized(false),
	  pendingUpdate(0)
{
	folder=folderArg->clone();

	if (!folder)
		outofmemory();
}

Hierarchy::Folder::~Folder()
{
	if (folder)
		delete folder;

	// If there's a pending folder update, detach that object from meself.

	if (pendingUpdate)
		pendingUpdate->myfolder=NULL;
}

Hierarchy::Entry *Hierarchy::Folder::clone() const
{
	Hierarchy::Folder *miniMe=new Hierarchy::Folder(getHierarchy(),
							NULL, folder);

	miniMe->messageCount=messageCount;
	miniMe->unreadCount=unreadCount;

	if (!miniMe)
		return NULL;

	try {
		miniMe->Entry::clone(*this);
	} catch (...)
	{
		delete miniMe;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
	return miniMe;
}

bool Hierarchy::Folder::visit(Hierarchy::EntryIterator &callback)
{
	return callback.visit(this);
}

void Hierarchy::drawErase(Folder *folder, bool doErase)
{
	if (display)
		display->drawErase(folder, doErase);
}

void Hierarchy::drawErase(Server *server, bool doErase)
{
	if (display)
		display->drawErase(server, doErase);
}

void Hierarchy::drawEraseBelow(Entry *entry, bool doErase)
{
	if (!doErase)
	{
		EntryIteratorAssignRow iterator(entry->getRow());

		entry->prefixIterateAfter( iterator );
	}
	if (display)
		display->drawEraseBelow(entry, doErase);
}

Hierarchy::Folder::sort::sort() : mail::folder::sort(false)
{
}

Hierarchy::Folder::sort::~sort()
{
}

void Hierarchy::Folder::updateFolder(const mail::folder *newPtr)
{
	mail::folder *f=newPtr->clone();

	if (!f)
		outofmemory();

	delete folder;
	folder=f;
}

void Hierarchy::Folder::updateInfo(myServer *server, bool doCheaply)
{
	if (pendingUpdate)
		return; // Already in progress

	if (!getFolder()->hasMessages())
		return;

	pendingUpdate=new HierarchyFolderUpdate(this, server, doCheaply);

	if (!pendingUpdate)
		outofmemory();

	pendingUpdate->add();
}

void Hierarchy::Folder::resortChildren()
{
	::sort( begin(), end(), sort());
}

bool Hierarchy::Folder::sort::operator()(Entry *a, Entry *b)
{
	Folder *af=a->toFolder();
	Folder *bf=b->toFolder();

	return (af && bf && mail::folder::sort::operator()(af->getFolder(),
							   bf->getFolder()));
}

// Background update.

HierarchyFolderUpdate::HierarchyFolderUpdate(Hierarchy::Folder *f,
					     myServer *myserver,
					     bool doCheaply)
	: myServer::Task(myserver), myfolder(f)
{
	folder_info.fastInfo=doCheaply;
}

HierarchyFolderUpdate::~HierarchyFolderUpdate()
{
	if (myfolder)
		myfolder->pendingUpdate=NULL; // Tit for tat
}

void HierarchyFolderUpdate::reportProgress(size_t bytesCompleted,
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
// HierarchyFolderUpdate is subclassed from Task -- pending background
// tasks for this server.  When its turn comes up, start() is invoked.

void HierarchyFolderUpdate::start()
{
	if (myfolder)
		myfolder->getFolder()->readFolderInfo(folder_info, *this);
	else
		done(); // Original folder was destroyed.
}

void HierarchyFolderUpdate::success(string message)
{
	if (myfolder) // Original folder not destroyed.
	{
		myfolder->getHierarchy().drawErase(myfolder, true);

		bool newMail= myfolder->getMessageCount()
			       < folder_info.messageCount
			|| myfolder->getUnreadCount()
			< folder_info.unreadCount;

		myfolder->setMessageCount(folder_info.messageCount);
		myfolder->setUnreadCount(folder_info.unreadCount);
		myfolder->countsInitialized=true;

		myfolder->getHierarchy().drawErase(myfolder, false);

		if (newMail && !myfolder->countsInitialized)
		{
			statusBar->clearstatus();
			statusBar->status(_("You have new mail"));
			statusBar->beepError();
		}
	}
	done();
}

void HierarchyFolderUpdate::fail(string message)
{
	done();
}



