/*
** Copyright 2004-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "acl.H"
#include "gettext.H"
#include "hierarchy.H"
#include "myserver.H"
#include "myservercallback.H"
#include "myserverpromptinfo.H"
#include "curses/cursesmainscreen.H"
#include "curses/cursesdialog.H"
#include "curses/cursestitlebar.H"
#include "curses/cursesstatusbar.H"
#include "curses/cursesbutton.H"
#include "curses/cursesmultilinelabel.H"
#include "colors.H"
#include "maildir/maildiraclt.h"
#include	<errno.h>

extern CursesMainScreen *mainScreen;
extern CursesTitleBar *titleBar;
extern CursesStatusBar *statusBar;
extern void mainMenu(void *);
extern void hierarchyScreen(void *);

extern Gettext::Key key_ABORT;
extern Gettext::Key key_ADDRIGHTS;

extern Gettext::Key key_ADDREMOVE;
extern Gettext::Key key_ADDRIGHTSADMINS;
extern Gettext::Key key_ADDRIGHTSANON;
extern Gettext::Key key_ADDRIGHTSANYONE;
extern Gettext::Key key_ADDRIGHTSAUTHUSER;
extern Gettext::Key key_ADDRIGHTSGROUP;
extern Gettext::Key key_ADDRIGHTSOWNER;
extern Gettext::Key key_ADDRIGHTSUSER;

extern Gettext::Key key_DELRIGHTS;

extern struct CustomColor color_perms_owner,
	color_perms_admins,
	color_perms_user,
	color_perms_group,
	color_perms_other,
	color_perms_rights,
	color_misc_buttonColor;

#define ACL_BITS ACL_ADMINISTER ACL_CREATE ACL_EXPUNGE ACL_INSERT \
		ACL_LOOKUP ACL_POST ACL_READ ACL_SEEN ACL_DELETEMSGS \
		ACL_WRITE ACL_DELETEFOLDER

Acl::Acl(std::string codeArg)
	: code(codeArg), descr(Gettext(_("Unknown permission \"%1%\""))
			       << codeArg)
{
#define DO(a,b) if (codeArg == a) descr=b;

	DO(ACL_LOOKUP, _("Visible"));
	DO(ACL_READ,_("Open"));
	DO(ACL_SEEN, _("Change read/unread status"));
	DO(ACL_WRITE, _("Change message status"));
	DO(ACL_INSERT, _("Insert messages"));
	DO(ACL_CREATE, _("Create subfolders"));
	DO(ACL_DELETEFOLDER, _("Delete folder"));
	DO(ACL_DELETEMSGS, _("Delete/undelete messages"));
	DO(ACL_EXPUNGE, _("Expunge deleted messages"));
	DO(ACL_ADMINISTER, _("Administrator"));
	DO(ACL_POST, _("Post messages to folder"));
#undef DO
}

Acl::~Acl()
{
}

/* Return comma-separated access rights list */

std::string Acl::getDescription(std::string rights)
{
	std::string descr;
	size_t i;

	for (i=0; i<rights.size(); i++)
	{
		if (descr.size() > 0)
			descr += ", ";
		descr += Acl(rights.substr(i, 1)).getDescription();
	}
	return descr;
}

/* Format ACL2 RIGHTS-INFO reply into something a human will understand */

std::string Acl::getErrorRightsDescription(std::string errorIdentifier,
				      std::vector<std::string> &errorRights)
{
	std::string verboseDescription=
		Gettext(_("Unable to set the requested permissions for %1%. "
			  "The mail server's valid permissions are"
			  " restricted as follows:\n\n"))
				  <<
		Gettext::fromutf8(errorIdentifier);

	if (errorRights.size() > 0 && errorRights[0].size() > 0)
	{
		verboseDescription +=
			Gettext(_("Minimum permissions: %1%.\n"))
				<<
			Acl::getDescription(errorRights[0]);
	}

	std::string singleRights;

	std::vector<std::string>::iterator
		b=errorRights.begin(), e=errorRights.end();

	if (b != e)
		++b;

	while (b != e)
	{
		if (b->size() == 1)
		{
			singleRights += *b;
		}
		else
		{
			verboseDescription +=
				Gettext(_("\"%1%\" may be set only all together as a group.\n"))
					<<
				Acl::getDescription(*b);
		}
		++b;
	}

	if (singleRights.size() > 0)
		verboseDescription +=
			Gettext(_("\"%1%\" may be set in any combination.\n"))
				<<
			Acl::getDescription(singleRights);

	return verboseDescription;
}

/* Screen: display ACLs for editing */

class Acl::EditScreen : public CursesDialog,
	   public CursesKeyHandler {

	Hierarchy::Folder *folder;

	/* Subclass CursesButton to go to edit screen for individual right */

	class EntryButton : public CursesButton {
		Acl::EditScreen *parent;
		std::string identifier;
		std::string rights;

	public:
		EntryButton(Acl::EditScreen *parentArg,
			    std::string label,
			    std::string identifierArg,
			    std::string rightsArg);
		~EntryButton();
		void clicked();
		bool processKeyInFocus(const Curses::Key &key);
	};

	class Entry {
	public:
		std::string identifierStr, rightsStr;

		EntryButton identifier;
		CursesMultilineLabel rights;
		int identifier_w;

		void setRow(int row)
		{
			identifier.setRow(row);
			rights.setRow(row);
		}
		int getHeight()
		{
			return rights.getHeight();
		}
		void resized();

		Entry(Acl::EditScreen *parent,
		      std::string identifierArg, std::string rightsArg);
		~Entry();
	};

	CursesButtonRedirect<Acl::EditScreen> *saveButton;

	std::vector<Entry *> entries;

	void clearrights();
	void setrights(	std::list<std::pair<std::string, std::string> > &);
public:
	EditScreen(Hierarchy::Folder *folderArg);
	~EditScreen();

	void init();
	void resized();

	// Inherited from CursesKeyHandler:

	bool processKey(const Curses::Key &key);
	bool listKeys( std::vector< std::pair<std::string, std::string> > &list);

	bool goEditIdentifier; // true: go to edit identifier screen
	std::string editIdentifier;
	std::string editRights;

	void deleteIdentifier(std::string identifier);

	void doSave();
};


/* Display screen for editing individual rights */

class Acl::RightsScreen : public CursesContainer,
	   public CursesKeyHandler {

	CursesButtonRedirect<Acl::RightsScreen> saveButton;

	mail::folder *folder;
	std::string identifier;
	std::string origRights;

	std::vector<CursesButton *> buttons;

public:
	RightsScreen(mail::folder *folderArg,
		     std::string identifierArg, std::string origRightsArg);
	~RightsScreen();
	void init();

	// Inherited from CursesKeyHandler:

	bool processKey(const Curses::Key &key);
	bool listKeys( std::vector< std::pair<std::string, std::string> > &list);

	bool rightsUpdated;
private:
	void doSave();
};



Acl::EditScreen::Entry::Entry(Acl::EditScreen *parent,
			      std::string identifierArg,
			      std::string rightsArg)
	: identifierStr(identifierArg),
	  rightsStr(rightsArg),
	  identifier(parent, Gettext::fromutf8(identifierArg),
		     identifierArg, rightsArg),
	  rights(parent, Acl::getDescription(rightsArg)),
	  identifier_w(10)
{
	Curses::CursesAttr attr;

	int color=
		identifierStr == "owner" ? color_perms_owner.fcolor:
		identifierStr == "administrators" ? color_perms_admins.fcolor :
		strncmp(identifierStr.c_str(), "user=", 5) == 0
		? color_perms_user.fcolor :
		strncmp(identifierStr.c_str(), "group=", 6) == 0
		? color_perms_group.fcolor:color_perms_other.fcolor;

	attr.setFgColor(color);

	identifier.setAttribute(attr);
	rights.setAttribute(attr);
	rights.init();
	resized();
}

void Acl::EditScreen::Entry::resized()
{
	int s=identifier_w+1;
	rights.setCol(s);

	int w=rights.getScreenWidth();

	rights.setWidth(w > s+10 ? w-s:10);
}

Acl::EditScreen::Entry::~Entry()
{
}


Acl::EditScreen::EntryButton::EntryButton(Acl::EditScreen *parentArg,
					  std::string label,
					  std::string identifierArg,
					  std::string rightsArg)
	: CursesButton(parentArg, label),
	  parent(parentArg),
	  identifier(identifierArg),
	  rights(rightsArg)
{
}

Acl::EditScreen::EntryButton::~EntryButton()
{
}

bool Acl::EditScreen::EntryButton::processKeyInFocus(const Curses::Key &key)
{
	if (key == key_DELRIGHTS)
	{
		myServer::promptInfo prompt=
			myServer::promptInfo(Gettext(_("Remove permissions"
						       " for \"%1%\"?"
						       " (Y/N) "))
					     <<
					     Gettext::fromutf8(identifier))
			.yesno();

		prompt=myServer::prompt(prompt);

		if (prompt.abortflag ||
		    (std::string)prompt != "Y")
			return true;

		parent->deleteIdentifier(identifier);
		return true;
	}

	return CursesButton::processKeyInFocus(key);
}
void Acl::EditScreen::EntryButton::clicked()
{
	parent->goEditIdentifier=true;
	parent->editIdentifier=identifier;
	parent->editRights=rights;
	Curses::keepgoing=false;
}

Acl::EditScreen::EditScreen(Hierarchy::Folder *folderArg)
	: CursesDialog(mainScreen),
	  CursesKeyHandler(PRI_SCREENHANDLER),
	  folder(folderArg),
	  saveButton(NULL),
	  goEditIdentifier(false)
{
}

Acl::EditScreen::~EditScreen()
{
	clearrights();
	if (saveButton)
		delete saveButton;
}

void Acl::EditScreen::setrights( std::list<std::pair<std::string, std::string> > &rights)
{
	clearrights();

	std::list<std::pair<std::string, std::string> >::iterator b=rights.begin(), e=rights.end();
	int row=1;

	size_t maxw=10;

	while (b != e)
	{
		Acl::EditScreen::Entry *p=
			new Acl::EditScreen::Entry(this, b->first,
						   b->second);

		try
		{
			entries.push_back(p);
		} catch (...)
		{
			delete p;
			throw;
		}
		size_t w=p->identifier.getWidth();
		if (w > maxw)
			maxw=w;
		++b;
	}

	std::vector<Entry *>::iterator bb=entries.begin(), ee=entries.end();

	while (bb != ee)
	{
		(*bb)->identifier_w=(int)maxw;
		(*bb)->resized();
		(*bb)->setRow(row);
		row += (*bb)->getHeight()+1;
		++bb;
	}

	if (saveButton == NULL)
	{
		saveButton=
			new CursesButtonRedirect<Acl::EditScreen>
			(this, _("Done"));

		if (!saveButton)
			throw strerror(errno);

		(*saveButton)=this;
		(*saveButton)= &Acl::EditScreen::doSave;

		Curses::CursesAttr attr;

		attr.setFgColor(color_misc_buttonColor.fcolor);
		saveButton->setAttribute(attr);
	}

	saveButton->setRow(row+1);
	saveButton->setCol(maxw);
}

void Acl::EditScreen::clearrights()
{
	std::vector<Entry *>::iterator b, e;

	b=entries.begin();
	e=entries.end();
	while (b != e)
	{
		delete *b;
		++b;
	}
	entries.clear();
}

void Acl::EditScreen::init()
{
	titleBar->setTitles(folder->getFolder()->getName(),
			    _("PERMISSIONS"));

	statusBar->clearstatus();
	statusBar->status(_("Reading folder permissions..."));

	std::list<std::pair<std::string, std::string> > rights;

	myServer::Callback callback;

	folder->getFolder()->getRights(callback, rights);

#if 0
	rights.push_back(make_pair(std::string("anyone"), std::string("lr")));
	rights.push_back(make_pair(std::string("user=fred"), std::string("lr")));
#endif
	if (!myServer::eventloop(callback))
		return;
	setrights(rights);

	if (entries.size() > 0)
		entries[0]->identifier.requestFocus();
	else
		saveButton->requestFocus();
}

void Acl::EditScreen::resized()
{
	std::vector<Entry *>::iterator b, e;

	b=entries.begin();
	e=entries.end();

	int row=1;

	while (b != e)
	{
		(*b)->resized();
		(*b)->setRow(row);
		row += (*b)->getHeight()+1;
		++b;
	}
	erase();
	draw();
}

void Acl::EditScreen::doSave()
{
	keepgoing=false;
	myServer::nextScreen= &hierarchyScreen;
}

bool Acl::EditScreen::processKey(const Curses::Key &key)
{
	if (key == key_ABORT)
	{
		keepgoing=false;
		myServer::nextScreen= &hierarchyScreen;
		return true;
	}

	if (key == key_ADDRIGHTS)
	{
		bool negativeRights=false;

		std::string identifier;

		for (;;)
		{
			myServer::promptInfo prompt=
				myServer::promptInfo(negativeRights ?
						     _("Remove rights from: ")
						     :
						     _("Add rights to: "))
				.option(key_ADDREMOVE,
					_("-"),
					_("Add/remove"))
				.option(key_ADDRIGHTSANYONE,
					_("1"),
					_("anyone"))
				.option(key_ADDRIGHTSANON,
					_("2"),
					_("anonymous"))
				.option(key_ADDRIGHTSAUTHUSER,
					_("3"),
					_("authuser"))
				.option(key_ADDRIGHTSOWNER,
					_("O"),
					_("owner"))
				.option(key_ADDRIGHTSADMINS,
					_("A"),
					_("admins"))
				.option(key_ADDRIGHTSUSER,
					_("U"),
					_("user"))
				.option(key_ADDRIGHTSGROUP,
					_("G"),
					_("group"));

			prompt=myServer::prompt(prompt);

			if (prompt.abortflag)
				return true;

			unicode_char promptKey=prompt.firstChar();

			if (promptKey == 0)
				return true;

			if (key_ADDREMOVE == promptKey)
			{
				negativeRights= !negativeRights;
				continue;
			}

			if (key_ADDRIGHTSANYONE == promptKey)
			{
				identifier="anyone";
				break;
			}

			if (key_ADDRIGHTSANON == promptKey)
			{
				identifier="anonymous";
				break;
			}

			if (key_ADDRIGHTSAUTHUSER == promptKey)
			{
				identifier="authuser";
				break;
			}

			if (key_ADDRIGHTSOWNER == promptKey)
			{
				identifier="owner";
				break;
			}

			if (key_ADDRIGHTSADMINS == promptKey)
			{
				identifier="administrators";
				break;
			}

			if (key_ADDRIGHTSUSER == promptKey)
			{
				prompt=myServer::promptInfo(_("User: "));
				prompt=myServer::prompt(prompt);


				if (prompt.abortflag)
					return true;

				identifier="user=" + Gettext::toutf8(prompt);
				break;
			}
					
			if (key_ADDRIGHTSGROUP == promptKey)
			{
				prompt=myServer::promptInfo(_("Group: "));
				prompt=myServer::prompt(prompt);


				if (prompt.abortflag)
					return true;

				identifier="group=" + Gettext::toutf8(prompt);
				break;
			}
		}

		if (negativeRights)
			identifier="-" + identifier;

		editIdentifier=identifier;
		editRights="";
		goEditIdentifier=true;
		Curses::keepgoing=false;
		return true;
	}
	return false;
}

bool Acl::EditScreen::listKeys( std::vector< std::pair<std::string, std::string> > &list)
{
        list.push_back( std::make_pair(Gettext::keyname(_("ADDRIGHTS:A")),
				       _("Add")));
        list.push_back( std::make_pair(Gettext::keyname(_("DELRIGHTS:D")),
				       _("Delete")));
	return true;
}

void Acl::editScreen(void *voidArg)
{
	for (;;)
	{
		std::string identifier;
		std::string rights;

		{
			Acl::EditScreen scrn((Hierarchy::Folder *)voidArg);

			scrn.init();
			myServer::eventloop();

			if (!scrn.goEditIdentifier)
				break;

			identifier=scrn.editIdentifier;
			rights=scrn.editRights;
		}

		{
			Acl::RightsScreen scrn(((Hierarchy::Folder *)voidArg)
					       ->getFolder(),
					       identifier, rights);

			scrn.init();
			myServer::eventloop();
			if (!scrn.rightsUpdated)
				break;
		}
	}
}


void Acl::EditScreen::deleteIdentifier(std::string identifier)
{
	std::string errorIdentifier;
	std::vector<std::string> errorRights;
	myServer::Callback cb;

	statusBar->clearstatus();
	statusBar->status(_("Updating folder permissions..."));

	folder->getFolder()->delRights(cb, errorIdentifier, errorRights,
				       identifier);

	if (!myServer::eventloop(cb))
	{
		if (errorIdentifier.size() > 0)
		{
			statusBar->clearstatus();
			statusBar->status(Acl::getErrorRightsDescription
					  (errorIdentifier,
					   errorRights));
		}
		return;
	}

	Curses::keepgoing=false;
	myServer::nextScreen= &Acl::editScreen;
	myServer::nextScreenArg=folder;
}


Acl::RightsScreen::RightsScreen(mail::folder *folderArg,
				std::string identifierArg, std::string origRightsArg)
	: CursesContainer(mainScreen),
	  CursesKeyHandler(PRI_SCREENHANDLER),
	  saveButton(this, _("Save")),
	  folder(folderArg),
	  identifier(identifierArg),
	  origRights(origRightsArg),
	  rightsUpdated(false)
{
	saveButton=this;
	saveButton= &Acl::RightsScreen::doSave;

	Curses::CursesAttr attr;

	attr.setFgColor(color_misc_buttonColor.fcolor);
	saveButton.setAttribute(attr);

}

Acl::RightsScreen::~RightsScreen()
{
	std::vector<CursesButton *>::iterator
		b=buttons.begin(),
		e=buttons.end();

	while (b != e)
	{
		delete *b;
		++b;
	}
}

void Acl::RightsScreen::init()
{
	size_t i;
	char c[2];

	std::string s=Gettext::fromutf8(identifier);

	titleBar->setTitles(s,_("PERMISSIONS"));

	Curses::CursesAttr attr;

	attr.setFgColor(color_perms_rights.fcolor);

	for (i=0; (c[0]=ACL_BITS[i]) != 0; i++)
	{
		c[1]=0;
		CursesButton *b=new CursesButton(this,
						 Acl(c).getDescription(),
						 origRights.find(c[0])
						 != std::string::npos ? -1:1);
		if (!b)
			throw strerror(errno);

		try {
			buttons.push_back(b);
		} catch (...) {
			delete b;
			throw;
		}

		b->setRow(i+2);
		b->setCol(2);
		b->setAttribute(attr);
	}

	saveButton.setRow(i+4);
	saveButton.setCol(4);
	buttons[0]->requestFocus();
}

bool Acl::RightsScreen::processKey(const Curses::Key &key)
{
	if (key == key_ABORT)
	{
		Curses::keepgoing=false;
		rightsUpdated=true;
		return true;
	}
	return false;
}

bool Acl::RightsScreen::listKeys( std::vector< std::pair<std::string,
							 std::string> > &list)
{
        list.push_back( std::make_pair(Gettext::keyname(_("ABORT:^C")),
				       _("Cancel")));
	return true;
}

void Acl::RightsScreen::doSave()
{
	/* Ok, figure out what happened */
	size_t i;
	char c[2];

	std::string saveRights=origRights;
	std::string rightsAdded;
	std::string rightsRemoved;

	for (i=0; (c[0]=ACL_BITS[i]) != 0; i++)
	{
		c[1]=0;

		if (buttons[i]->getSelected())
		{
			if (origRights.find(c[0]) != std::string::npos)
				continue; /* Still there */

			rightsAdded += c;
			origRights += c;
		}
		else
		{
			size_t p=origRights.find(c[0]);

			if (p == std::string::npos)
				continue; /* Still not there */

			rightsRemoved += c;
			origRights.erase(origRights.begin() + p,
					 origRights.begin() + p + 1);
		}
	}

	if (rightsAdded.size() > 0 || rightsRemoved.size() > 0)
	{
		if (rightsAdded.size() && rightsRemoved.size())
			rightsAdded=origRights; /* Updated rights */
		else
		{
			/* Something was added or removed, exclusively */

			if (rightsRemoved.size() > 0)
				rightsAdded="-";
			else
			{
				rightsRemoved=rightsAdded;
				rightsAdded="+";
			}
			rightsAdded += rightsRemoved;
		}

		statusBar->clearstatus();
		statusBar->status(_("Updating folder permissions..."));

		origRights=saveRights;

		std::string errorIdentifier;
		std::vector<std::string> errorRights;

		myServer::Callback cb;

		folder->setRights(cb, errorIdentifier,
				  errorRights,
				  identifier, rightsAdded);

		if (!myServer::eventloop(cb))
		{
			if (errorIdentifier.size() > 0)
			{
				statusBar->clearstatus();
				statusBar->
					status(Acl::getErrorRightsDescription
					       (errorIdentifier,
						errorRights));
			}
			return;
		}
	}

	Curses::keepgoing=false;
	rightsUpdated=true;
}
