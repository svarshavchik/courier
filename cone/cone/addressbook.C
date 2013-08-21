/*
** Copyright 2003-2009, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include "unicode/unicode.h"

#include "curses/cursesscreen.H"
#include "curses/cursesbutton.H"
#include "curses/cursestitlebar.H"
#include "curses/cursesstatusbar.H"
#include "curses/cursesmainscreen.H"
#include "curses/cursesdialog.H"
#include "curses/curseslabel.H"
#include "curses/cursesfield.H"
#include "curses/curseskeyhandler.H"

#include "gettext.H"
#include "myserver.H"
#include "globalkeys.H"
#include "opendialog.H"
#include "savedialog.H"
#include "myservercallback.H"
#include "myserverpromptinfo.H"
#include "previousscreen.H"

#include "libmail/rfcaddr.H"
#include "libmail/misc.H"

#include "opensubfolders.H"
#include "addressbook.H"

#include "libmail/logininfo.H"
#include <fstream>
#include <vector>
#include <sstream>
#include <list>
#include <set>
#include <map>
#include <algorithm>

#include "addressbookinterfacemail.H"
#include "addressbookinterfaceldap.H"

using namespace std;

extern Gettext::Key key_DELADDRESSBOOKENTRY;
extern Gettext::Key key_RENAMEADDRESSBOOKENTRY;
extern Gettext::Key key_IMPORTADDRESSBOOKENTRY;
extern Gettext::Key key_EXPORTADDRESSBOOKENTRY;
extern Gettext::Key key_ADDADDRESSBOOK;
extern Gettext::Key key_DELETEADDRESSBOOK;
extern Gettext::Key key_ABORT;
extern Gettext::Key key_UP;
extern Gettext::Key key_DOWN;
extern Gettext::Key key_ADDLDAP;

extern CursesScreen *cursesScreen;
extern CursesMainScreen *mainScreen;
extern CursesTitleBar *titleBar;
extern CursesStatusBar *statusBar;

extern void mainMenu(void *);

void listaddressbookScreenImport(void *vp);
void addressbookIndexScreen(void *);
void addLdapScreen(void *);

list<AddressBook *> AddressBook::addressBookList;
string AddressBook::importName;
string AddressBook::importAddr;

AddressBook *AddressBook::defaultAddressBook=NULL;

AddressBook::AddressBook()
	: myInterface(NULL),
	  iwantFocus(false)
{
}

AddressBook::~AddressBook()
{
	if (myInterface)
	{
		Interface *i=myInterface;

		myInterface=NULL;
		delete i;
	}
}

class AddressBook::disconnect : public mail::callback::disconnect {
public:
	disconnect();
	~disconnect();
	void disconnected(const char *errmsg);
	void servererror(const char *errmsg);
};


AddressBook::disconnect::disconnect()
{
}

AddressBook::disconnect::~disconnect()
{
}

void AddressBook::disconnect::disconnected(const char *errmsg)
{
}

void AddressBook::disconnect::servererror(const char *errmsg)
{
}

void AddressBook::init()
{
	// Open the default address book first.

	string afile=myServer::getConfigDir()+ "/addressbook";

	int fd= ::open(afile.c_str(), O_RDWR|O_CREAT, 0600);

	if (fd >= 0)
		::close(fd);

	if (defaultAddressBook)
		return; // Already initialized.

#if HAVE_OPENLDAP

	const char *ldap_auto=getenv("CONE_AUTOLDAP");

	if (ldap_auto && *ldap_auto)
	{
		std::string name=ldap_auto;
		std::string server;
		std::string suffix;

		size_t p=name.find('=');

		if (p != std::string::npos)
		{
			server=name.substr(p+1);
			name=name.substr(0, p);

			p=server.find('/');

			if (p != std::string::npos)
			{
				suffix=server.substr(p+1);
				server=server.substr(0, p);
			}
			std::list<AddressBook *>::iterator
				b=addressBookList.begin(),
				e=addressBookList.end();

			while (b != e)
			{
				if ((*b)->name == name)
					break;
				++b;
			}

			if (b == e)
			{
				AddressBook *abook=new AddressBook();

				if (!abook)
					outofmemory();

				try {
					abook->init(name,
						    "ldap://" + server,
						    suffix);

					AddressBook::addressBookList
						.insert(AddressBook::
							addressBookList.end(),
							abook);
				} catch (...) {
					delete abook;
					LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
				}

				myServer::saveconfig();
			}
		}
	}
#endif


	defaultAddressBook=new AddressBook;

	if (!defaultAddressBook)
		outofmemory();

	try {
		string url="mbox:" + afile;

		myServer::Callback login_callback;

		mail::account::openInfo loginInfo;

		loginInfo.url=url;

		disconnect temp_disconnect;

		mail::account *abook=mail::account::open(loginInfo,
							 login_callback,
							 temp_disconnect);

		if (!abook)
			outofmemory();

		// Need to do some song-n-dance with LibMAIL in order to
		// initialize the default address book folder.

		try {
			if (!myServer::eventloop(login_callback))
			{
				delete abook;
				delete defaultAddressBook;
				return;
			}

			OpenSubFoldersCallback readFolders;
			myServer::Callback readfolders_callback;

			abook->readTopLevelFolders(readFolders,
						   readfolders_callback);
			if (!myServer::eventloop(login_callback))
			{
				delete abook;
				delete defaultAddressBook;
				return;
			}

			if (readFolders.folders.size() > 0)
			{
				defaultAddressBook->init(_("Local Address Book"),
							 url,
							 readFolders
							 .folders[0]
							 ->toString());

				myServer::Callback logout_callback;

				abook->logout(logout_callback);
				myServer::eventloop(logout_callback);

				delete abook;
				abook=NULL;
			}
			else
			{
				delete defaultAddressBook;
				defaultAddressBook=NULL;
			}

		} catch (...) {
			if (abook)
				delete abook;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}
	} catch (...) {
		delete defaultAddressBook;
		defaultAddressBook=NULL;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

void AddressBook::init(string nameArg,
		       string urlArg,
		       string folderStrArg)
{
	name=nameArg;
	url=urlArg;
	folderStr=folderStrArg;
}

bool AddressBook::open()
{
	if (myInterface && myInterface->closed())
		close();

	if (myInterface)
		return true;

	if (url.substr(0, 7) == "ldap://")
	{
#if HAVE_OPENLDAP
		Interface::LDAP *i=new Interface::LDAP(name);

		if (!i)
			outofmemory();

		try {
			if (i->openUrl(url, folderStr))
			{
				myInterface=i;
				return true;
			}
		} catch (...)
		{
			delete i;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}
		delete i;
#endif
	}
	else
	{
		Interface::Mail *i=new Interface::Mail(name);

		if (!i)
			outofmemory();

		try {
			if (i->open(url, folderStr))
			{
				myInterface=i;
				return true;
			}
		} catch (...)
		{
			delete i;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}
		delete i;
	}
	return false;
}

void AddressBook::closeAllRemote()
{
	list<AddressBook *>::iterator b, e;

	b=addressBookList.begin();
	e=addressBookList.end();

	while (b != e)
	{
		AddressBook *p= *b;

		p->close();
		b++;
	}
}

void AddressBook::closeAll()
{
	if (defaultAddressBook)
	{
		defaultAddressBook->close();
		delete defaultAddressBook;
		defaultAddressBook=NULL;
	}

	list<AddressBook *>::iterator b, e;

	while ((b=addressBookList.begin()) != (e=addressBookList.end()))
	{
		AddressBook *p= *b;

		addressBookList.erase(b);

		try {
			p->close();
			delete p;
		} catch (...) {
			delete p;
		}
	}
}

void AddressBook::close()
{
	Interface *i=myInterface;

	myInterface=NULL;

	try {
		if (i)
			i->close();
	} catch (...)
	{
		delete i;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	delete i;
}

// Add a new address book entry.

bool AddressBook::add(mail::addressbook::Entry &newEntry)
{
	if (!open())
		return false;

	return myInterface->add(newEntry,
				currentRecord.second); // Old UID, if not empty
}

bool AddressBook::import(list<mail::addressbook::Entry> &newList)
{
	if (!open())
		return false;

	return myInterface->import(newList);
}

// Delete an address book entry

bool AddressBook::del(string uid)
{
	if (!open())
		return false;

	return myInterface->del(uid);
}

// Rename an address book entry

bool AddressBook::rename(std::string uid,
			 std::string newnickname)
{
	if (!open())
		return false;

	return myInterface->rename(uid, newnickname);
}

// Search the address book for a specific nickname.
bool AddressBook::searchNickname(std::string nickname,
				 std::vector<mail::emailAddress> &addrListArg)
{
	if (!open())
		return false;

	return myInterface->searchNickname(nickname, addrListArg);
}

// List address book contents.

void AddressBook::getIndex( std::list< std::pair<std::string,
			    std::string> > &listArg)
{
	if (!open())
		return;

	return myInterface->getIndex(listArg);
}

//
// Read the current address book entry into addrList:

bool AddressBook::getEntry(std::vector<mail::emailAddress> &addrList)
{
	if (!open())
		return true;

	return myInterface->getEntry(currentRecord.second, addrList);
}

bool AddressBook::getEntry(std::string uid,
			   std::vector<mail::emailAddress> &addrList)
{
	if (!open())
		return true;

	return myInterface->getEntry(uid, addrList);
}

//
// Replace addresses in the address list with anything found in
// the address books.

bool AddressBook::searchAll(vector<mail::emailAddress> &addrList, // Orig list
			    vector<mail::emailAddress> &retList) // Final list
{
	set<string> addrSet; // Eliminate dupes

	vector<mail::emailAddress>::iterator curAddr=addrList.begin(),
		lastAddr=addrList.end();

	for ( ; curAddr != lastAddr; curAddr++)
	{
		string s=curAddr->getAddr();

		if (s.find('@') != std::string::npos)
		{
			// Not a nickname, a dupe?

			if (addrSet.count( s ) == 0)
			{
				addrSet.insert(s);
				retList.push_back( *curAddr);
			}
			continue;
		}

		// Go through each address book, see what we can find.

		list<AddressBook *>::iterator ab=addressBookList.begin(),
			ae=addressBookList.end();

		vector<mail::emailAddress> addresses;

		if (defaultAddressBook &&
		    !defaultAddressBook->searchNickname(curAddr->getAddr(),
							addresses))
			return false;

		while (ab != ae && addresses.size() == 0)
		{
			if (! (*ab++)->searchNickname(curAddr->getAddr(), addresses))
				return false;
		}

		if (addresses.size() == 0) // Not found, check the pw file
		{
			mail::emailAddress addr= *curAddr;

			if (addr.getDisplayName("utf-8").size() == 0)
			{
				struct passwd *pw=getpwnam(addr.getAddr()
							   .c_str());

				if (pw != NULL)
				{
					string nn=pw->pw_gecos;

					size_t n=nn.find(',');

					if (n != std::string::npos)
						nn=nn.substr(0, n);

					addr.setDisplayName(nn,
							    unicode_default_chset());
				}
			}

			addr.setAddr(addr.getAddr() + "@"
				     + mail::hostname());

			addresses.push_back(addr);
		}

		// Found addresses, check for dupes, add them.

		vector<mail::emailAddress>::iterator addrb=addresses.begin(),
			addre=addresses.end();

		while (addrb != addre)
		{
			if (addrSet.count( addrb->getAddr() ) == 0)
			{
				addrSet.insert(addrb->getAddr());
				retList.push_back( *addrb );
			}

			++addrb;
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////
//
// Add new address book entry screen.

class AddAddressBookScreen : public CursesContainer,
			     public CursesKeyHandler {

	AddressBook *addressBook;
	CursesLabel title;
	CursesDialog addDialog;

	CursesLabel nickname_label;
	CursesLabel name_label;
	CursesLabel addr_label;

	CursesField nickname_field;
	CursesField name_field;
	CursesField addr_field;

	CursesButtonRedirect<AddAddressBookScreen> saveButton, cancelButton;

	void save();
	void cancel();

	bool processKey(const Curses::Key &key);

public:
	void requestFocus()
	{
		nickname_field.requestFocus();
	}

	AddAddressBookScreen(CursesMainScreen *,
			     AddressBook *);
	~AddAddressBookScreen();
};

AddAddressBookScreen::AddAddressBookScreen(CursesMainScreen *mainScreen,
					   AddressBook *addressBookArg)
	: CursesContainer(mainScreen),
	  CursesKeyHandler(PRI_SCREENHANDLER),
	  addressBook(addressBookArg),
	  title(this, _("Add Entry")),
	  addDialog(this),
	  nickname_label(NULL, _("Nickname: ")),
	  name_label(NULL, _("Name: ")),
	  addr_label(NULL, _("Address: ")),

	  nickname_field(NULL),
	  name_field(NULL),
	  addr_field(NULL),

	  saveButton(this, _("SAVE")),
	  cancelButton(this, _("CANCEL"))
{
	setRow(4);
	setAlignment(Curses::PARENTCENTER);

	title.setAlignment(Curses::PARENTCENTER);

	addDialog.setRow(2);

	addDialog.addPrompt(&nickname_label, &nickname_field, 0);
	addDialog.addPrompt(&name_label, &name_field, 1);
	addDialog.addPrompt(&addr_label, &addr_field, 2);

	titleBar->setTitles(_("ADD ENTRY"), "");

	saveButton=this;
	cancelButton=this;

	saveButton= &AddAddressBookScreen::save;
	cancelButton= &AddAddressBookScreen::cancel;

	addDialog.addPrompt(NULL, &saveButton, 4);
	addDialog.addPrompt(NULL, &cancelButton, 6);

	vector<mail::emailAddress> addresses;

	if (addressBook->currentRecord.second.size() > 0) // Edit exist rec
	{
		nickname_field.setText(addressBook->currentRecord.first);

		if (!addressBook->getEntry(addresses))
			addresses.clear();
	}

	// Get any defaults from the take addresses screen.

	if (AddressBook::importAddr.size() > 0)
	{
		mail::emailAddress addr;

		addr.setDisplayName(AddressBook::importName,
				    unicode_default_chset());
		addr.setDisplayAddr(AddressBook::importAddr,
				    unicode_default_chset());

		addresses.push_back(addr);
	}
		
	if (addresses.size() == 1)
	{
		name_field.setText(addresses[0]
				   .getDisplayName(unicode_default_chset()));
		addresses[0].setDisplayName("", "utf-8");
	}

	// We want to decode MIME encoded addresses.  Hack.

	vector<mail::address> decodedAddresses;

	decodedAddresses.reserve(addresses.size());

	{
		vector<mail::emailAddress>::iterator b, e;

		for (b=addresses.begin(), e=addresses.end(); b != e; ++b)
			decodedAddresses
				.push_back(mail::address(b->getDisplayName(unicode_default_chset()),
							 b->getDisplayAddr(unicode_default_chset())));
	}

	string s=mail::address::toString("", decodedAddresses, 0);

	string::iterator b=s.begin(), e=s.end();

	while (b != e)
	{
		if (*b == '\n')
			*b=' ';
		b++;
	}
	addr_field.setText(s);
	name_field.requestFocus();
}

bool AddAddressBookScreen::processKey(const Curses::Key &key)
{
	if (key == key_ABORT)
	{
		keepgoing=false;
		myServer::nextScreen= &mainMenu;
		myServer::nextScreenArg=NULL;
		PreviousScreen::previousScreen();
		return true;
	}
	return false;
}

AddAddressBookScreen::~AddAddressBookScreen()
{
}

void AddAddressBookScreen::save()
{
	size_t errindex;

	mail::addressbook::Entry newEntry;

	{
		vector<mail::address> addrBuf;

		if (!mail::address::fromString(addr_field.getText(),
					       addrBuf, errindex))
		{
			addr_field.requestFocus();
			addr_field.setCursorPos(errindex);
			addr_field.beepError();
			return;
		}

		// entered addresses use the native character set, convert
		// to MIME encoding using this hack

		newEntry.addresses.clear();
		newEntry.addresses.reserve(addrBuf.size());

		vector<mail::address>::iterator b, e;

		for (b=addrBuf.begin(), e=addrBuf.end(); b != e; ++b)
		{
			mail::emailAddress addr;

			std::string errmsg;

			errmsg=addr.setDisplayName(b->getName(),
						   unicode_default_chset());

			if (errmsg == "")
				errmsg=addr
					.setDisplayAddr(b->getAddr(),
							unicode_default_chset()
							);

			if (errmsg != "")
			{
				addr_field.requestFocus();
				statusBar->clearstatus();
				statusBar->status(errmsg,
						  statusBar->SYSERROR);
				statusBar->beepError();
				return;
			}
			newEntry.addresses.push_back(addr);
		}
	}

	if (newEntry.addresses.size() == 0)
	{
		addr_field.requestFocus();
		addr_field.beepError();
		return;
	}

	newEntry.nickname=nickname_field.getText();

	if (newEntry.addresses.size() == 1 &&
	    newEntry.addresses[0].getName().size() == 0)
	{
		newEntry.addresses[0].setDisplayName(name_field.getText(),
						     unicode_default_chset());
	}

	if (addressBook->add(newEntry))
	{
		keepgoing=false;
		myServer::nextScreen= &addressbookIndexScreen;
		myServer::nextScreenArg=addressBook;
		return;
	}
}

void AddAddressBookScreen::cancel()
{
	keepgoing=false;
	myServer::nextScreen= &mainMenu;
	myServer::nextScreenArg=NULL;
	PreviousScreen::previousScreen();
}

void addressbookScreen(void *vp)
{
	AddressBook *a=(AddressBook *)vp;

	AddAddressBookScreen screen(mainScreen, a);

	screen.requestFocus();
	myServer::eventloop();
}

/////////////////////////////////////////////////////////////////////////////
//
// Address book Index
//

class AddressBookIndexScreen : public CursesContainer,
			       public CursesKeyHandler {

	AddressBook *addressBook;
	CursesLabel title;
	CursesDialog indexDialog;

	class indexButton : public CursesButton {
		AddressBookIndexScreen *myScreen;
		pair<string, string> myEntry;
		// first: nickname, second: uid

		bool processKeyInFocus(const Key &key);

	public:
		size_t indexNum;

		indexButton(AddressBookIndexScreen *myScreenArg,
			    pair<string, string> myEntryArg);
		~indexButton();

		void clicked();
	};

	vector<indexButton *> buttons;

	class indexSort {
	public:
		bool operator()(const pair<string, string> &a,
				const pair<string, string> &b);
	};

	bool processKey(const Curses::Key &key);
	bool listKeys( vector< pair<string,  string> > &list);
public:
	void requestFocus()
	{
		if (buttons.size() > 0)
			buttons[0]->requestFocus();
	}

	AddressBookIndexScreen(CursesMainScreen *, AddressBook *,
			       vector< pair<string, string> > &index);
	~AddressBookIndexScreen();

	void clicked( pair<string, string> entryArg);
	void deleteEntry(size_t n, string uid);
	void renameEntry(size_t n, string uid, string newnickname);
};

AddressBookIndexScreen::indexButton::indexButton(AddressBookIndexScreen
						 *myScreenArg,
						 pair<string, string>
						 myEntryArg)
	: CursesButton(myScreenArg, myEntryArg.first),
	  myScreen(myScreenArg),
	  myEntry(myEntryArg),
	  indexNum(0)
{
	setStyle(MENU);
}

AddressBookIndexScreen::indexButton::~indexButton()
{
}

void AddressBookIndexScreen::indexButton::clicked()
{
	myScreen->clicked(myEntry);
}

bool AddressBookIndexScreen::indexButton::processKeyInFocus(const Key &key)
{
	if (key == key_DELADDRESSBOOKENTRY)
	{
		myServer::promptInfo response=
			myServer
			::promptInfo(Gettext(_("Delete address book entry for '%1%'? (Y/N) "))
				     << myEntry.first).yesno();

		response=myServer::prompt(response);

		if (response.aborted())
			return true;

		if ((string)response == "Y")
			myScreen->deleteEntry(indexNum, myEntry.second);

		return true;
	}

	if (key == key_RENAMEADDRESSBOOKENTRY)
	{
		myServer::promptInfo response=
			myServer
			::promptInfo(_("New nickname (CTRL-C - cancel: "));

		response=myServer::prompt(response);

		if (response.aborted())
			return true;

		std::string n=response;

		if (n.size() == 0)
			return true;

		myScreen->renameEntry(indexNum, myEntry.second, n);
		return true;
	}

	return CursesButton::processKeyInFocus(key);
}

AddressBookIndexScreen::AddressBookIndexScreen(CursesMainScreen *parent,
					       AddressBook *addressBookArg,
					       vector< pair<string, string> >
					       &indexArg)
	: CursesContainer(parent),
	  CursesKeyHandler(PRI_SCREENHANDLER),
	  addressBook(addressBookArg),
	  title(this, addressBookArg->getName()),
	  indexDialog(this)
{
	setRow(4);
	setAlignment(Curses::PARENTCENTER);
	title.setAlignment(Curses::PARENTCENTER);

	indexDialog.setRow(2);
	titleBar->setTitles(_("ADDRESS BOOK INDEX"), "");

	sort(indexArg.begin(), indexArg.end(), indexSort());

	buttons.reserve(indexArg.size());
	vector< pair<string, string> >::iterator b=indexArg.begin(),
		e=indexArg.end();

	while (b != e)
	{
		indexButton *bb=new indexButton(this, *b);
		b++;

		if (!bb)
			outofmemory();

		try {
			bb->indexNum=buttons.size();
			buttons.insert(buttons.end(), bb);
		} catch (...) {
			delete bb;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}

		indexDialog.addPrompt(NULL, bb);
	}
}

void AddressBookIndexScreen::renameEntry(size_t n, string uid,
					 string newnickname)
{
	if (addressBook->rename(uid, newnickname))
	{
		keepgoing=false;
		addressBook->currentRecord.second="";
		myServer::nextScreen= &addressbookIndexScreen;
		myServer::nextScreenArg=addressBook;
	}
}

void AddressBookIndexScreen::deleteEntry(size_t n, string uid)
{
	if (addressBook->del(uid))
	{
		indexButton *b=buttons[n];

		buttons.erase(buttons.begin() + n);

		delete b;

		vector<indexButton *>::iterator p=buttons.begin() + n,
			e=buttons.end();

		if (p != e)
			(*p)->requestFocus();
		else if (buttons.size() > 0)
			(*(buttons.end()-1))->requestFocus();

		while (p != e)
		{
			(*p)->indexNum=n++;
			(*p)->setRow( (*p)->getRow() - 1);

			p++;
		}
	}
}

bool AddressBookIndexScreen
::indexSort::operator()(const pair<string, string> &a,
			const pair<string, string> &b)
{
	return (a.first.compare(b.first) < 0);
}

bool AddressBookIndexScreen::processKey(const Curses::Key &key)
{
	if (key == key_ABORT)
	{
		keepgoing=false;
		myServer::nextScreen= &mainMenu;
		myServer::nextScreenArg=NULL;
		PreviousScreen::previousScreen();
		return true;
	}

	if (key == key_ADDADDRESSBOOK)
	{
		keepgoing=false;
		addressBook->currentRecord.second="";
		myServer::nextScreen= &addressbookScreen;
		myServer::nextScreenArg=addressBook;
		return true;
	}

	if (key == key_IMPORTADDRESSBOOKENTRY)
	{
		string filename;

		{
			OpenDialog open_dialog;

			open_dialog.noMultiples();

			open_dialog.requestFocus();
			myServer::eventloop();

			vector<string> &filenameList=
				open_dialog.getFilenameList();

			if (filenameList.size() == 0)
				filename="";
			else
				filename=filenameList[0];

			mainScreen->erase();
		}
		mainScreen->draw();
		requestFocus();

		if (filename.size() == 0)
			return true;

		ifstream i(filename.c_str());

		if (!i.is_open())
		{
			statusBar->clearstatus();
			statusBar->status(strerror(errno),
					  statusBar->SYSERROR);
			statusBar->beepError();
			return true;
		}

		set<string> nicknames;

		{
			list<pair<string, string> > index;

			addressBook->getIndex(index);

			list<pair<string, string> >::iterator
				b=index.begin(), e=index.end();

			while (b != e)
			{
				nicknames.insert(b->first);
				++b;
			}
		}

		statusBar->clearstatus();
		statusBar->status(_("Importing..."));
		std::string line;
		size_t linenum=0;

		list<mail::addressbook::Entry> newList;

		while (!std::getline(i, line).eof())
		{
			std::vector<mail::emailAddress> v;
			size_t n;
			std::string name;

			++linenum;
			if (!mail::address::fromString(line, v, n))
			{
				statusBar->clearstatus();
				statusBar->status(Gettext(_("Syntax error on line %1%"))
						  << linenum);
				statusBar->beepError();
				return true;
			}

			if (v.size() && v[0].getAddr().size() == 0)
			{
				// Nickname:

				name=v[0].getName();
				v.erase(v.begin());

				if (name.size() > 0)
					name=name.substr(0, name.size()-1);
				// Drop trailing colon.
			}

			if (v.size() == 0)
				continue;

			if (name.size() == 0)
				name=v[0].getName();

			if (name.size() == 0)
				name=v[0].getAddr();

			name=mail::iconvert
				::convert_tocase(Gettext::toutf8(name),
						 "utf-8",
						 unicode_lc);

			size_t ip;
			ip=name.find(' ');
			if (ip != std::string::npos)
				name=name.substr(0, ip);
			ip=name.find('@');
			if (ip != std::string::npos)
				name=name.substr(0, ip);

			std::string nCheck=name;
			size_t cnt=0;
			while (nicknames.count(nCheck))
			{
				std::ostringstream o;

				o << name << ++cnt;
				nCheck=o.str();
			}

			mail::addressbook::Entry newEntry;
			newEntry.nickname=nCheck;
			newEntry.addresses=v;
			newList.push_back(newEntry);
			nicknames.insert(nCheck);
		}

		addressBook->import(newList);

		keepgoing=false;
		addressBook->currentRecord.second="";
		myServer::nextScreen= &addressbookIndexScreen;
		myServer::nextScreenArg=addressBook;
		return true;
	}

	if (key == key_EXPORTADDRESSBOOKENTRY)
	{
		std::string filename;

		{
			SaveDialog save_dialog;

			save_dialog.requestFocus();
			myServer::eventloop();

			filename=save_dialog;
		}

		mainScreen->erase();
		mainScreen->draw();
		requestFocus();

		if (filename.size() == 0)
			return true;

		ofstream o(filename.c_str());

		if (!o.is_open())
		{
			statusBar->clearstatus();
			statusBar->status(strerror(errno),
					  statusBar->SYSERROR);
			statusBar->beepError();
			return true;
		}

		std::list<std::pair<std::string, std::string> > index;

		addressBook->getIndex(index);

		while (!index.empty())
		{
			std::vector<mail::emailAddress> v;

			addressBook->getEntry(index.front().second, v);

			std::string s=
				mail::address::toString(index.front().first
							+ ": ", v);

			std::string::iterator b=s.begin(), e=s.end();

			while (b != e)
			{
				if (*b == '\n') *b=' ';
				++b;
			}

			o << s << std::endl;
			index.pop_front();
		}

		if (o.flush().fail() || o.bad())
		{
			statusBar->clearstatus();
			statusBar->status(strerror(errno),
					  statusBar->SYSERROR);
			statusBar->beepError();
		}
		return true;
	}

	return GlobalKeys::processKey(key, GlobalKeys::ADDRESSBOOKINDEXSCREEN,
				      NULL);
}

bool AddressBookIndexScreen::listKeys( vector< pair<string,  string> > &list)
{
	GlobalKeys::listKeys(list, GlobalKeys::ADDRESSBOOKINDEXSCREEN);

	list.push_back(make_pair(Gettext::keyname(_("ADDADDRESSBOOK_K:A")),
				 _("Add")));
	list.push_back(make_pair(Gettext::keyname(_("DELADDRESSBOOKENTRY_K:D"))
				 , _("Delete")));
	list.push_back(make_pair(Gettext::keyname(_("RENAMEADDRESSBOOKENTRY_K:R"))
				 , _("Rename")));
	list.push_back(make_pair(Gettext::keyname(_("IMPORTADDRESSBOOKENTRY_K:I"))
				 , _("Import")));
	list.push_back(make_pair(Gettext::keyname(_("EXPORTADDRESSBOOKENTRY_K:E"))
				 , _("Export")));
	return false;
}

AddressBookIndexScreen::~AddressBookIndexScreen()
{
	vector<indexButton *>::iterator b=buttons.begin(), e=buttons.end();

	while (b != e)
	{
		indexButton *p= *b;

		*b++ = NULL;

		if (p)
			delete p;
	}
}

void AddressBookIndexScreen::clicked( pair<string, string> entryArg)
{
	keepgoing=false;
	addressBook->currentRecord=entryArg;
	myServer::nextScreen= &addressbookScreen;
	myServer::nextScreenArg=addressBook;
}

void addressbookIndexScreen(void *vp)
{
	AddressBook *a=(AddressBook *)vp;

	vector< pair<string, string> > folder_indexArray;

	{
		list< pair<string, string> > folder_index;

		a->getIndex(folder_index);

		folder_indexArray.reserve(folder_index.size());

		folder_indexArray.insert(folder_indexArray.end(),
					 folder_index.begin(),
					 folder_index.end());
	}

	AddressBookIndexScreen screen(mainScreen, a, folder_indexArray);

	screen.requestFocus();
	myServer::eventloop();
}

/////////////////////////////////////////////////////////////////////////////
//
// Add LDAP directory screen

class AddLdapScreen : public CursesContainer,
		      public CursesKeyHandler {

	CursesLabel title;
	CursesDialog addDialog;

	CursesLabel nickname_label;
	CursesLabel server_label;
	CursesLabel userid_label;
	CursesLabel password_label;
	CursesLabel suffix_label;

	CursesField nickname_field;
	CursesField server_field;
	CursesField userid_field;
	CursesField password_field;
	CursesField suffix_field;

	CursesButtonRedirect<AddLdapScreen> saveButton, cancelButton;

	void save();
	void cancel();

public:
	AddLdapScreen(CursesMainScreen *mainScreen);
	~AddLdapScreen();

	bool processKey(const Curses::Key &key);
	bool listKeys( vector< pair<string,  string> > &list);
};


AddLdapScreen::AddLdapScreen(CursesMainScreen *parent)
	: CursesContainer(parent),
	  CursesKeyHandler(PRI_SCREENHANDLER),
	  title(this, _("Add LDAP directory")),
	  addDialog(this),
	  nickname_label(NULL, _("Directory name: ")),
	  server_label(NULL, _("LDAP server hostname: ")),
	  userid_label(NULL, _("LDAP userid (optional): ")),
	  password_label(NULL, _("LDAP password (optional): ")),
	  suffix_label(NULL, _("LDAP suffix/root: ")),

	  nickname_field(NULL),
	  server_field(NULL),
	  userid_field(NULL),
	  password_field(NULL),
	  suffix_field(NULL),

	  saveButton(this, _("SAVE")),
	  cancelButton(this, _("CANCEL"))
{
	setRow(4);
	setAlignment(Curses::PARENTCENTER);
	title.setAlignment(Curses::PARENTCENTER);
	addDialog.setRow(2);
	addDialog.addPrompt(&nickname_label, &nickname_field, 0);
	addDialog.addPrompt(&server_label, &server_field, 1);
	addDialog.addPrompt(&userid_label, &userid_field, 2);
	addDialog.addPrompt(&password_label, &password_field, 3);
	addDialog.addPrompt(&suffix_label, &suffix_field, 4);
	titleBar->setTitles(_("LDAP ADDRESSBOOK ADD"), "");

	password_field.setPasswordChar();
	saveButton=this;
	cancelButton=this;

	saveButton=&AddLdapScreen::save;
	cancelButton=&AddLdapScreen::cancel;

	addDialog.addPrompt(NULL, &saveButton, 6);
	addDialog.addPrompt(NULL, &cancelButton, 8);

	nickname_field.requestFocus();
}

AddLdapScreen::~AddLdapScreen()
{
}

bool AddLdapScreen::processKey(const Curses::Key &key)
{
	return GlobalKeys::processKey(key,
				      GlobalKeys::ADDLDAPADDRESSBOOKSCREEN,
				      NULL);
}

bool AddLdapScreen::listKeys( vector< pair<string,  string> > &list)
{
	GlobalKeys::listKeys(list, GlobalKeys::ADDLDAPADDRESSBOOKSCREEN);
	return false;
}

void AddLdapScreen::save()
{
	std::string name=nickname_field.getText();
	std::string server=server_field.getText();
	std::string userid=userid_field.getText();
	std::string password=password_field.getText();
	std::string suffix=suffix_field.getText();

	if (name.size() == 0)
	{
		nickname_field.requestFocus();
		nickname_field.beepError();
		return;
	}

	if (server.size() == 0)
	{
		server_field.requestFocus();
		server_field.beepError();
		return;
	}

	/* TODO */

#if HAVE_OPENLDAP

	{
		struct ldapsearch *s=l_search_alloc(server.c_str(),
						    LDAP_PORT,
						    userid.c_str(),
						    password.c_str(),
						    suffix.c_str());

		if (s)
		{
			if (l_search_ping(s) == 0)
			{
				l_search_free(s);

				AddressBook *abook=new AddressBook();

				if (!abook)
					outofmemory();

				try {
					std::string url="ldap://" + server;

					abook->init(name,
						    mail::loginUrlEncode
						    ("ldap", server,
						     userid,
						     password),
						    suffix);

					AddressBook::addressBookList
						.insert(AddressBook::
							addressBookList.end(),
							abook);
				} catch (...) {
					delete abook;
					LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
				}

				myServer::saveconfig();
				statusBar->clearstatus();
				statusBar->status(_("Address Book created."));
				cancel();
				return;
			}
			l_search_free(s);
		}
		statusBar->clearstatus();
		statusBar->status(Gettext(_("Connection to %1% failed: %2"))
				  << name
				  << strerror(errno),
				  statusBar->NORMAL);
		statusBar->beepError();
	}
#endif
}

void AddLdapScreen::cancel()
{
	keepgoing=false;

	myServer::nextScreen= &listaddressbookScreenImport;
	myServer::nextScreenArg=NULL;
}

void addLdapScreen(void *vp)
{
	AddLdapScreen als(mainScreen);

	myServer::eventloop();
}

/////////////////////////////////////////////////////////////////////////////
//
// List all address books
//

class ListAddressBookScreen : public CursesContainer ,
			      public CursesKeyHandler {

	CursesLabel title;
	CursesDialog listDialog;

	class addressbookButton : public CursesButton {

		ListAddressBookScreen *parent;

	public:
		AddressBook *addressBook;
	private:
		std::list<AddressBook *>::iterator listIterator;

		bool processKeyInFocus(const Key &key);

	public:
		addressbookButton(ListAddressBookScreen *parent,
				  AddressBook *addressBook,
				  list<AddressBook *>::iterator
				  listIterator);

		~addressbookButton();

		void clicked();
	};

	list<addressbookButton *> buttons;

	bool processKey(const Curses::Key &key);
	bool listKeys( vector< pair<string,  string> > &list);

public:
	ListAddressBookScreen(CursesMainScreen *parent);
	~ListAddressBookScreen();

	void requestFocus();

	void deleteAddressBook(list<AddressBook *>::iterator addressBook);
	void moveDown(list<AddressBook *>::iterator addressBook);
	void moveUp(list<AddressBook *>::iterator addressBook);
};

ListAddressBookScreen::addressbookButton
::addressbookButton(ListAddressBookScreen *parentArg,
		    AddressBook *addressBookArg,
		    list<AddressBook *>::iterator listIteratorArg)
	: CursesButton(NULL, addressBookArg->getName()),
	  parent(parentArg),
	  addressBook(addressBookArg),
	  listIterator(listIteratorArg)
{
}

ListAddressBookScreen::addressbookButton::~addressbookButton()
{
}

void ListAddressBookScreen::addressbookButton::clicked()
{
	keepgoing=false;
	myServer::nextScreen= &addressbookIndexScreen;
	myServer::nextScreenArg=addressBook;
}

bool ListAddressBookScreen::addressbookButton::processKeyInFocus(const
								 Key &key)
{
	if (listIterator != AddressBook::addressBookList.end())
	{
		if (key == key_DELETEADDRESSBOOK)
		{
			parent->deleteAddressBook(listIterator);
			return true;
		}

		// Control relative search order:

		if (key == key_DOWN) // CTRL-D
		{
			parent->moveDown(listIterator);
			return true;
		}

		if (key == key_UP) // CTRL-U
		{
			parent->moveUp(listIterator);
			return true;
		}
	}

	if (key == key_ADDLDAP)
	{
		keepgoing=false;
		myServer::nextScreen= &addLdapScreen;
		myServer::nextScreenArg=NULL;
		return true;
	}

	return CursesButton::processKeyInFocus(key);
}

ListAddressBookScreen::ListAddressBookScreen(CursesMainScreen *parent)
	: CursesContainer(mainScreen),
	  CursesKeyHandler(PRI_SCREENHANDLER),
	  title(this, _("Address Book List")),
	  listDialog(this)
{
	setRow(4);
	setAlignment(Curses::PARENTCENTER);
	title.setAlignment(Curses::PARENTCENTER);

	listDialog.setRow(2);

	titleBar->setTitles(_("ADDRESS BOOKS"), "");

	if (AddressBook::defaultAddressBook)
	{
		addressbookButton *bp=
			new addressbookButton(NULL,
					      AddressBook::defaultAddressBook,
					      AddressBook::addressBookList
					      .end());

		if (!bp)
			outofmemory();
		
		try {
			buttons.insert(buttons.end(), bp);
		} catch (...) {
			delete bp;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}

		listDialog.addPrompt(NULL, bp);
		listDialog.addPrompt(NULL, NULL);
	}

	list<AddressBook *>::iterator b=AddressBook::addressBookList.begin(),
		e=AddressBook::addressBookList.end();

	while (b != e)
	{
		AddressBook *p= *b;

		addressbookButton *bp=new addressbookButton(NULL, p, b);

		b++;

		if (!bp)
			outofmemory();

		try {
			buttons.insert(buttons.end(), bp);
		} catch (...) {
			delete bp;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}

		listDialog.addPrompt(NULL, bp);
		listDialog.addPrompt(NULL, NULL);
	}

}

void ListAddressBookScreen::requestFocus()
{
	list<addressbookButton *>::iterator b=buttons.begin(), e=buttons.end();

	while (b != e)
	{
		if ( (*b)->addressBook->iwantFocus)
		{
			(*b)->addressBook->iwantFocus=false;
			(*b)->requestFocus();
			return;
		}
		++b;
	}

	b=buttons.begin();
	e=buttons.end();

	if (b != e)
		(*b)->requestFocus();
}

ListAddressBookScreen::~ListAddressBookScreen()
{
	list<addressbookButton *>::iterator b=buttons.begin(), e=buttons.end();

	while (b != e)
	{
		addressbookButton *bp= *b;

		*b++=NULL;

		if (bp)
			delete bp;
	}
}

bool ListAddressBookScreen::processKey(const Curses::Key &key)
{
	if (key == key_ABORT)
	{
		keepgoing=false;
		myServer::nextScreen= &mainMenu;
		myServer::nextScreenArg=NULL;
		PreviousScreen::previousScreen();
		return true;
	}

	return GlobalKeys::processKey(key, GlobalKeys::ADDRESSBOOKLISTSCREEN,
				      NULL);
}

bool ListAddressBookScreen::listKeys( vector< pair<string, string> > &list)
{
	list.push_back(make_pair(Gettext::keyname(_("DELETEADDRESSBOOK:D")),
				 _("Delete")));
	list.push_back( make_pair(Gettext::keyname(_("MOVEUP:^U")),
				  _("Move up")));
	list.push_back( make_pair(Gettext::keyname(_("MOVEDN:^D")),
				  _("Move down")));
	list.push_back( make_pair(Gettext::keyname(_("ADDLDAP:A")),
				  _("Add LDAP server")));

	GlobalKeys::listKeys(list, GlobalKeys::ADDRESSBOOKLISTSCREEN);
	return false;
}


void listaddressbookScreenImport(void *vp)
{
	ListAddressBookScreen screen(mainScreen);

	screen.requestFocus();
	myServer::eventloop();
}

void listaddressbookScreen(void *vp)
{
	AddressBook::importName="";
	AddressBook::importAddr="";

	listaddressbookScreenImport(vp);
}

void ListAddressBookScreen
::deleteAddressBook(list<AddressBook *>::iterator addressBook)
{
	myServer::promptInfo response=
		myServer::prompt(myServer
				 ::promptInfo(Gettext(_("Delete address book \"%1%\"? (Y/N) "))
					      << (*addressBook)->getName())
				 .yesno());

	if (response.abortflag || (string)response != "Y")
		return;

	AddressBook *p= *addressBook;

	p->close();

	AddressBook::addressBookList.erase(addressBook);
	delete p;

	myServer::saveconfig();

	Curses::keepgoing=false;
	myServer::nextScreen= &listaddressbookScreenImport;
	myServer::nextScreenArg=NULL;
}

void ListAddressBookScreen
::moveDown(list<AddressBook *>::iterator ptr)
{
	list<AddressBook *>::iterator ptrNext=ptr;

	++ptrNext;

	if (ptrNext == AddressBook::addressBookList.end())
		return;

	(*ptr)->iwantFocus=true;
	AddressBook::addressBookList.insert(ptr, *ptrNext);
	AddressBook::addressBookList.erase(ptrNext);
	myServer::saveconfig();

	Curses::keepgoing=false;
	myServer::nextScreen= &listaddressbookScreenImport;
	myServer::nextScreenArg=NULL;
}

void ListAddressBookScreen
::moveUp(list<AddressBook *>::iterator ptr)
{
	if (ptr == AddressBook::addressBookList.begin())
		return;

	(*ptr)->iwantFocus=true;

	list<AddressBook *>::iterator ptrPrev=ptr;

	--ptrPrev;

	AddressBook::addressBookList.insert(ptrPrev, *ptr);
	AddressBook::addressBookList.erase(ptr);
	myServer::saveconfig();

	Curses::keepgoing=false;
	myServer::nextScreen= &listaddressbookScreenImport;
	myServer::nextScreenArg=NULL;
}

//////////////////////////////////////////////////////////////////////////////
//
// Quick address book import

class AddressBookTakeScreen : public CursesContainer,
			       public CursesKeyHandler {

	CursesLabel title;
	CursesDialog indexDialog;

	class addressButton : public CursesButton {
		AddressBookTakeScreen *myScreen;

		string name;
		string addr;

	public:
		addressButton(AddressBookTakeScreen *myScreenArg,
			      mail::emailAddress addr);
		~addressButton();

		void clicked();
	};

	vector<addressButton *> buttons;

	bool processKey(const Curses::Key &key);

	class indexSort {
	public:
		bool operator()(const mail::emailAddress &a,
				const mail::emailAddress &b);
	};

public:
	void requestFocus()
	{
		if (buttons.size() > 0)
			buttons[0]->requestFocus();
	}

	AddressBookTakeScreen(CursesMainScreen *,
			       vector< mail::emailAddress > &index);
	~AddressBookTakeScreen();
};

AddressBookTakeScreen::AddressBookTakeScreen(CursesMainScreen *parent,
					     vector<mail::emailAddress>
					     &indexArg)
	: CursesContainer(parent),
	  CursesKeyHandler(PRI_SCREENHANDLER),
	  title(this, "Take/Import Address"),
	  indexDialog(this)
{
	setRow(4);
	setAlignment(Curses::PARENTCENTER);
	title.setAlignment(Curses::PARENTCENTER);

	indexDialog.setRow(2);
	titleBar->setTitles(_("TAKE ADDRESS"), "");

	buttons.reserve(indexArg.size());

	// Sort addresses to import

	sort(indexArg.begin(), indexArg.end(), indexSort());

	vector< mail::emailAddress >::iterator b=indexArg.begin(),
		e=indexArg.end();

	while (b != e)
	{
		addressButton *bb=new addressButton(this, *b);
		b++;

		if (!bb)
			outofmemory();

		try {
			buttons.insert(buttons.end(), bb);
		} catch (...) {
			delete bb;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}

		indexDialog.addPrompt(NULL, bb);
	}
}

AddressBookTakeScreen::~AddressBookTakeScreen()
{
	vector<addressButton *>::iterator b=buttons.begin(), e=buttons.end();

	while (b != e)
	{
		addressButton *p= *b;

		*b++ = NULL;

		if (p)
			delete p;
	}
}

bool AddressBookTakeScreen::processKey(const Curses::Key &key)
{
	if (key == key_ABORT)
	{
		keepgoing=false;
		myServer::nextScreen= &mainMenu;
		myServer::nextScreenArg=NULL;
		PreviousScreen::previousScreen();
		return true;
	}
	return false;
}

AddressBookTakeScreen::addressButton
::addressButton(AddressBookTakeScreen *myScreenArg,
		mail::emailAddress addrArg)
	: CursesButton(myScreenArg,
		       mail::address(addrArg.getDisplayName(unicode_default_chset()),
				     addrArg.getDisplayAddr(unicode_default_chset())).toString()),
	  name(addrArg.getDisplayName(unicode_default_chset())),
	  addr(addrArg.getDisplayAddr(unicode_default_chset()))
{
	setStyle(MENU);
}

AddressBookTakeScreen::addressButton::~addressButton()
{
}

void AddressBookTakeScreen::addressButton::clicked()
{
	AddressBook::importName=name;
	AddressBook::importAddr=addr;

	Curses::keepgoing=false;
	myServer::nextScreen= &listaddressbookScreenImport;
	myServer::nextScreenArg=NULL;
}

void AddressBook::takeScreen(void *arg)
{
	vector<mail::emailAddress> *addresses=
		(vector<mail::emailAddress> *)arg;

	try {
		AddressBookTakeScreen takeScreen(mainScreen, *addresses);

		delete addresses;
		addresses=NULL;

		takeScreen.requestFocus();
		myServer::eventloop();
	} catch (...)
	{
		if (addresses)
			delete addresses;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

void AddressBook::take(vector<mail::address> &addrListArg)
{
	vector<mail::emailAddress> *addrbuf;

	addrbuf=new vector<mail::emailAddress>;

	if (!addrbuf)
		LIBMAIL_THROW(strerror(errno));

	try
	{
		map<string, mail::address *> addrMap;

		{
			vector<mail::address>::iterator b=addrListArg.begin(),
				e=addrListArg.end();

			while (b != e)
			{
				mail::address &a= *b++;

				if (a.getAddr().size() == 0)
					continue;

				addrMap.insert(make_pair(a.getAddr(), &a));
			}
		}

		{
			addrbuf->reserve(addrMap.size());

			map<string, mail::address *>::iterator
				b=addrMap.begin(),
				e=addrMap.end();

			while (b != e)
			{
				addrbuf->push_back( *b->second );
				++b;
			}
		}
	}
	catch (...)
	{
		delete addrbuf;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	if (addrbuf->size() == 0)
	{
		delete addrbuf;

		statusBar->clearstatus();
		statusBar->status(_("No addresses found."),
				  statusBar->NORMAL);
		statusBar->beepError();
		return;
	}

	Curses::keepgoing=false;
	myServer::nextScreen= &takeScreen;
	myServer::nextScreenArg=addrbuf;
}


void AddressBook::updateAccount(string oldUrl, string newUrl)
{
	if (defaultAddressBook)
		defaultAddressBook->updateAccountChk(oldUrl, newUrl);

	list<AddressBook *>::iterator b=addressBookList.begin(),
		e=addressBookList.end();

	while (b != e)
	{
		(*b)->updateAccountChk(oldUrl, newUrl);
		b++;
	}
}

void AddressBook::updateAccountChk(string oldUrl, string newUrl)
{
	if (url == oldUrl)
		url=newUrl;
}


/////////////////////////////////////////////////////////////////////////////

bool AddressBookTakeScreen
::indexSort::operator()(const mail::emailAddress &a,
			const mail::emailAddress &b)
{
	int n=a.getDisplayName("utf-8").compare(b.getDisplayName("utf-8"));

	if (n)
		return n < 0;

	return a.getDisplayAddr("utf-8").compare(b.getDisplayAddr("utf-8")) < 0;
}
