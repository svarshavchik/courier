/*
** Copyright 2003-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "curses/mycurses.H"
#include "curses/cursesmainscreen.H"
#include "curses/cursesstatusbar.H"
#include "curseshierarchy.H"
#include "gettext.H"
#include "filter.H"
#include "passwordlist.H"
#include "libmail/mail.H"
#include "libmail/misc.H"
#include "libmail/maildir.H"
#include "libmail/logininfo.H"
#include "myfolder.H"
#include "mymessage.H"
#include "myserver.H"
#include "myservercallback.H"
#include "myserverpromptinfo.H"
#include "myserverlogincallback.H"
#include "myserverremoteconfig.H"
#include "opensubfolders.H"
#include "specialfolder.H"
#include "globalkeys.H"
#include "addressbook.H"
#include "colors.H"
#include "acl.H"
#include <errno.h>
#include <set>
#include <algorithm>

using namespace std;

extern struct CustomColor color_fl_accountName,
	color_fl_accountType,
	color_fl_folderName,
	color_fl_folderDirName,
	color_fl_messageCount;

extern Gettext::Key key_ADDFOLDER;
extern Gettext::Key key_RENAME;
extern Gettext::Key key_DELFOLDER;
extern Gettext::Key key_FILTER;
extern Gettext::Key key_EDIT;
extern Gettext::Key key_SPECIAL;
extern Gettext::Key key_ABORT;
extern Gettext::Key key_ADDDIRECTORY;
extern Gettext::Key key_DOWN;
extern Gettext::Key key_UP;

extern Gettext::Key key_ADDRESSBOOK;
extern Gettext::Key key_DRAFTS;
extern Gettext::Key key_REMOTE;
extern Gettext::Key key_SENT;
extern Gettext::Key key_TOP;
extern Gettext::Key key_RESET;
extern Gettext::Key key_PERMS;
extern Gettext::Key key_PERMVIEW;
extern Gettext::Key key_PERMEDIT;

extern CursesStatusBar *statusBar;
extern CursesMainScreen *mainScreen;

extern void hierarchyScreen(void *);
extern void mainMenu(void *);
extern void editAccountScreen(void *);
extern void quitScreen(void *);
extern bool isMaildir(string);

extern unicode_char ucplus, ucasterisk;

CursesHierarchy::DrawIterator::DrawIterator(CursesHierarchy *meArg,
					    bool doEraseArg,
					    bool allArg)
	: me(meArg), doErase(doEraseArg), all(allArg)
{
}

CursesHierarchy::DrawIterator::~DrawIterator()
{
}

bool CursesHierarchy::DrawIterator::visit(Hierarchy::Folder *f)
{
	bool rc=me->drawErase(f, doErase);

	if (all)
		rc=true;
	return rc;
}

bool CursesHierarchy::DrawIterator::visit(Hierarchy::Server *s)
{
	bool rc=me->drawErase(s, doErase);

	if (all)
		rc=true;
	return rc;
}

/////////////////////

CursesHierarchy::KeyHandlerIterator::KeyHandlerIterator(CursesHierarchy *meArg,
							const Curses::Key
							&keyArg)
	: me(meArg), key(keyArg), consumed(false)
{
}

CursesHierarchy::KeyHandlerIterator::~KeyHandlerIterator()
{
}

bool CursesHierarchy::KeyHandlerIterator::visit(Hierarchy::Folder *f)
{
	consumed=me->processKeyInFocus(f, key);
	return true;
}

bool CursesHierarchy::KeyHandlerIterator::visit(Hierarchy::Server *s)
{
	consumed=me->processKeyInFocus(s, key);
	return true;
}




/////////////////////

int CursesHierarchy::currentRowNum=0;

CursesHierarchy::CursesHierarchy(Hierarchy *h, CursesContainer *parent) :
	Curses(parent), HierarchyDisplay(h),
	CursesKeyHandler(PRI_SCREENHANDLER),
	selectingFolder(false),
	folderSelected(NULL),
	serverSelected(NULL)
{
	h->screenOpened();
}

CursesHierarchy::~CursesHierarchy()
{
}

bool CursesHierarchy::drawErase(Hierarchy::Folder *folder, bool doErase)
{
	const mail::folder *f=folder->getFolder();

	Hierarchy::Server *s=folder->getParent()->toServer();

	size_t n=folder->getMessageCount();
	size_t u=folder->getUnreadCount();

	string render1, render2;

	int color1;

	if (s && s->getServer()->server
	    && s->getServer()->server->hasCapability(LIBMAIL_SINGLEFOLDER))
	{
		const char *fmt;

		color1=color_fl_folderName.fcolor;

		if (n || u)
		{
			fmt=u ? _("%1% messages, %2% unread")
				: _("%1% messages");
		}
		else
		{
			fmt=_("*** empty ***");
		}

		render1=_("Folder: ");
		render2=Gettext(fmt) << n << u;
	}
	else
	{
		const unicode_char *pfix=f->hasSubFolders() || !f->hasMessages()
			? f->hasMessages() ? &ucasterisk:&ucplus:NULL;

		color1=color_fl_folderDirName.fcolor;

		if (pfix == 0)
			color1=color_fl_folderName.fcolor;
		else
		{
			std::vector<unicode_char> buf;

			buf.push_back(*pfix);

			render1=mail::iconvert::convert(buf,
							unicode_default_chset()
							);
			render1 += " ";
		}

		render1 += f->getName();
		render1 += " ";

		if (n || u)
			render2 = Gettext( u ?
					   _("(%1% messages, %2% unread)"):
					   _("(%1% messages)")) << n << u;
	}

	return drawErase(render1, color1, render2,
			 color_fl_messageCount.fcolor,
			 folder, doErase);
}

bool CursesHierarchy::drawErase(Hierarchy::Server *server, bool doErase)
{
	string name= Gettext("%1%: ") <<
		server->getServer()->serverName;

	return drawErase(name,
			 color_fl_accountName.fcolor,
			 server->getServer()->serverDescr,
			 color_fl_accountType.fcolor,
			 server, doErase);
}

bool CursesHierarchy::drawErase(string render1,
				int color1,
				string render2,
				int color2,
				Hierarchy::Entry *e,
				bool doErase)
{
	vector<unicode_char> w1, w2;

	mail::iconvert::convert(render1, unicode_default_chset(), w1);
	mail::iconvert::convert(render2, unicode_default_chset(), w2);

	widecharbuf wcbuf1, wcbuf2;

	wcbuf1.init_unicode(w1.begin(), w1.end());
	wcbuf2.init_unicode(w2.begin(), w2.end());

	wcbuf1.expandtabs(0);
	wcbuf2.expandtabs(0);

	w1.clear();
	w2.clear();
	wcbuf1.tounicode(w1);
	wcbuf2.tounicode(w2);

	Curses::CursesAttr attribute1;
	Curses::CursesAttr attribute2;

	attribute1.setFgColor(color1);
	attribute2.setFgColor(color2);

	if (doErase)
	{
		w1.clear();
		w1.insert(w1.end(), wcbuf1.wcwidth(0), ' ');
		w2.clear();
		w2.insert(w2.end(), wcbuf2.wcwidth(0), ' ');
	}
	else if (currentRowNum == e->getRow())
	{
		attribute1.setReverse();
		attribute2.setReverse();
	}

	int col1=e->getNestingLevel()*2+2;

	bool flag1=writeText(w1, e->getRow(), col1, attribute1);

	col1 += wcbuf1.wcwidth(0);

	bool flag2=writeText(w2, e->getRow(), col1, attribute2);

	return (flag1 && flag2);
}

	// Inherited from Curses:

int CursesHierarchy::getWidth() const
{
	return getScreenWidth();
}

int CursesHierarchy::getHeight() const
{
	return getHierarchy()->getLastRow()->getRow()+1;
}

void CursesHierarchy::draw()
{
	DrawIterator iterator(this, false, true);

	getHierarchy()->root.prefixIterate(iterator);
}

void CursesHierarchy::draw(Hierarchy::Entry *e, bool doErase)
{
	DrawIterator iterator(this, doErase, false);

	e->visit(iterator);
}

bool CursesHierarchy::isFocusable()
{
	return true;
}

// Navigate folder hierarchy

bool CursesHierarchy::processKeyInFocus(const Curses::Key &key)
{
	Hierarchy::Entry *oldEntry=getHierarchy()->getEntry(currentRowNum);

	if (key == key.UP || key == key.PGUP)
	{
		size_t sh;
		size_t firstShownRow;

		getVerticalViewport(firstShownRow, sh);

		if (sh < 1)
			sh=1;

		if ((size_t)currentRowNum < sh)
			sh=currentRowNum;
		else if (sh > 1)
			--sh;

		if (key == key.UP)
			sh=1;

		Hierarchy::Entry *newEntry=NULL;
		size_t n=currentRowNum;

		while (n > 0)
		{
			--n;

			if (sh > 0)
				--sh;

			Hierarchy::Entry *e=getHierarchy()->getEntry(n);

			if (e != NULL)
			{
				newEntry=e;
				currentRowNum=n;
				if (sh == 0)
					break;
			}
		}

		if (newEntry)
		{
			draw(oldEntry, false);
			draw(newEntry, false);
			return true;
		}
		else
		{
			Curses *p=getParent();

			if (p)
				p->scrollTo(0);
		}
		return true;
	}

	if (key == key.DOWN || key == key.PGDN)
	{
		size_t h=getHeight();
		size_t n=currentRowNum;

		size_t firstRow, sh;

		getVerticalViewport(firstRow, sh);

		if (sh < 1)
			sh=1;

		if (firstRow + sh - 1 > (size_t)currentRowNum)
			sh=firstRow + sh - 1 - currentRowNum;
		else if (sh > 1)
			--sh;

		if (key == key.DOWN)
			sh=1;

		Hierarchy::Entry *newEntry=NULL;

		while (++n < h)
		{
			if (sh > 0)
				--sh;

			Hierarchy::Entry *e=getHierarchy()->getEntry(n);

			if (e != NULL)
			{
				newEntry=e;
				currentRowNum=n;
				if (sh == 0)
					break;
			}
		}

		if (newEntry)
		{
			draw(oldEntry, false);
			draw(newEntry, false);
			return true;
		}

		return true;
	}

	if (!oldEntry)
		return false;

	// Over to the other processKeyInFocus() functions...

	KeyHandlerIterator kh(this, key);

	oldEntry->visit(kh);
	return kh.isConsumed();
}

bool CursesHierarchy::processKeyInFocus(Hierarchy::Folder *f,
					const Curses::Key &key)
{
	if (key == key.ENTER)
	{
		if (f->getFolder()->hasMessages())
		{
			Hierarchy::Entry *e=f;

			while ((e=e->getParent()) != NULL)
			{
				Hierarchy::Server *s=e->toServer();

				if (s != NULL)
				{
					if (selectingFolder)
					{
						folderSelected=f->getFolder();
						serverSelected=s->getServer();

						if (folderSelected &&
						    serverSelected)
						{
							Curses::keepgoing
								=false;
							return true;
						}
						folderSelected=NULL;
						serverSelected=NULL;
						break;
					}

					s->getServer()->
						openFolder(f->getFolder());
					break;
				}
			}
		}
		else
		{
			if (f->size() > 0)
				closeSubFolders(f);
			else
				openSubFolders(f);
		}
		return true;
	}

	if (key == '+' || key == '=')
	{
		if (f->size() == 0)
			openSubFolders(f);
		return true;
	}

	if (key == '-')
	{
		if (f->size() > 0)
			closeSubFolders(f);
		return true;
	}
	return false;
}

// Helper class that returns my Folder object.

Hierarchy::Folder *CursesHierarchy::getCurrentFolder()
{
	Hierarchy::Entry *e=getHierarchy()->getEntry(currentRowNum);

	if (!e)
		return NULL;

	return e->toFolder();
}

// Helper class that finds my server object.

class CursesHierarchyFindServerIterator : public Hierarchy::EntryIterator {
public:
	myServer *server;

	CursesHierarchyFindServerIterator();
	~CursesHierarchyFindServerIterator();

	bool visit(Hierarchy::Server *s);
	static myServer *find(Hierarchy::Entry *e);
};

CursesHierarchyFindServerIterator::CursesHierarchyFindServerIterator()
	: server(NULL) 
{
}

CursesHierarchyFindServerIterator::~CursesHierarchyFindServerIterator()
{
}

bool CursesHierarchyFindServerIterator::visit(Hierarchy::Server *s)
{
	server=s->getServer();
	return true;
}

myServer *CursesHierarchyFindServerIterator::find(Hierarchy::Entry *e)
{
	CursesHierarchyFindServerIterator iterator;

	while (e != NULL && iterator.server == NULL)
	{
		e->visit(iterator);
		e=e->getParent();
	}
	return iterator.server;
}

void CursesHierarchy::openSubFolders(Hierarchy::Folder *f)
{
	myServer *s=CursesHierarchyFindServerIterator::find(f);

	if (!s)
		return;

	statusBar->clearstatus();
	statusBar->status(Gettext(_("Opening %1%..."))
			  << f->getFolder()->getName());

	OpenSubFoldersCallback openSubfolders;
	{
		myServer::Callback callback;
		f->getFolder()->readSubFolders(openSubfolders, callback);
		myServer::eventloop(callback);
	}

	sort(openSubfolders.folders.begin(),
	     openSubfolders.folders.end(),
	     mail::folder::sort(false));

	vector<mail::folder *>::iterator
		b=openSubfolders.folders.begin(),
		e=openSubfolders.folders.end();

	drawEraseBelow(f, true);

	b=openSubfolders.folders.begin();
	e=openSubfolders.folders.end();

	vector<Hierarchy::Folder *> folders;

	Hierarchy::Server *parentServer=f->getServer();

	set<string> topmostFolders;

	if (parentServer) // Opening top-level folders.
	{
		vector<Hierarchy::Entry *>::iterator tb=parentServer->begin(),
			te=parentServer->end();

		while (tb != te)
		{
			Hierarchy::Folder *f= (*tb)->toFolder();

			tb++;

			if (f)
				topmostFolders.insert(f->getFolder()
						      ->getPath());
		}
	}

	while (b != e)
	{
		string fid= (*b)->getPath();

		// Do not show folders listed at the topmost level

		if (topmostFolders.count(fid) > 0)
		{
			b++;
			continue;
		}

		Hierarchy::Folder *child=
			new Hierarchy::Folder(myServer::hierarchy, f,
					      *b++);
		
		if (!child)
			outofmemory();
		folders.push_back(child);
	}

	drawEraseBelow(f, false); // Redraw the display below.

	s->openedHierarchy(f, openSubfolders.folders);

	vector<Hierarchy::Folder *>::iterator sb=folders.begin(),
		se=folders.end();

	while (sb != se)
	{
		(*sb++)->updateInfo(s, true); // Get folder sizes
	}
}

//
// Make sure that 'e' is visible.  This is usually needed after opening
// subfolders.  If the parent folder is on the last line of the display then
// there's no immediate visual indication that subfolders were opened.
// Fix this by temporarily fudging the focus to be on the first opened
// subfolder, if its off the screen then getCursorPosition() will scroll it.
// Afterwards restore cursor position where it was.

void CursesHierarchy::visible(Hierarchy::Entry *e)
{
	Hierarchy::Entry *oldEntry=getHierarchy()->getEntry(currentRowNum);

	if (!oldEntry)
		return;

	int saveRowNum=currentRowNum;

	int dummyrow;
	int dummycol;

	currentRowNum=e->getRow();
	getCursorPosition(dummyrow, dummycol);
	currentRowNum=saveRowNum;
	draw( e, false);
	draw( oldEntry, false);
}

void CursesHierarchy::closeSubFolders(Hierarchy::Folder *f)
{
	drawEraseBelow(f, true);
	f->clear();
	drawEraseBelow(f, false);
}

//
// Make sure that this server is logged into.

bool CursesHierarchy::autologin(myServer *ss)
{
	if (ss->server)
		return true;

	string savedPwd;

	if (!PasswordList::passwordList.check(ss->url, savedPwd) ||
	    !ss->login(savedPwd))
	{
		ss->disconnect();
		PasswordList::passwordList.remove(ss->url);

		myServerLoginCallback loginCallback;

		if (!ss->login(loginCallback))
		{
			ss->disconnect();
			statusBar->beepError();
			return false;
		}

		PasswordList::passwordList.save(ss->url, ss->password);
	}

	return true;
}

bool CursesHierarchy::processKeyInFocus(Hierarchy::Server *s,
					const Curses::Key &key)
{
	if (key == key.ENTER)
	{
		bool doShowHierarchy=false;

		//
		// Automatically open the folder of a SINGLEFOLDER account.
		// Now, we used to call ss->showHierarchy() immediately
		// after autologin() returned true.  What happened was that
		// this resulted in a new line being shown on the screen,
		// that contained the number of messages in the folder.
		// Hierarchy::Folder::processKeyInFocus() would then get
		// called next, which opens the folder.
		//
		// The initial reaction is to hit enter again, after the
		// folder size is displayed; however the folder open process
		// is already kicked off automatically, and is already in
		// progress.
		//
		// To fix this, CursesMainScreen::Lock structure was created
		// which suppresses screen output, and ss->showHierarchy
		// is called while the output lock is being held (in the case
		// where the account is a SINGLEFOLDER account.  This looks
		// much nicer.
		//

		myServer *ss=s->getServer();

		if (ss->server == NULL ||
		    s->size() == 0)
		{
			if (autologin(ss))
				doShowHierarchy=true;
			else
				return true;
		}

		// Automatically open the folder in single-folder accounts.

		if (ss->server &&
		    ss->server->hasCapability(LIBMAIL_SINGLEFOLDER))
		{
			CursesMainScreen::Lock updateLock(mainScreen);

			if (doShowHierarchy)
				ss->showHierarchy();

			if (ss->server && s->size() == 1)
			{
				Hierarchy::Folder *f=(*s->begin())->toFolder();

				if (f)
					processKeyInFocus(f, key);
			}
		}
		else
		{
			if (doShowHierarchy)
				ss->showHierarchy();
		}

		return true;
	}

	return false;
}

int CursesHierarchy::getCursorPosition(int &row, int &col)
{
	Hierarchy::Entry *e;
	bool refocus=false;

	// Try to adjust cursor position

	while ((e=getHierarchy()->getEntry(currentRowNum)) == NULL)
	{
		if (currentRowNum == 0)
		{
			while (currentRowNum < 10)
			{
				e=getHierarchy()->getEntry(++currentRowNum);
				if (e)
				{
					refocus=true;
					break;
				}
			}
			if (e == NULL)
				currentRowNum=0;
			break;
		}
		--currentRowNum;
		refocus=true;
	}

	if (refocus && e)
		draw(e);

	row=currentRowNum;
	if (e)
	{
		col=e->getNestingLevel() * 2 + 1;

		Curses::getCursorPosition(row, col);
		return 0;
	}
	col=0;
	Curses::getCursorPosition(row, col);
	return 0;
}

//
// On entry to the curses screen, try to reposition the cursor where we left
// off.

void CursesHierarchy::requestFocus()
{
	Hierarchy::Entry *e=getHierarchy()->getEntry(currentRowNum);

	if (e)
	{
		if (e->toFolder() != NULL || e->toServer() != NULL)
		{
			Curses::requestFocus();
			return;
		}
	}

	vector<Hierarchy::Entry *>::iterator sb, se, fb, fe;

	sb=myServer::hierarchy.root.begin();
	se=myServer::hierarchy.root.end();

	while (sb != se)
	{
		fb=(*sb)->begin();
		fe=(*sb)->end();

		while (fb != fe)
		{
			currentRowNum=(*fb)->getRow();
			Curses::requestFocus();
			return;
		}

		sb++;
	}

	currentRowNum=0;
	Curses::requestFocus();
}

//
// This node's children were added or removed.  We need to move a portion of
// the hierarchy display.
//

void CursesHierarchy::drawEraseBelow(Hierarchy::Entry *e, bool doErase)
{
	if (doErase)
	{
		// When we're about to do something (first step is to erase
		// the entry) make sure that's where the cursor is.

		currentRowNum=e->getRow();
		int dummyRow, dummyCol;
		getCursorPosition(dummyRow, dummyCol); // Perhaps a scroll?
	}
	else	// After we did something, renumber rows.
	{
		Hierarchy::EntryIteratorAssignRow iterator(e->getRow());

		e->prefixIterateAfter(iterator);
	}

	DrawIterator iterator(this, doErase, false);

	e->prefixIterateAfter( iterator );
}

///////////////////////////////////////////////////////////////////////////

class CursesHierarchyAddFolderKeyHandler : public CursesKeyHandler {
public:
	bool addDirectory;

	CursesHierarchyAddFolderKeyHandler();
	~CursesHierarchyAddFolderKeyHandler();
	bool processKey(const Curses::Key &key);
	bool listKeys( vector< pair<string, string> > &list);
};



CursesHierarchyAddFolderKeyHandler::CursesHierarchyAddFolderKeyHandler()
		: CursesKeyHandler(PRI_STATUSOVERRIDEHANDLER),
		  addDirectory(false)
{
}

CursesHierarchyAddFolderKeyHandler::~CursesHierarchyAddFolderKeyHandler()
{
}

bool CursesHierarchyAddFolderKeyHandler::processKey(const Curses::Key &key)
{
	if (key == key_ADDDIRECTORY)
	{
		addDirectory=true;
		statusBar->fieldEnter();
		return true;
	}

	return false;
}

bool CursesHierarchyAddFolderKeyHandler
::listKeys(vector< pair<string, string> > &list)
{
	list.push_back( make_pair(Gettext::
				  keyname(_("ADDDIRECTORY:^D")),
				  _("Add Folder Directory")));
	return true;
}

///////////
//
// Helper class to reverify folder's existence after a DELETE


class CursesHierarchyDeleteFolderVerifyCallback :
	public mail::callback::folderList {

	CursesHierarchy *me;
	Hierarchy::Folder *parent, *deleted;

public:
	CursesHierarchyDeleteFolderVerifyCallback(CursesHierarchy *meArg,
						  Hierarchy::Folder *parentArg,
						  Hierarchy::Folder
						  *deletedArg);
	~CursesHierarchyDeleteFolderVerifyCallback();
	void success(const vector<const mail::folder *> &folders);
};

CursesHierarchyDeleteFolderVerifyCallback::
CursesHierarchyDeleteFolderVerifyCallback(CursesHierarchy *meArg,
					  Hierarchy::Folder *parentArg,
					  Hierarchy::Folder *deletedArg)
	: me(meArg), parent(parentArg), deleted(deletedArg)
{
}

CursesHierarchyDeleteFolderVerifyCallback::
~CursesHierarchyDeleteFolderVerifyCallback()
{
}

void CursesHierarchyDeleteFolderVerifyCallback
::success(const vector<const mail::folder *> &folders)
{
	vector<const mail::folder *>::const_iterator b=folders.begin(),
		e=folders.end();

	const mail::folder *updatedFolder=NULL;

	while (b != e)
	{
		const mail::folder *f= *b++;
		if ( f->getName() ==
		     deleted->getFolder()->getName())
		{
			updatedFolder=f;
			break;
		}
	}

	me->processDeletedFolder(parent, deleted, updatedFolder);
}

///////////
bool CursesHierarchy::processKey(const Curses::Key &key)
{
	if (selectingFolder)
	{
		if (key == key_ABORT)
		{
			folderSelected=NULL;
			Curses::keepgoing=false;
			return true;
		}

		return false;
	}

	if (key == key_ADDFOLDER)
	{
		bool adddir;
		string folder;

		Hierarchy::Folder *f=getCurrentFolder();

		myServer *serverPtr;

		if (f == NULL) // Add shortcut to a server manually.
		{
			Hierarchy::Entry *e=getHierarchy()
				->getEntry(currentRowNum);

			if (!e || !e->toServer())
				return true;

			myServer::promptInfo response=myServer
				::prompt(myServer::
					 promptInfo(Gettext(_("Add folder/directory in %1%: "))
						    << (serverPtr=e->toServer()
							->getServer())
						    ->serverName));
			if (response.aborted() || serverPtr->server == NULL)
				return true;

			string path=response;

			myServer *ms=e->toServer()->getServer();

			if (ms->server == NULL && !autologin(ms))
				return true;

			if (path.size() > 0)
			{
				path=ms->server->translatePath(path);

				if (path.size() == 0)
				{
					statusBar->status(Gettext(_("Invalid folder name: %s"))
							  << strerror(errno));
					statusBar->beepError();
					return true;
				}
			}

			myServer::Callback callback;
			myServer::CreateFolderCallback searchCallback;

			ms->server->findFolder(path,
					       searchCallback, callback);

			if (!myServer::eventloop(callback))
				return true;

			if (searchCallback.folderFound == NULL)
			{
				statusBar->status(_("Folder not found."));
				statusBar->beepError();
				return true;
			}

			if (searchCallback.folderFound->hasMessages() &&
			    searchCallback.folderFound->hasSubFolders())
			{
				response=
					myServer
					::prompt(myServer::
						 promptInfo(Gettext(_("Add %1% as a folder directory? (Y/N) "))
							    << (string)response
						    ).yesno());

				if (response.aborted()
				    || serverPtr->server == NULL)
					return true;

				if ((string)response == "Y")
					searchCallback.folderFound
						->hasMessages(false);
				else
					searchCallback.folderFound
						->hasSubFolders(false);

			}
			drawEraseBelow(ms->hierarchyEntry, true);

			Hierarchy::Folder *hf=
				new Hierarchy::Folder(e->getHierarchy(),
						      ms->hierarchyEntry,
						      searchCallback
						      .folderFound);

			if (!hf)
				outofmemory();

			drawEraseBelow(ms->hierarchyEntry, false);
			ms->updateHierarchy();
			hf->updateInfo(ms, false);
			return true;
		}
		else
		{
			Hierarchy::Server *s=f->getServer();

			if (!s || !(serverPtr=s->getServer()))
				return true;
		}

		if (!f->getFolder()->hasSubFolders() &&
		    f->size() == 0)
		{
			// "A" on a top-level folder (not a folder directory)
			// is a noop.

			Hierarchy::Entry *e=f->getParent();

			if (e == NULL || (f=e->toFolder()) == NULL)
				return true;
		}

		{
			CursesHierarchyAddFolderKeyHandler add_key_handler;

			myServer::promptInfo response=
				myServer
				::prompt(myServer
					 ::promptInfo(Gettext(_("Create new folder in %1%: "))
						      << f->getFolder()->getName()
						      ));

			if (response.aborted() || serverPtr->server == NULL)
				return true;
			folder=response;
			adddir=add_key_handler.addDirectory;
		}

		if (adddir)
		{
			myServer::promptInfo response=
				myServer
				::prompt(myServer
					 ::promptInfo(Gettext(_("Create new folder directory in %1%: "))
						      << f->getFolder()->getName()
						      )
					 .initialValue(folder));

			if (response.aborted() || serverPtr->server == NULL)
				return true;
			folder=response;
		}

		if (folder.size() == 0)
			return true;

		if (f->size() == 0) // Make sure it's open now...
			openSubFolders(f);

		statusBar->clearstatus();

		// If this folder already exists, we must now be creating
		// a folder that both has subfolders, and messages.
		// Search for an existing folder with the same name.

		vector<Hierarchy::Entry *>::iterator b=f->begin(), e=f->end();

		Hierarchy::Folder *oldFolder=NULL;

		bool hasSubfolders=false;
		bool hasMessages=false;

		while (b != e)
		{
			Hierarchy::Entry *entryPtr= *b++;

			Hierarchy::Folder *p= entryPtr->toFolder();

			if (p != NULL &&
			    folder == p->getFolder()->getName())
			{
				oldFolder=p;
				hasMessages=p->getFolder()->hasMessages();
				hasSubfolders=p->getFolder()->hasSubFolders();
				break;
			}
		}

		if (adddir)
			hasSubfolders=true;
		else
			hasMessages=true;


		myServer::Callback callback;
		myServer::CreateFolderCallback newsubfolder;

		f->getFolder()->createSubFolder(folder, adddir,
						newsubfolder, callback);

		if (!myServer::eventloop(callback)
		    || newsubfolder.folderFound == NULL)
			return true;

		mail::folder *newFolder=newsubfolder.folderFound;

		// Now, update the hierarchy tree.

		newFolder->hasMessages(hasMessages);
		newFolder->hasSubFolders(hasSubfolders);

		drawEraseBelow(f, true);

		Hierarchy::Folder *addFolder=
			new Hierarchy::Folder(f->getHierarchy(),
					      f, newFolder);

		if (!addFolder)
			outofmemory();

		if (oldFolder)
		{
			addFolder->setMessageCount(oldFolder->
						   getMessageCount());
			addFolder->setUnreadCount(oldFolder->
						  getUnreadCount());

			delete oldFolder;
		}

		f->resortChildren();
		drawEraseBelow(f, false);

		Hierarchy::Entry *oldCursor
			=getHierarchy()->getEntry(currentRowNum);

		currentRowNum=addFolder->getRow();

		if (oldCursor)
			draw(oldCursor, false);
		draw(addFolder, false);

		return true;
	}

	if (key == key_RENAME)
	{
		Hierarchy::Folder *oldFolder=getCurrentFolder(), *parent;
		Hierarchy::Entry *parentEntry;

		// Cannot rename top level folders.
		if (!oldFolder || !(parentEntry=oldFolder->getParent()) ||
		    !(parent=parentEntry->toFolder()))
		{
			statusBar->status("Cannot rename this folder.");
			statusBar->beepError();
			return true;
		}

		Hierarchy::Server *s=oldFolder->getServer();
		myServer *serverPtr;

		if (!s || !(serverPtr=s->getServer()))
			return true;

		myServer::promptInfo response=
			myServer
			::prompt(myServer::
				 promptInfo(_("New name: ")));

		if (response.aborted() || serverPtr->server == NULL)
			return true;

		string newname=(string)response;

		if (newname.size() == 0)
			return true;

		myServer::Callback callback;
		myServer::CreateFolderCallback newsubfolder;

		oldFolder->getFolder()->renameFolder(parent->getFolder(),
						     newname,
						     newsubfolder, callback);

		if (!myServer::eventloop(callback)
		    || newsubfolder.folderFound == NULL)
			return true;

		Hierarchy::Entry *oldCursor
			=getHierarchy()->getEntry(currentRowNum);

		drawEraseBelow(parentEntry, true);

		if (oldCursor)
			draw(oldCursor, true);

		Hierarchy::Folder *addFolder=
			new Hierarchy::Folder(parentEntry->getHierarchy(),
					      parentEntry,
					      newsubfolder.folderFound);

		delete oldFolder;

		parent->resortChildren();
		drawEraseBelow(parent, false);

		oldCursor=getHierarchy()->getEntry(currentRowNum);

		currentRowNum=addFolder->getRow();

		draw(addFolder, false);
		if (oldCursor)
			draw(oldCursor, false);

		return true;
	}

	if (key == key_PERMS)
	{
		Hierarchy::Folder *currentFolder=getCurrentFolder();
		Hierarchy::Server *s;

		if (!currentFolder ||
		    !(s=currentFolder->getServer()))
			return true;

		myServer::promptInfo prompt=
			myServer::promptInfo(_("Permissions: (V/E) ")
					     )
			.option(key_PERMVIEW,
				_("V"),
				_("View permissions"))
			.option(key_PERMEDIT,
				_("E"),
				_("Edit permissions"));

		prompt=myServer::prompt(prompt);

		if (prompt.abortflag)
		{
			return true;
		}

		currentFolder=getCurrentFolder();

		if (!currentFolder ||
		    !(s=currentFolder->getServer()))
			return true;

		vector<unicode_char> ka;

		mail::iconvert::convert((string)prompt,
					unicode_default_chset(), ka);

		if (ka.size() == 0)
			return true;

		if (key_PERMVIEW == ka[0])
		{
			statusBar->clearstatus();
			statusBar->status(_("Checking folder permissions..."));

			string perms;
			myServer::Callback callback;

			currentFolder->getFolder()->
				getMyRights(callback, perms);

			if (!myServer::eventloop(callback))
				return true;

			string permsVerbose="";
			size_t i;

			for (i=0; i<perms.size(); i++)
			{
				if (permsVerbose.size() > 0)
					permsVerbose += ", ";
				permsVerbose += Acl(perms.substr(i, 1))
					.getDescription();
			}
			if (permsVerbose.size() == 0)
				permsVerbose=_("No permissions");
			statusBar->clearstatus();
			statusBar->
				status(Gettext(_("You have the following "
						 "permissions on this folder:"
						 "\n\n%1%."))
				       << permsVerbose);
			return true;
		}

		if (key_PERMEDIT == ka[0])
		{
			Curses::keepgoing=false;
			myServer::nextScreen=&Acl::editScreen;
			myServer::nextScreenArg=currentFolder;
			return true;
		}
		return true;
	}

	if (key == key_RESET)
	{
		const char * const fldnames[]=
			{"FROM", "REPLY-TO", "FCC", "CUSTOM", "SIGNKEY", NULL};

		Hierarchy::Folder *currentFolder=getCurrentFolder();
		Hierarchy::Server *s;

		if (!currentFolder ||
		    !(s=currentFolder->getServer()))

		{
			statusBar->clearstatus();
			statusBar->status(_("This folder does not contain"
					    " messages."));
			statusBar->beepError();
			return true;
		}

		myServer::promptInfo prompt=
			myServer::promptInfo(Gettext(_("Reset memorized "
						       "headers for %1%? "
						       "(Y/N) ")) <<
					     currentFolder->getFolder()->
					     getName()).yesno();

		prompt=myServer::prompt(prompt);

		if (prompt.abortflag || (string)prompt != "Y" ||
		    myServer::nextScreen)
			return true;

		size_t i=0;

		for (i=0; fldnames[i]; i++)
			s->getServer()->
				updateFolderConfiguration(currentFolder
							  ->getFolder(),
							  fldnames[i], "");
		myServer::saveconfig();

		statusBar->clearstatus();
		statusBar->status(_("Memorized headers cleared."));
		return true;
	}


	if (key == key_DELFOLDER)
	{
		Hierarchy::Entry *e=getHierarchy()->getEntry(currentRowNum);

		Hierarchy::Server *server;

		myServer *serverPtr;

		if (e && (server=e->toServer()) != NULL) // Deleting server
		{
			serverPtr=server->getServer();

			if (!serverPtr)
				return true;

			if (myServer::server_list.size() <= 1)
			{
				statusBar->clearstatus();
				statusBar->status(_("Cannot remove the only mail account."));
				statusBar->beepError();
				return true;
			}

			myServer::promptInfo response=
				myServer
				::prompt(myServer::
					 promptInfo(Gettext(_("Remove account %1%? (Y/N) "))
						    << server->getServer()
						    ->serverName).yesno());

			if ((string)response == "Y")
			{
				Hierarchy::Entry *parent=e->getParent();

				if (parent)
				{
					// Find the previous server entry.

					vector<Hierarchy::Entry *>::iterator
						ce=parent->begin(),
						cb=parent->end();

					Hierarchy::Entry *erasePtr=NULL;
					Hierarchy::Entry *drawPtr=NULL;

					while (ce != cb)
					{
						Hierarchy::Entry *ptr=*ce++;

						if (ptr == e)
							break;
						erasePtr=drawPtr=ptr;
					}

					if (!erasePtr)
					{
						erasePtr=e;

						ce=parent->begin();
						cb=parent->end();

						while (ce != cb)
						{
							Hierarchy::Entry
								*ptr=*ce++;

							if (ptr == e)
								continue;
							drawPtr=ptr;
							break;
						}
					}

					string auxfile=serverPtr->newsrc;

					serverPtr->disconnect();

					CursesMainScreen::Lock
						logoutLock(mainScreen);
					int rowSave=erasePtr->getRow();

					// If nothing else has this URL,
					// delete all special folders for
					// this URL

					vector<myServer *>::iterator
						b=myServer::server_list
						.begin(),
						e=myServer::server_list.end();
					size_t cnt=0;

					while (b != e)
					{
						if ((*b)->url==serverPtr->url)
							++cnt;
						b++;
					}

					if (cnt < 2)
						SpecialFolder
							::deleteAccount(serverPtr
									->url);

					drawEraseBelow(erasePtr, true);

					bool wasRemote=mail::account
						::isRemoteUrl(serverPtr->url);
					delete server;
					delete serverPtr;
					myServer::saveconfig(wasRemote);
					drawPtr->setRow(rowSave);
					drawEraseBelow(drawPtr, false);

					if (auxfile.size() > 0)
					{
						if (unlink(auxfile.c_str()) < 0
						    && errno == EISDIR)
						{
							mail::maildir::
								maildirdestroy
								(auxfile);
						}
					}
				}
			}
			return true;
		}

		Hierarchy::Folder *f=getCurrentFolder();

		if (f == NULL || (server=f->getServer()) == NULL ||
		    (serverPtr=server->getServer()) == NULL)
			return true;

		e=f->getParent();

		Hierarchy::Folder *parent;

		if (e && (server=e->toServer()))
		{
			myServer::promptInfo response=
				myServer
				::prompt(myServer::
					 promptInfo(Gettext(_("Remove %1% from %2%? (Y/N) "))
						    << f->getFolder()
						    ->getName()
						    << server->getServer()
						    ->serverName).yesno());

			if ((string)response == "Y" && serverPtr->server)
			{
				drawEraseBelow(server, true);
				delete f;
				drawEraseBelow(server, false);
				server->getServer()->updateHierarchy();
			}
			statusBar->clearstatus();
			return true;
		}

		if (e == NULL || (parent=e->toFolder()) == NULL)
			return true;

		Hierarchy::Folder *deletedFolder=NULL;

		if (f->getFolder()->hasSubFolders() ||
		    !f->getFolder()->hasMessages())
		{
			myServer::promptInfo response=
				myServer
				::prompt(myServer
					 ::promptInfo(Gettext(_("Delete folder directory %1%? (Y/N) "))
						      << f->getFolder()->getName()
						      )
					 .yesno());

			if (!serverPtr->server)
				return true;

			if ((string)response == "Y")
			{
				statusBar->clearstatus();
				myServer::Callback callback;

				f->getFolder()->destroy(callback, true);
				if (!myServer::eventloop(callback))
					return true;
				deletedFolder=f;
			}
		}

		if (!deletedFolder && f->getFolder()->hasMessages())
		{
			myServer::promptInfo response=
				myServer
				::prompt(myServer
					 ::promptInfo(Gettext(_("Delete folder %1%? (Y/N) "))
						      << f->getFolder()->getName()
						      )
					 .yesno());

			if (!serverPtr->server)
				return true;

			if ((string)response == "Y")
			{
				statusBar->clearstatus();
				myServer::Callback callback;

				f->getFolder()->destroy(callback, false);
				if (!myServer::eventloop(callback))
					return true;
				deletedFolder=f;
			}
		}

		if (!deletedFolder)
			return true;

		myServer::Callback callback;
		CursesHierarchyDeleteFolderVerifyCallback
			verify(this, parent, deletedFolder);

		parent->getFolder()->readSubFolders(verify, callback);

		myServer::eventloop(callback);
		return true;
	}

	if (key == key_FILTER)
	{
		Hierarchy::Folder *f=getCurrentFolder();

		if (f)
		{
			Curses::keepgoing=false;
			myServer::nextScreen=&Filter::filterEditScreen;
			myServer::nextScreenArg=f;
			return true;
		}

		statusBar->clearstatus();
		statusBar->status(_("Only a folder may be filtered."));
		statusBar->beepError();
		return true;
	}

	if (key == key_UP) // CTRL-U
	{
		Hierarchy::Folder *f=getCurrentFolder();
		Hierarchy::Entry *e;

		if (f)
		{
			Hierarchy::Entry *e=f->getParent();

			if (e && e->toServer())
			{
				f->MoveUp();
				e->toServer()->getServer()->updateHierarchy();
				return true;
			}
		}
		else if ((e=getHierarchy()->getEntry(currentRowNum)) != NULL
			 && e->toServer())
		{
			myServer *ms=e->toServer()->getServer();

			vector<myServer *>::iterator msl=
				find(myServer::server_list.begin(),
				     myServer::server_list.end(), ms);

			if (msl != myServer::server_list.end() &&
			    msl != myServer::server_list.begin())
			{
				// Move the server structures

				vector<myServer *>::iterator msp=msl;

				--msp;

				myServer *swap= *msp;

				*msp=*msl;

				*msl=swap;

				// Move the stuff on screen.

				ms->hierarchyEntry->MoveUp();
				myServer::saveconfig();
			}
			return true;
		}
		statusBar->clearstatus();
		statusBar->status(_("Only top level folders or servers may be rearranged."));
		statusBar->beepError();
		return true;
	}

	if (key == key_DOWN) // CTRL-D
	{
		Hierarchy::Folder *f=getCurrentFolder();
		Hierarchy::Entry *e;

		if (f)
		{
			Hierarchy::Entry *e=f->getParent();

			if (e && e->toServer())
			{
				f->MoveDown();
				e->toServer()->getServer()->updateHierarchy();

				Hierarchy::Folder *ff=getCurrentFolder();

				// Follow the cursor
				currentRowNum=f->getRow();
				drawErase(f, false);
				drawErase(ff, false);
				return true;
			}
		}
		else if ((e=getHierarchy()->getEntry(currentRowNum)) != NULL
			 && e->toServer())
		{
			myServer *ms=e->toServer()->getServer();

			vector<myServer *>::iterator msl=
				find(myServer::server_list.begin(),
				     myServer::server_list.end(), ms);

			if (msl != myServer::server_list.end())
			{
				// Move the server structure

				vector<myServer *>::iterator msp=msl;

				msp++;

				if (msp != myServer::server_list.end())
				{
					myServer *swap= *msp;

					*msp=*msl;

					*msl=swap;

					// Move what's on the screen.
					ms->hierarchyEntry->MoveDown();

					currentRowNum=ms->hierarchyEntry
						->getRow();

					drawErase(swap->hierarchyEntry, false);
					drawErase(ms->hierarchyEntry, false);
					myServer::saveconfig();
				}
			}
			return true;
		}
		statusBar->clearstatus();
		statusBar->status(_("Only top level folders or servers may be rearranged."));
		statusBar->beepError();
		return true;

	}

	if (key == key_EDIT)
	{
		Hierarchy::Entry *e=getHierarchy()->getEntry(currentRowNum);

		if (!e)
			return true;

		Hierarchy::Server *s=e->toServer();

		if (!s)
		{
			statusBar->clearstatus();
			statusBar->status(_("Current selection not an account."));
			statusBar->beepError();
			return true;
		}

		string url=s->getServer()->url;

		{
			mail::loginInfo loginInfo;

			if (mail::loginUrlDecode(url, loginInfo) &&
			    (loginInfo.method == "imap" ||
			     loginInfo.method == "imaps" ||
			     loginInfo.method == "pop3" ||
			     loginInfo.method == "pop3s" ||
			     loginInfo.method == "pop3maildrop" ||
			     loginInfo.method == "pop3maildrops" ||
			     loginInfo.method == "nntp" ||
			     loginInfo.method == "nntps"))
			{
				myServer::nextScreen= &editAccountScreen;
				myServer::nextScreenArg=s->getServer();
				Curses::keepgoing=false;
				return true;
			}
		}
		statusBar->clearstatus();
		statusBar->status(_("This is a local mail account. "
				    "Create another local account from "
				    "the main menu."));
		statusBar->beepError();
		return true;
	}

	if (key == key_SPECIAL)
	{
		Hierarchy::Folder *f=getCurrentFolder();

		if (!f)
		{
			statusBar->clearstatus();
			statusBar->status(_("Current selection not a folder."));
			statusBar->beepError();
			return true;
		}

		myServer *serverPtr=CursesHierarchyFindServerIterator::find(f);

		if (!serverPtr)
			return true;

		mail::folder *folder=f->getFolder()->clone();

		if (!folder)
			outofmemory();

		try {
			myServer::promptInfo prompt=
				myServer::prompt(myServer::promptInfo(_("Use folder as: "))
						 .option(key_ADDRESSBOOK,
							 Gettext::keyname(_("ADDRESSBOOK_K:A")),
							 _("Address Book"))
						 .option(key_DRAFTS,
							 Gettext::keyname(_("DRAFTS_K:D")),
							 _("Drafts"))
						 .option(key_SENT,
							 Gettext::keyname(_("SENT_K:S")),
							 _("Sent mail"))
						 .option(key_TOP,
							 Gettext::keyname(_("TOP_K:T")),
							 _("Top level folder"))
						 .option(key_REMOTE,
							 Gettext::keyname(_("REMOTE_K:R")),
							 _("Remote configuration"))
						 );

			if (prompt.abortflag)
			{
				delete folder;
				return true;
			}

			vector<unicode_char> ka;

			mail::iconvert::convert(((string)prompt),
						unicode_default_chset(),
						ka);

			if (ka.size() == 0 || !serverPtr->server)
			{
				delete folder;
				return true;
			}

			if (key_ADDRESSBOOK == ka[0])
			{
				myServer::promptInfo prompt=
					myServer::prompt(myServer::promptInfo(_("Name of this address book: "))
							 .initialValue(Gettext(_("%1% on %2%")) <<
								       folder->getName() << serverPtr->serverName)
							 );

				string name=prompt;

				if (prompt.abortflag || name.size() == 0)
				{
					delete folder;
					return true;
				}

				AddressBook *abook=
					new AddressBook();

				if (!abook)
					outofmemory();

				try {
					abook->init(name,
						    serverPtr->url,
						    folder->toString());

					AddressBook::addressBookList
						.insert(AddressBook::
							addressBookList.end(),
							abook);
				} catch (...) {
					delete abook;
					LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
				}

				delete folder;
				myServer::saveconfig();
				statusBar->clearstatus();
				statusBar->status(_("Address Book created."));
				return true;

			}

			if (key_TOP == ka[0])
			{
				serverPtr->addTopLevelFolderRedraw(folder->
								   toString());
				delete folder;
				return true;
			}

			if (!folder->hasMessages())
			{
				statusBar->clearstatus();
				statusBar->status(_("This folder cannot store messages, only subfolders."));
				statusBar->beepError();
				delete folder;
				return true;
			}

			if (key_DRAFTS == ka[0])
			{
				SpecialFolder &f=SpecialFolder::folders
					.find(DRAFTS)->second;

				f.setSingleFolder(serverPtr->url,
						  folder->toString());
			}

			if (key_SENT == ka[0])
			{
				myServer::promptInfo prompt=
					myServer::prompt(myServer::promptInfo(_("Nickname for this sent mail folder: "))
							 .initialValue(Gettext(_("%1% in %2%"))
								       << folder->getName()
								       << serverPtr->serverName));

				string nameStr=prompt;

				if (prompt.abortflag || nameStr.size() == 0)
				{
					delete folder;
					return true;
				}


				SpecialFolder &f=SpecialFolder::folders
					.find(SENT)->second;

				nameStr=Gettext::toutf8(nameStr);

				if (f.findFolder(nameStr))
				{
					statusBar->clearstatus();
					statusBar->status(_("This nickname already exists."));
					statusBar->beepError();
					delete folder;
					return true;
				}

				f.addFolder(nameStr, serverPtr->url,
					    folder->toString());
			}

			if (key_REMOTE == ka[0])
			{
				myServer::promptInfo prompt=
					myServer::prompt(myServer::promptInfo(_("Import remote configuration (requires restart)? (Y/N): "))
							 .yesno());


				if (prompt.abortflag)
				{
					delete folder;
					return true;
				}

				myServer::remoteConfigURL=serverPtr->url;
				myServer::remoteConfigPassword=
					serverPtr->password;
				myServer::remoteConfigFolder=folder->getPath();

				if (myServer::remoteConfigAccount)
				{
					myServer::remoteConfigAccount
						->logout();
					delete myServer::remoteConfigAccount;
					myServer::remoteConfigAccount=NULL;
				}

				if ((myServer::remoteConfigAccount=
				     new myServer::remoteConfig()) == NULL)
				{
					LIBMAIL_THROW((strerror(errno)));
				}

				if ((string)prompt == "Y")
				{
					myServer::saveconfig(false, true);

					try {
						quitScreen(NULL);
					} catch (...) {
					}

					throw "RESET";
				}
			}
			myServer::saveconfig(true);
			delete folder;
		} catch (...) {
			delete folder;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}
		return true;
	}

	Hierarchy::Entry *e=getHierarchy()
		->getEntry(currentRowNum);

	myServer *s=e ? CursesHierarchyFindServerIterator::find(e):NULL;

	if (GlobalKeys::processKey(key, GlobalKeys::LISTFOLDERS, s))
		return true;

	return false;
}

//
// Folder's been deleted, supposedly, and we've listed the parent hierarchy.
// Figure out how to update the display.
//

void CursesHierarchy::processDeletedFolder(Hierarchy::Folder *parent,
					   Hierarchy::Folder *deleted,
					   const mail::folder *updated)
{
	drawEraseBelow(parent, true);

	if (updated) // Folder still exists
		deleted->updateFolder(updated);
	else
	{
		delete deleted;
		deleted=NULL;
	}

	parent->resortChildren();
	drawEraseBelow(parent, false);

	Hierarchy::Entry *oldCursor
		=getHierarchy()->getEntry(currentRowNum);

	if (deleted == NULL)
		deleted=parent;

	currentRowNum=deleted->getRow();

	if (oldCursor)
		draw(oldCursor, false);
	draw(deleted, false);
}

bool CursesHierarchy::listKeys( vector< pair<string, string> > &list)
{
	list.push_back( make_pair(Gettext::keyname(_("HIER_K:+/-")),
				  _("Subfolders")));

	if (selectingFolder)
	{
		list.push_back( make_pair(Gettext::keyname(_("ENTER_K:ENTER")),
					  _("Select folder")));
		list.push_back( make_pair(Gettext::keyname(_("ABORT_K:^C")),
					  _("Cancel")));
		return true;
	}

	GlobalKeys::listKeys(list, GlobalKeys::LISTFOLDERS);

	list.push_back( make_pair(Gettext::keyname(_("EDIT_K:E")),
				  _("Edit Acct")));
	list.push_back( make_pair(Gettext::keyname(_("FILTER_K:F")),
				  _("Filter")));
	list.push_back( make_pair(Gettext::keyname(_("ADDFOLDER_K:A")),
				  _("Add")));
	list.push_back( make_pair(Gettext::keyname(_("DELFOLDER_K:D")),
				  _("Delete")));
	list.push_back( make_pair(Gettext::keyname(_("MOVEUP:^U")),
				  _("Move up")));
	list.push_back( make_pair(Gettext::keyname(_("MOVEDN:^D")),
				  _("Move down")));
	list.push_back( make_pair(Gettext::keyname(_("SPECIAL:U")),
				  _("Use special")));
	list.push_back( make_pair(Gettext::keyname(_("RENAME:R")),
				  _("Rename")));
	list.push_back( make_pair(Gettext::keyname(_("RESET:^R")),
				  _("Reset hdrs")));
	list.push_back( make_pair(Gettext::keyname(_("PERMS:P")),
				  _("Permissions")));
	return false;
}
