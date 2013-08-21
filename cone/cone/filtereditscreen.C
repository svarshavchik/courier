/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "filtereditscreen.H"
#include "hierarchy.H"
#include "myserver.H"
#include "myserverpromptinfo.H"
#include "cursesindexdisplay.H"
#include "searchprompt.H"
#include "curses/cursesmainscreen.H"
#include "curses/cursestitlebar.H"
#include "curses/cursesstatusbar.H"
#include "gettext.H"
#include <errno.h>
#include <stdlib.h>
#include <sstream>

extern CursesMainScreen *mainScreen;
extern CursesTitleBar *titleBar;
extern CursesStatusBar *statusBar;
extern void mainMenu(void *);
extern void hierarchyScreen(void *);
extern void openHierarchyScreen(std::string prompt,
				PreviousScreen *prevScreen,
				mail::folder **selectedFolder,
				myServer **selectedServer);

extern Gettext::Key key_COPY;
extern Gettext::Key key_DELETE;
extern Gettext::Key key_EXPUNGE;
extern Gettext::Key key_MARK;
extern Gettext::Key key_MOVE;
extern Gettext::Key key_SELECT;
extern Gettext::Key key_TAG;
extern Gettext::Key key_UNDELETE;
extern Gettext::Key key_UNMARK;
extern Gettext::Key key_UNWATCH;
extern Gettext::Key key_WATCH2;

extern Gettext::Key key_UP;
extern Gettext::Key key_DOWN;
extern Gettext::Key key_ABORT;


using namespace std;

////////////////////////////////////////////////////////////////////////////

Filter::editScreen::Button::Button(Filter::editScreen *parentArg,
				   string Name,
				   bool isAddButtonArg)
	: CursesButton(parentArg, Name), myParent(parentArg),
	  isAddButton(isAddButtonArg)
{
	if (!isAddButtonArg)
		setStyle(MENU);
}

Filter::editScreen::Button::~Button()
{
}

bool Filter::editScreen::Button::processKeyInFocus(const Key &key)
{
	if (key == key.DEL)
	{
		myParent->del(myPos);
		return true;
	}

	if (key == key_UP)
	{
		myParent->moveup(myPos);
		return true;
	}

	if (key == key_DOWN)
	{
		myParent->movedn(myPos);
		return true;
	}

	return CursesButton::processKeyInFocus(key);
}

void Filter::editScreen::Button::clicked()
{
	if (isAddButton)
		myParent->add();
}

////////////////////////////////////////////////////////////////////////////

Filter::editScreen::editScreen(CursesContainer *parent)
	: CursesDialog(parent), CursesKeyHandler(PRI_SCREENHANDLER),
	  myTitle(this, _("Current Filters:")),
	  saveButton(this, _("Save Filters")),
	  doSelectFolder(false),
	  selectFolderFor(Filter::Step::filter_step_copy), doSave(false)
{
	saveButton=this;
	saveButton= &Filter::editScreen::save;
}

void Filter::editScreen::init(string filter)
{
	string::iterator b=filter.begin(), e=filter.end();
	string::iterator s=b;

	while (b != e)
	{
		if (*b != '\n')
		{
			++b;
			continue;
		}

		Filter::Step newStep( string(s, b));
		s=++b;

		Button *bb=new Button(this, newStep.getDescription(), false);

		if (!bb)
			LIBMAIL_THROW(strerror(errno));

		bb->step=newStep;

		try {
			buttons.push_back(bb);
		} catch (...) {
			delete bb;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}

		bb->myPos= --buttons.end();
	}

	Button *newButton=new Button(this, _("Add new filter"), true);
	if (!newButton)
		throw strerror(errno);

	try {
		buttons.push_back(newButton);
	} catch (...) {
		delete newButton;
		throw;
	}

	newButton->myPos= --buttons.end();

	myTitle.setRow(1);
	myTitle.setCol(2);

	size_t rowNum=3;

	list<Filter::editScreen::Button *>::iterator bb=buttons.begin(),
		ee=buttons.end();

	while (bb != ee)
	{
		(*bb)->setRow(rowNum);
		(*bb)->setCol(4);
		++rowNum;
		++bb;
	}

	++rowNum;
	saveButton.setRow(rowNum);
	saveButton.setCol(4);

	newButton->requestFocus();
}

Filter::editScreen::~editScreen()
{
}

bool Filter::editScreen::processKey(const Curses::Key &key)
{
	if (key == key_ABORT)
	{
		keepgoing=false;
		myServer::nextScreen= &hierarchyScreen;
		return true;
	}

	return false;
}

Filter::editScreen::operator std::string() const
{
	string filter;

	list<Filter::editScreen::Button *>::const_iterator
		b=buttons.begin(), e=buttons.end();

	while (b != e)
	{
		if (! (*b)->isAddButton)
		{
			filter += (string)(*b)->step;
			filter += "\n";
		}
		++b;
	}

	return filter;
}


void Filter::editScreen::add()
{
	myServer::promptInfo prompt=
		myServer::promptInfo(_("Add filter: "))
		.option(key_SELECT,
			Gettext::keyname(_("SELECT_K:S")),
			_("Select"))
		.option(key_DELETE,
			Gettext::keyname(_("DELETE_K:D")),
			_("mark Del"))
		.option(key_UNDELETE,
			Gettext::keyname(_("UNDELETE_K:U")),
			_("Unmark del"))
		.option(key_EXPUNGE,
			Gettext::keyname(_("EXPUNGE_K:X")),
			_("Del & eXpunge"))
		.option(key_MARK,
			Gettext::keyname(_("MARK_K:R")),
			_("maRk"))
		.option(key_UNMARK,
			Gettext::keyname(_("UNMARK_K:K")),
			_("unmarK"))
		.option(key_WATCH2,
			Gettext::keyname(_("WATCH_K:W")),
			_("Watch"))
		.option(key_UNWATCH,
			Gettext::keyname(_("UNWATCH_K:A")),
			_("unwAtch"))
		.option(key_COPY,
			Gettext::keyname(_("COPY_K:C")),
			_("Copy"))
		.option(key_MOVE,
			Gettext::keyname(_("MOVE_K:M")),
			_("Move"))
		.option(key_TAG,
			Gettext::keyname(_("TAG_K:T")),
			_("Tag"));

	prompt=myServer::prompt(prompt);

	if (prompt.abortflag || myServer::nextScreen)
		return;

	vector<unicode_char> ka;

	mail::iconvert::convert((string)prompt, unicode_default_chset(), ka);

	if (ka.size() == 0)
		return;

	Filter::Step newStep;

	if (key_SELECT == ka[0])
	{
		bool selectAllFlag;

		newStep.type=Filter::Step::filter_step_selectsrch;

		if (!searchPrompt::prompt(newStep.searchType, selectAllFlag))
			return;

		if (selectAllFlag)
			newStep.type=Filter::Step::filter_step_selectall;

	}
	else if (key_DELETE == ka[0])
	{
		newStep.type=Filter::Step::filter_step_delete;
	}
	else if (key_UNDELETE == ka[0])
	{
		newStep.type=Filter::Step::filter_step_undelete;
	}
	else if (key_EXPUNGE == ka[0])
	{
		newStep.type=Filter::Step::filter_step_delexpunge;
	}
	else if (key_MARK == ka[0])
	{
		newStep.type=Filter::Step::filter_step_mark;
	}
	else if (key_UNMARK == ka[0])
	{
		newStep.type=Filter::Step::filter_step_unmark;
	}
	else if (key_COPY == ka[0])
	{
		doSelectFolder=true;
		selectFolderFor=Filter::Step::filter_step_copy;
		Curses::keepgoing=false;
		return;
	}
	else if (key_TAG == ka[0])
	{
		size_t n;

		if (!CursesIndexDisplay::getTag(_("Tag: "), n))
			return;

		newStep.type=Filter::Step::filter_step_tag;

		ostringstream o;

		o << n;
		newStep.name_utf8=o.str();
	}
	else if (key_MOVE == ka[0])
	{
		doSelectFolder=true;
		selectFolderFor=Filter::Step::filter_step_move;
		Curses::keepgoing=false;
		return;
	}
	else if (key_WATCH2 == ka[0])
	{
		newStep.type=Filter::Step::filter_step_watch;
	}
	else if (key_UNWATCH == ka[0])
	{
		newStep.type=Filter::Step::filter_step_unwatch;
	}

	add(newStep);
}

void Filter::editScreen::del(list<Filter::editScreen::Button *>::iterator p)
{
	if ( (*p)->isAddButton)
		return;

	Filter::editScreen::Button *button= *p;

	list<Filter::editScreen::Button *>::iterator q=p;

	++q;

	buttons.erase(p);
	delete button;

	(*q)->requestFocus();

	while (q != buttons.end())
	{
		(*q)->setRow((*q)->getRow()-1);
		++q;
	}
	saveButton.setRow(saveButton.getRow()-1);
}

void Filter::editScreen::moveup(list<Filter::editScreen::Button *>::iterator p)
{
	Button *b= *p;

	domoveup(p);
	b->requestFocus();
}

void Filter::editScreen::movedn(list<Filter::editScreen::Button *>::iterator p)
{
	Button *b= *p;

	list<Filter::editScreen::Button *>::iterator q=p;

	++q;

	if (q != buttons.end())
	{
		domoveup(q);
		b->requestFocus();
	}
}

void Filter::editScreen::domoveup(list<Filter::editScreen::Button *>::iterator
				  p)
{
	if ( (*p)->isAddButton || p == buttons.begin())
		return;

	list<Filter::editScreen::Button *>::iterator q=p;

	--q;

	Button *r= *p;

	*p=*q;
	*q=r;

	(*p)->myPos=p;
	(*q)->myPos=q;

	(*q)->setRow( (*q)->getRow()-1);
	(*p)->setRow( (*p)->getRow()+1);

	(*p)->draw();
	(*q)->draw();
}

void Filter::editScreen::add(Filter::Step &newStep)
{
	std::list<Filter::editScreen::Button *>::iterator
		addButton=--buttons.end();

	Button *newButton=new Button(this, newStep.getDescription(), false);

	newButton->step=newStep;

	if (!newButton)
		LIBMAIL_THROW(strerror(errno));

	try {
		buttons.insert(addButton, newButton);
	} catch (...) {
		delete newButton;
		throw;
	}

	saveButton.setRow(saveButton.getRow()+1);
	std::list<Filter::editScreen::Button *>::iterator p=addButton;

	newButton->myPos= --p;

	newButton->setRow((*addButton)->getRow());
	newButton->setCol((*addButton)->getCol());
	(*addButton)->setRow((*addButton)->getRow()+1);
	(*addButton)->draw();
	newButton->draw();
	(*addButton)->requestFocus();
}

bool Filter::editScreen::listKeys( vector< pair<string, string> >
				   &list)
{
	list.push_back( make_pair(Gettext::keyname(_("DELFILTER:DEL")),
				  _("Delete")));
        list.push_back( make_pair(Gettext::keyname(_("ABORT:^C")),
                                  _("Cancel")));
        list.push_back( make_pair(Gettext::keyname(_("MOVEUP:^U")),
                                  _("Move up")));
        list.push_back( make_pair(Gettext::keyname(_("MOVEDN:^D")),
                                  _("Move down")));
 	return true;
}

void Filter::editScreen::save()
{
	doSave=true;
	Curses::keepgoing=false;
}

bool Filter::doEditScreen(myServer *server,
			  std::string folderName,
			  std::string &filter,
			  Filter::Step::Type &nextType,
			  bool &doSave)
{
	Filter::editScreen escr(mainScreen);

	titleBar->setTitles( Gettext(_("%1% - FILTERS")) << folderName, "");

	escr.init(filter);
	myServer::eventloop();

	nextType=escr.selectFolderFor;
	filter=escr;

	doSave=escr.doSave;

	return escr.doSelectFolder;
}

void Filter::filterEditScreen(void *vp)
{
	Hierarchy::Folder *f=(Hierarchy::Folder *)vp;

	string folder=f->getFolder()->getPath();
	string folderName=f->getFolder()->getName();

	Hierarchy::Server *s=f->getServer();
	myServer *server;

	if (!s || !(server=s->getServer()))
	{
		myServer::nextScreen= &hierarchyScreen;
		myServer::nextScreenArg=NULL;
		return;
	}

	string filter=server->getFolderConfiguration(folder, "FILTER");

	Filter::Step::Type copyMoveType;
	bool doSave;

	while (doEditScreen(server, folderName, filter, copyMoveType,
			    doSave))
	{
		mail::folder *f;
		myServer *toServer;

		openHierarchyScreen(_("Select destination folder."),
				    NULL, &f, &toServer);

		if (myServer::nextScreen || server->server == NULL)
			break;

		if (toServer == NULL)
			continue;


		Filter::Step newStep;

		newStep.type=copyMoveType;
		newStep.toserver= server->serverName == toServer->serverName
			? string(""):toServer->serverName;
		newStep.tofolder=f->getPath();
		newStep.name_utf8= Gettext::toutf8(f->getName());

		if (newStep.toserver.size() > 0)
		{
			// TODO

			statusBar->clearstatus();
			statusBar->status(_("ERROR: This folder is from"
					    " another mail account.  Messages"
					    " may only be moved or copied"
					    " to another folder in the same"
					    " account."));
			statusBar->beepError();
		}
		else
		{
			filter += (string)newStep;
			filter += "\n";
		}
	}

	if (doSave)
	{
		server->updateFolderConfiguration(folder, "FILTER", filter);
		server->saveFolderConfiguration();
		Curses::keepgoing=false;
		myServer::nextScreen= &hierarchyScreen;
		myServer::nextScreenArg=NULL;
	}
}
