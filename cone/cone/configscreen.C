/*
** Copyright 2003-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "configscreen.H"
#include "nntpcommand.H"
#include "gettext.H"
#include "curseseditmessage.H"
#include "unicode/unicode.h"
#include "libmail/mail.H"
#include "libmail/misc.H"
#include "libmail/logininfo.H"
#include "init.H"
#include "colors.H"
#include "gpg.H"
#include "tags.H"
#include "globalkeys.H"
#include "curses/cursesmainscreen.H"
#include "curses/cursestitlebar.H"
#include "curses/cursesmoronize.H"
#include "myserver.H"
#include "myserverremoteconfig.H"
#include "myserverpromptinfo.H"
#include "myreferences.H"
#include "specialfolder.H"
#include "passwordlist.H"

#include <sstream>
#include <errno.h>
#include <algorithm>

extern struct CustomColor color_misc_promptColor;
extern struct CustomColor color_misc_inputField;
extern struct CustomColor color_misc_buttonColor;

extern CursesMainScreen *mainScreen;
extern CursesTitleBar *titleBar;
extern void mainMenu(void *);
extern std::string ChooseCertificate();

using namespace std;

extern Gettext::Key key_ABORT;
extern void resetScreenColors();

//////////////////////////////////////////////////////////////////////////////
//
// Helper CursesButton subclass

ConfigScreen::Button::Button(ConfigScreen *parentArg,
			     void (ConfigScreen::*deleteCallbackArg)(Button *),
			     string textArg)
	: CursesButton(parentArg, textArg),
	  parent(parentArg),
	  deleteCallback(deleteCallbackArg),
	  address(textArg)
{
}

ConfigScreen::Button::~Button()
{
}

bool ConfigScreen::Button::processKeyInFocus(const Key &key)
{
	if (key == Key::DEL)
	{
		(parent->*deleteCallback)(this);
		return true;
	}
	return CursesButton::processKeyInFocus(key);
}

void ConfigScreen::addPrompt(CursesLabel *label,
			     CursesField *field,
			     size_t atRow)
{
	Curses::CursesAttr labelAttr;
	Curses::CursesAttr fieldAttr;

	labelAttr.setFgColor(color_misc_promptColor.fcolor);
	fieldAttr.setFgColor(color_misc_inputField.fcolor);

	if (label)
		label->setAttribute(labelAttr);
	if (field)
		field->setAttribute(fieldAttr);

	CursesDialog::addPrompt(label, field, atRow);
}

void ConfigScreen::addPrompt(CursesLabel *label,
			     Curses *field,
			     size_t atRow)
{
	Curses::CursesAttr labelAttr;

	labelAttr.setFgColor(color_misc_promptColor.fcolor);

	if (label)
		label->setAttribute(labelAttr);

	CursesDialog::addPrompt(label, field, atRow);
}

//////////////////////////////////////////////////////////////////////////////

ConfigScreen::ColorButton::ColorButton(ConfigScreen *parentArg,
				       struct CustomColor *customColorArg)
	: CursesButton(parentArg, _("[ CHANGE ]")),
	  parent(parentArg), colorInfo(customColorArg),
	  currentColor(customColorArg->fcolor)
{
	setStyle(MENU);
	attribute.setFgColor(currentColor);
}

ConfigScreen::ColorButton::~ColorButton()
{
}

void ConfigScreen::ColorButton::clicked()
{
	int ncolors=getColorCount();

	if (ncolors > 1)
	{
		currentColor= (currentColor+1) % ncolors;
		attribute.setFgColor(currentColor);
		draw();
	}
}

//////////////////////////////////////////////////////////////////////////////

ConfigScreen::MacroButton::MacroButton(ConfigScreen *parentArg,
				       Macros::name &macroNameArg,
				       std::string macroNameAsStr)
	: CursesButton(parentArg, macroNameAsStr),
	  parent(parentArg), macroName(macroNameArg)
{
}

ConfigScreen::MacroButton::~MacroButton()
{
}

bool ConfigScreen::MacroButton::processKeyInFocus(const Key &key)
{
	if (key == Key::DEL)
	{
		parent->delMacro(this);
		return true;
	}
	return CursesButton::processKeyInFocus(key);
}

//////////////////////////////////////////////////////////////////////////////

ConfigScreen::ConfigScreen(CursesContainer *parent)
	: CursesDialog(parent), CursesKeyHandler(PRI_SCREENHANDLER),
	  myAddressLabel(this, _("Your E-mail addresses: ")),
	  myAddressAdd(this),

	  htmlDemoronization(this),

	  postAndMailLabel(this, _("Cc on NetNews follow-ups: ")),
	  postAndMail(this),

	  myListAddressLabel(this, _("Your mailing list addresses: ")),

	  myListAddressAdd(this),

	  myHeadersLabel(this, _("New custom header: ")),
	  myHeadersAdd(this),

	  sentFoldersLabel(this, _("Custom sent mail folders: ")),

	  dictionaryLabel(this, _("Spelling dictionary: ")),
	  dictionaryField(this),

	  smtpServerLabel(this, _("Outgoing SMTP Server: ")),
	  smtpServer(this),

	  smtpCertificateLabel(this, _("SMTP SSL certificate (optional): ")),
	  smtpCertificateButton(myServer::smtpServerCertificate),

	  smtpServerUIDLabel(this, _("SMTP userid (optional): ")),
	  smtpServerUID(this),

	  smtpServerCRAM(this, _("Don't send SMTP password in clear text"),
			 1),
	  smtpUseIMAP(this, _("Send mail via main acct (if available)"), 1),

	  smtpUseSSL(this, _("Use SMTP tunneled over SSL"), 1),

	  nntpCommandLabel(this, _("NetNews posting command: ")),
	  nntpCommand(this),

	  suspendLabel(this, _("CTRL-Z command: ")),
	  suspendCommand(this),

	  editorLabel(this, _("External Editor (^U): ")),
	  editorCommand(this),

	  moronizationEnabled(this, _("\"Smart\" keyboard shortcuts")),
	  quitNoPrompt(this, _("'Q' quits without prompting"), 1),

	  autosaveLabel(this, _("Autosave (minutes): ")),
	  autosaveField(this),

	  watchDaysLabel(this, _("Watch threads for # of days: ")),
	  watchDaysField(this),

	  watchDepthLabel(this, _("Watch threads until # of followups: ")),
	  watchDepthField(this),

	  gpgopts1Label(this, _("Extra GnuPG encrypt/sign options: ")),
	  gpgopts1Field(this),

	  gpgopts2Label(this, _("Extra GnuPG decrypt/verify options: ")),
	  gpgopts2Field(this),

	  resetRemote(this, _("Turn off remote configuration")),
	  macrosLabel(this, _("Currently defined macros: ")),

	  save(this, _("SAVE")),
	  cancel(this, _("CANCEL"))
{
	{
		CursesMainScreen::Lock updateLock(mainScreen);
		init();
	}
	erase();
	draw();
	myAddressAdd.requestFocus();

}

void ConfigScreen::init()
{
	myAddressAdd=this;
	myListAddressAdd=this;
	myHeadersAdd=this;

	save=this;
	cancel=this;
	resetRemote=this;

	myAddressAdd= &ConfigScreen::addAddress;
	myListAddressAdd= &ConfigScreen::addListAddress;
	myHeadersAdd= &ConfigScreen::addHeader;

	save= &ConfigScreen::doSave;
	cancel= &ConfigScreen::doCancel;
	resetRemote= &ConfigScreen::doResetRemote;

	// Initialize various fields.

	vector<string> demoronizationOptions;

	demoronizationOptions.push_back(_("Disable rich text de-moronization"));
	demoronizationOptions.push_back(_("Minimum rich text de-moronization"));
	demoronizationOptions.push_back(_("Maximum rich text de-moronization"));
	htmlDemoronization
		.setOptions(demoronizationOptions,
			    myServer::demoronizationType==myServer::DEMORON_MIN
			    ? 1 :
			    myServer::demoronizationType==myServer::DEMORON_MAX
			    ? 2 : 0);

	vector<string> postAndMailOpts;

	postAndMailOpts.push_back(_("Ask"));
	postAndMailOpts.push_back(_("Yes"));
	postAndMailOpts.push_back(_("No"));
	postAndMail.setOptions(postAndMailOpts,
			       myServer::postAndMail ==
			       myServer::POSTANDMAIL_YES ? 1:
			       myServer::postAndMail ==
			       myServer::POSTANDMAIL_NO ? 2:0);

	int rownum=0;

	addPrompt(&myAddressLabel, &myAddressAdd, (rownum += 1));

	htmlDemoronization.setStyle( CursesButton::NORMAL );
	addPrompt(NULL, &htmlDemoronization, (rownum += 2));

	addPrompt(&myListAddressLabel, &myListAddressAdd, (rownum += 2));

	addPrompt(&sentFoldersLabel, (CursesField *)NULL, (rownum += 2));

	addPrompt(&myHeadersLabel, &myHeadersAdd, (rownum += 2));

	addPrompt(&dictionaryLabel, &dictionaryField, (rownum += 2));

	addPrompt(&smtpServerLabel, &smtpServer, (rownum += 2));
	addPrompt(&smtpCertificateLabel, &smtpCertificateButton, (rownum += 1));
	addPrompt(&smtpServerUIDLabel, &smtpServerUID, (rownum += 1));
	addPrompt(NULL, &smtpServerCRAM, (rownum += 1));
	addPrompt(NULL, &smtpUseIMAP, (rownum += 1));
	addPrompt(NULL, &smtpUseSSL, (rownum += 1));

	addPrompt(&nntpCommandLabel, &nntpCommand, (rownum += 2));

	addPrompt(&suspendLabel, &suspendCommand, (rownum += 2));

	addPrompt(&editorLabel, &editorCommand, (rownum += 2));

	addPrompt(NULL, &moronizationEnabled, (rownum += 2));
	addPrompt(NULL, &quitNoPrompt, (rownum += 2));

	addPrompt(&autosaveLabel, &autosaveField, (rownum += 2));

	addPrompt(&watchDaysLabel, &watchDaysField, (rownum += 2));
	addPrompt(&watchDepthLabel, &watchDepthField, (rownum += 1));

	postAndMail.setStyle( CursesButton::NORMAL );
	addPrompt(&postAndMailLabel, &postAndMail, (rownum += 2));

	addPrompt(&gpgopts1Label, &gpgopts1Field, (rownum += 2));

	addPrompt(&gpgopts2Label, &gpgopts2Field, (rownum += 2));

	{
		size_t i;
		size_t skip=2;

		for (i=1; i < Tags::tags.names.size(); i++)
		{
			CursesLabel *l=new
				CursesLabel(this, Gettext(_("Tag %1% name: "))
					    << i);
			if (!l)
				throw strerror(errno);

			CursesField *f=new CursesField(this);

			if (!f)
			{
				delete l;
				throw strerror(errno);
			}

			try {
				tagLabels.push_back(l);
			} catch (...) {
				delete f;
				delete l;
				throw;
			}

			try {
				tagFields.push_back(f);
			} catch (...) {
				delete f;
				throw;
			}

			addPrompt(l, f, (rownum += skip));
			skip=1;

			f->setText(Tags::tags.names[i]);
		}
	}

	{
		struct CustomColorGroup *g=getColorGroups();

		while (g->colors)
		{
			CursesLabel *l=new CursesLabel(this,
						       gettext(g->groupDescr));

			if (!l)
				throw strerror(errno);

			try {
				colorLabels.push_back(l);
			} catch (...) {
				delete l;
				throw;
			}

			addPrompt(l, (CursesField *)NULL, rownum += 2);

			struct CustomColor **c=g->colors;

			while (*c)
			{
				string s=" ";

				s += gettext((*c)->descr);

				CursesLabel *l=new CursesLabel(this, s);

				if (!l)
					throw strerror(errno);

				try {
					colorLabels.push_back(l);
				} catch (...) {
					delete l;
					throw;
				}

				ColorButton *cb= new
					ColorButton(this, *c);

				if (!cb)
					throw strerror(errno);

				try {
					colorButtons.push_back(cb);
				} catch (...) {
					delete cb;
					throw;
				}

				addPrompt(l, cb, ++rownum);
				++c;
			}

			++g;
		}
	}

	addPrompt(NULL, &resetRemote, (rownum += 2));

	addPrompt(&macrosLabel, (Curses *)NULL, (rownum += 2));

	Macros *mp=Macros::getRuntimeMacros();

	if (mp)
	{
		map<Macros::name, string>::iterator b, e;

		for (b=mp->macroList.begin(), e=mp->macroList.end(); b != e;
		     ++b)
		{
			Macros::name n=b->first;

			string nameStr;

			if (b->first.f)
			{
				nameStr=Gettext(_("FUNCTION KEY: F%1%")) <<
					(unsigned)(b->first.f-1);
			}
			else
			{
				vector<unicode_char> cpy=b->first.n;

				bool err;

				nameStr=mail::iconvert::convert(cpy,
								unicode_default_chset(),
								err);

				if (err || nameStr.size() == 0)
					nameStr=_("[macro]");
			}

			MacroButton *mb=new MacroButton(this, n, nameStr);

			if (!mb)
				throw strerror(errno);

			try {
				macroButtons.insert(mb);
			} catch (...)
			{
				delete mb;
				LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
			}
		}

		vector<MacroButton *> sortedList;

		sortedList.insert(sortedList.end(),
				  macroButtons.begin(),
				  macroButtons.end());

		std::sort(sortedList.begin(),
			  sortedList.end(),
			  MacroButtonSortHelper());

		vector<MacroButton *>::iterator slb=sortedList.begin(),
			sle=sortedList.end();

		while (slb != sle)
		{
			addPrompt(NULL, *slb, ++rownum);
			++slb;
		}
	}

	Curses::CursesAttr bca;

	bca.setFgColor(color_misc_buttonColor.fcolor);

	save.setAttribute(bca);
	cancel.setAttribute(bca);

	addPrompt(NULL, &save, (rownum += 2));
	addPrompt(NULL, &cancel, (rownum += 1));

	dictionaryField.setText(spellCheckerBase->dictionaryName);

	nntpCommand.setText(nntpCommandFolder::nntpCommand);
	suspendCommand.setText( Curses::suspendCommand );
	editorCommand.setText( CursesEditMessage::externalEditor);

	moronizationEnabled.setToggled(CursesMoronize::enabled);
	quitNoPrompt.setToggled(GlobalKeys::quitNoPrompt);


	{
		ostringstream o;
		time_t t=CursesEditMessage::autosaveInterval / 60;

		if (t <= 0)
			t=1;

		o << t;
		autosaveField.setWidth(3);
		autosaveField.setText(o.str());
	}

	{
		ostringstream i;

		i << Watch::defaultWatchDays;

		watchDaysField.setWidth(3);
		watchDaysField.setText(i.str());
	}

	{
		ostringstream i;

		i << Watch::defaultWatchDepth;

		watchDepthField.setWidth(3);
		watchDepthField.setText(i.str());
	}

	gpgopts1Field.setText(GPG::gpg.extraEncryptSignOptions);
	gpgopts2Field.setText(GPG::gpg.extraDecryptVerifyOptions);

	vector<string>::iterator b, e;

	size_t r=2;

	{
		mail::loginInfo SMTPInfo;

		smtpUseSSL.setToggled(false);

		if (mail::loginUrlDecode(myServer::smtpServerURL, SMTPInfo)
		    && (SMTPInfo.method == "smtp"
			|| SMTPInfo.method == "smtps"))
		{
			if (SMTPInfo.method.size() > 4)
				smtpUseSSL.setToggled(true);

			string serverName=SMTPInfo.server;
			smtpServerUID.setText(SMTPInfo.uid);

			map<string, string>::iterator b, e;

			b=SMTPInfo.options.begin();
			e=SMTPInfo.options.end();

			while (b != e)
			{
				string s= b->first;

				if (s == "cram")
					smtpServerCRAM.setToggled(true);
				else
				{
					if (b->second.size() > 0)
						s= s + "=" + b->second;

					serverName = serverName + "/" + s;
				}
				++b;
			}


			smtpServer.setText(serverName);
		}

		smtpUseIMAP.setToggled(myServer::useIMAPforSending);
	}

	for (b=myServer::myAddresses.begin(),
		     e=myServer::myAddresses.end(); b != e; b++)
	{
		mail::emailAddress emailAddress(mail::address("", *b));

		Button *bb=new Button(this, &ConfigScreen::delAddress,
				      emailAddress.getDisplayAddr(unicode_default_chset()));

		if (!bb)
			outofmemory();

		try {
			addresses.insert(bb);
		} catch (...) {
			delete bb;
		}

		bb->setAttribute(bca);
		addPrompt(NULL, bb, r++);
	}

	r=sentFoldersLabel.getRow()+1;

	SpecialFolder &sentFolder=SpecialFolder::folders.find(SENT)->second;
	vector<SpecialFolder::Info>::iterator sentFolderInfo=
		sentFolder.folderList.begin();

	while (sentFolderInfo != sentFolder.folderList.end())
	{
		SpecialFolder::Info &info= *sentFolderInfo++;

		string nameStr=Gettext::fromutf8(info.nameUTF8.c_str());

		Button *bb=new Button(this, &ConfigScreen::delSentFolder,
				      nameStr);
		if (!bb)
			outofmemory();

		try {
			sentFolders.insert(bb);
		} catch (...) {
			delete bb;
		}
		bb->setAttribute(bca);
		addPrompt(NULL, bb, r++);
	}

	r=myListAddressAdd.getRow()+1;
	for (b=myServer::myListAddresses.begin(),
		     e=myServer::myListAddresses.end(); b != e; b++)
	{
		mail::emailAddress emailAddress(mail::address("", *b));

		Button *bb=new Button(this, &ConfigScreen::delListAddress,
				      emailAddress.getDisplayAddr(unicode_default_chset()));

		if (!bb)
			outofmemory();

		try {
			listAddresses.insert(bb);
		} catch (...) {
			delete bb;
		}
		bb->setAttribute(bca);
		addPrompt(NULL, bb, r++);
	}

	r=myHeadersAdd.getRow()+1;

	string h=myServer::customHeaders;

	while (h.size() > 0)
	{
		size_t p=h.find(',');

		string hh;

		if (p != std::string::npos)
		{
			hh=h.substr(0, p);
			h=h.substr(p+1);
		}
		else
		{
			hh=h;
			h="";
		}

		Button *bb=new Button(this, &ConfigScreen::delHeader, hh);

		if (!bb)
			outofmemory();

		try {
			headers.insert(bb);
		} catch (...) {
			delete bb;
		}
		bb->setAttribute(bca);
		addPrompt(NULL, bb, r++);
	}
}

ConfigScreen::~ConfigScreen()
{
	set<Button *>::iterator b, e;

	for (b=addresses.begin(), e=addresses.end(); b != e; )
	{
		Button *p= *b;

		if (p)
		{
			addresses.erase(b++);
			delete p;
		}
		else
			b++;
	}

	for (b=listAddresses.begin(), e=listAddresses.end(); b != e; )
	{
		Button *p= *b;

		if (p)
		{
			listAddresses.erase(b++);
			delete p;
		}
		else
			b++;
	}

	for (b=sentFolders.begin(), e=sentFolders.end(); b != e; )
	{
		Button *p= *b;

		if (p)
		{
			sentFolders.erase(b++);
			delete p;
		}
		else
			b++;
	}

	size_t n;

	for (n=0; n<tagLabels.size(); n++)
		if (tagLabels[n])
		{
			delete tagLabels[n];
			tagLabels[n]=NULL;
		}

	for (n=0; n<tagFields.size(); n++)
		if (tagFields[n])
		{
			delete tagFields[n];
			tagFields[n]=NULL;
		}

	for (n=0; n<colorLabels.size(); n++)	
		if (colorLabels[n])
		{
			delete colorLabels[n];
			colorLabels[n]=NULL;
		}

	for (n=0; n<colorButtons.size(); n++)
		if (colorButtons[n])
		{
			delete colorButtons[n];
			colorButtons[n]=NULL;
		}


	set<MacroButton *>::iterator mb, me;

	for (mb=macroButtons.begin(), me=macroButtons.end(); mb != me; )
	{
		MacroButton *p= *mb;

		if (p)
		{
			macroButtons.erase(mb++);
			delete p;
		}
		else
			mb++;
	}
}

// Add my address

void ConfigScreen::addAddress()
{
	string address=myAddressAdd.getText();

	if (address.size() == 0)
		return;

	Button *newButton=new Button(this, &ConfigScreen::delAddress,
				     address);

	if (!newButton)
		outofmemory();

	try {
		addresses.insert(newButton);
	} catch (...) {
		delete newButton;
	}

	size_t r=myAddressLabel.getRow()+1;

	set<Button *>::iterator b=addresses.begin(), e=addresses.end();

	while (b != e)
	{
		Button *bb= *b++;

		if ((size_t)bb->getRow() >= r)
			r=bb->getRow()+1;
	}
	erase();
	myAddressAdd.setText("");

	Curses::CursesAttr bca;

	bca.setFgColor(color_misc_buttonColor.fcolor);
	newButton->setAttribute(bca);
	addPrompt(NULL, newButton, r);
	draw();
}

//
// Add a mailing list address.

void ConfigScreen::addListAddress()
{
	string address=myListAddressAdd.getText();

	if (address.size() == 0)
		return;

	Button *newButton=new Button(this, &ConfigScreen::delListAddress,
				     address);

	if (!newButton)
		outofmemory();

	try {
		listAddresses.insert(newButton);
	} catch (...) {
		delete newButton;
	}

	size_t r=myListAddressLabel.getRow()+1;

	set<Button *>::iterator b=listAddresses.begin(), e=listAddresses.end();

	while (b != e)
	{
		Button *bb= *b++;

		if ((size_t)bb->getRow() >= r)
			r=bb->getRow()+1;
	}
	erase();
	myListAddressAdd.setText("");
	Curses::CursesAttr bca;

	bca.setFgColor(color_misc_buttonColor.fcolor);
	newButton->setAttribute(bca);
	addPrompt(NULL, newButton, r);
	draw();
}

// Add vanity header.

void ConfigScreen::addHeader()
{
	string header=myHeadersAdd.getText();

	if (header.size() == 0)
		return;

	static const char * const reserved_headers[]={
		"From",
		"To",
		"Cc",
		"Bcc",
		"Sender",
		"Reply-To",
		"In-Reply-To",
		"Message-ID",
		"Newsgroups",
		"Followup-To",
		"References",
		"Subject",
		"Date",
		"X-Server",
		"X-Folder",
		"X-UID",
		"Mime-Version",
		"Content-Type",
		"Content-Transfer-Encoding",
		NULL};

	size_t i;

	for (i=0; reserved_headers[i]; i++)
	{
		if (strcasecmp(header.c_str(), reserved_headers[i]) == 0)
		{
			statusBar->clearstatus();
			statusBar->status(_("Cannot add this custom header."));
			statusBar->beepError();
			return;
		}
	}

	string::iterator b=header.begin(), e=header.end();

	while (b != e)
	{
		if (strchr("abcdefghijklmnopqrstuvwxyz"
			   "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			   "0123456789-", *b) == NULL)
		{
			statusBar->clearstatus();
			statusBar->status(_("Invalid character in header's"
					    " name"));
			statusBar->beepError();
			return;
		}
		b++;
	}

	myServer::promptInfo promptInfo=
		myServer::prompt(myServer::promptInfo(_("Hide this header"
							" by default? (Y/N) "))
				 .yesno());

	if (promptInfo.abortflag)
		return;

	if ((string)promptInfo == "Y")
		header="*" + header;

	header=header + ":";

	Button *newButton=new Button(this, &ConfigScreen::delHeader,
				    header);

	if (!newButton)
		outofmemory();

	try {
		headers.insert(newButton);
	} catch (...) {
		delete newButton;
	}

	size_t r=myHeadersAdd.getRow();

	{
		set<Button *>::iterator b=headers.begin(), e=headers.end();

		while (b != e)
		{
			size_t rr=(*b)->getRow();

			if (rr > r) r=rr;
			b++;
		}
	}

	erase();
	myHeadersAdd.setText("");
	Curses::CursesAttr bca;

	bca.setFgColor(color_misc_buttonColor.fcolor);
	newButton->setAttribute(bca);
	addPrompt(NULL, newButton, r+1);
	draw();
}

// Delete my address

void ConfigScreen::delAddress(Button *b)
{
	set<Button *>::iterator p=addresses.find(b), a;

	if (p != addresses.end())
	{
		erase();
		delPrompt((CursesField *)b);

		a=p;
		a++;
		erase();
		addresses.erase(p);
		delete b;

		if (a != addresses.end())
			(*a)->requestFocus();
		else if (a != addresses.begin())
			(*--a)->requestFocus();
		else myAddressAdd.requestFocus();
		draw();
	}
}

// Delete a macro button

void ConfigScreen::delMacro(MacroButton *b)
{
	set<MacroButton *>::iterator p=macroButtons.find(b), a;

	if (p == macroButtons.end())
		return;

	erase();
	delPrompt( (CursesField *)b);
	deletedMacros.push_back(b->macroName);

	a=p;
	a++;
	erase();
	macroButtons.erase(p);
	delete b;

	if (a != macroButtons.end())
		(*a)->requestFocus();
	else if (a != macroButtons.begin())
		(*--a)->requestFocus();
	else save.requestFocus();
	draw();
}

// Delete mailing list address

void ConfigScreen::delListAddress(Button *b)
{
	set<Button *>::iterator p=listAddresses.find(b), a;

	if (p != listAddresses.end())
	{
		erase();
		delPrompt((CursesField *)b);

		a=p;
		a++;
		erase();
		listAddresses.erase(p);
		delete b;

		if (a != listAddresses.end())
			(*a)->requestFocus();
		else if (a != listAddresses.begin())
			(*--a)->requestFocus();
		else myListAddressAdd.requestFocus();
		draw();
	}
}

// Delete header

void ConfigScreen::delHeader(Button *b)
{
	set<Button *>::iterator p=headers.find(b), a;

	if (p != headers.end())
	{
		erase();
		delPrompt( (CursesField *)b);

		a=p;
		a++;
		erase();
		headers.erase(p);
		delete b;

		if (a != headers.end())
			(*a)->requestFocus();
		else if (a != headers.begin())
			(*--a)->requestFocus();
		else myHeadersAdd.requestFocus();
		draw();
	}
}

// Delete sent folder

void ConfigScreen::delSentFolder(Button *b)
{
	set<Button *>::iterator p=sentFolders.find(b), a;

	if (p != sentFolders.end())
	{
		erase();
		delPrompt((CursesField *)b);

		a=p;
		a++;
		erase();
		sentFolders.erase(p);
		delete b;

		if (a != sentFolders.end())
			(*a)->requestFocus();
		else if (a != sentFolders.begin())
			(*--a)->requestFocus();
		else myListAddressAdd.requestFocus();
		draw();
	}
}

bool ConfigScreen::processKey(const Curses::Key &key)
{
	if (key == key_ABORT)
	{
		doCancel();
		return true;
	}
	return false;
}

bool ConfigScreen::listKeys( vector< pair<string, string> > &list)
{
        list.push_back( make_pair(Gettext::keyname(_("ABORT:^C")),
                                  _("Cancel")));
        list.push_back( make_pair(Gettext::keyname(_("DELADDRESS:DEL")),
                                  _("Delete address/folder/macro")));
	return true;
}

void ConfigScreen::doSave()
{
	nntpCommandFolder::nntpCommand=nntpCommand.getText();

	set<Button *>::iterator b, e;

	std::string errmsg;

	b=addresses.begin();
	e=addresses.end();

	while (b != e)
	{
		mail::emailAddress addr;

		errmsg=addr.setDisplayAddr((*b)->address,
					   unicode_default_chset());

		if (errmsg != "")
		{
			(*b)->requestFocus();
			break;
		}
		b++;
	}

	if (errmsg == "")
	{
		b=listAddresses.begin();
		e=listAddresses.end();

		while (b != e)
		{
			mail::emailAddress addr;

			errmsg=addr.setDisplayAddr((*b)->address,
						   unicode_default_chset());
			if (errmsg != "")
			{
				(*b)->requestFocus();
				break;
			}
			b++;
		}
	}

	if (errmsg != "")
	{
		statusBar->clearstatus();
		statusBar->status(errmsg);
		statusBar->beepError();
		return;
	}

	b=addresses.begin();
	e=addresses.end();

	myServer::myAddresses.clear();

	while (b != e)
	{
		mail::emailAddress addr;

		addr.setDisplayAddr((*b)->address,
				    unicode_default_chset());

		myServer::myAddresses.push_back( addr.getAddr() );
		b++;
	}

	myServer::myListAddresses.clear();
	b=listAddresses.begin();
	e=listAddresses.end();

	while (b != e)
	{
		mail::emailAddress addr;

		addr.setDisplayAddr((*b)->address,
				    unicode_default_chset());

		myServer::myListAddresses.push_back( addr.getAddr() );
		b++;
	}

	b=headers.begin();
	e=headers.end();

	vector<string> headerList;
	while (b != e)
	{
		headerList.push_back( (*b)->address );
		b++;
	}

	sort(headerList.begin(), headerList.end());

	myServer::customHeaders="";

	vector<string>::iterator fb=headerList.begin(),	fe=headerList.end();

	while (fb != fe)
	{
		if (myServer::customHeaders.size() > 0)
			myServer::customHeaders += ",";

		myServer::customHeaders += *fb;

		fb++;
	}


	string server=smtpServer.getText();

	if (server.size() > 0)
	{
		string uid=smtpServerUID.getText();
		
		if (smtpServerCRAM.getSelected())
			server += "/cram";

		string newurl=
			mail::loginUrlEncode(smtpUseSSL.getSelected() ?
					     "smtps":"smtp", server, uid, "");

		if (newurl != myServer::smtpServerURL)
		{
			myServer::smtpServerPassword="";
			PasswordList::passwordList.remove(myServer::smtpServerURL);
		}
		myServer::smtpServerURL=newurl;
	}
	else
	{
		if (myServer::smtpServerURL != "")
			PasswordList::passwordList.remove(myServer::smtpServerURL);

		myServer::smtpServerURL="";
		myServer::smtpServerPassword="";
	}

	myServer::useIMAPforSending=
		smtpUseIMAP.getSelected() ? true:false;

	myServer::smtpServerCertificate=smtpCertificateButton.cert;

	switch (htmlDemoronization.getSelectedOption()) {
	case 0:
		myServer::setDemoronizationType(myServer::DEMORON_OFF);
		break;
	case 1:
		myServer::setDemoronizationType(myServer::DEMORON_MIN);
		break;
	case 2:
		myServer::setDemoronizationType(myServer::DEMORON_MAX);
		break;
	}

	switch (postAndMail.getSelectedOption()) {
	case 0:
		myServer::postAndMail=myServer::POSTANDMAIL_ASK;
		break;
	case 1:
		myServer::postAndMail=myServer::POSTANDMAIL_YES;
		break;
	case 2:
		myServer::postAndMail=myServer::POSTANDMAIL_NO;
		break;
	default:
		break;
	}

	b=sentFolders.begin();
	e=sentFolders.end();
	set<string> sentFoldersList;

	while (b != e)
	{
		sentFoldersList.insert( (*b)->address );
		b++;
	}

	SpecialFolder &sentFolder=SpecialFolder::folders.find(SENT)->second;
	size_t sentFolderIndex;

	for (sentFolderIndex=0;
	     sentFolderIndex < sentFolder.folderList.size(); )
	{
		SpecialFolder::Info &info=
			sentFolder.folderList[sentFolderIndex];

		string nameStr=Gettext::fromutf8(info.nameUTF8.c_str());

		set<string>::iterator p=sentFoldersList.find(nameStr);

		if (p != sentFoldersList.end())
		{
			++sentFolderIndex;
			sentFoldersList.erase(p);
		}
		else
		{
			sentFolder.folderList.erase(sentFolder.folderList
						    .begin()+sentFolderIndex);
		}
	}

	string newDictionaryName=dictionaryField.getText();

	if (newDictionaryName != spellCheckerBase->dictionaryName &&
	    newDictionaryName.size() > 0)
		spellCheckerBase->setDictionary(newDictionaryName);

	Curses::suspendCommand=suspendCommand.getText();
	CursesEditMessage::externalEditor=editorCommand.getText();
	GlobalKeys::quitNoPrompt=
		quitNoPrompt.getSelected() ? true:false;
	CursesMoronize::enabled=
		moronizationEnabled.getSelected() ? true:false;

	{
		time_t t=1;
		istringstream i(autosaveField.getText());

		i >> t;

		if (t <= 0)
			t=1;
		if (t > 60)
			t=60;
		CursesEditMessage::autosaveInterval= t * 60;
	}

	{
		istringstream i(watchDaysField.getText());

		i >> Watch::defaultWatchDays;

		if (Watch::defaultWatchDays <= 0)
			Watch::defaultWatchDays=1;
		if (Watch::defaultWatchDays > 99)
			Watch::defaultWatchDays=99;
	}

	{
		istringstream i(watchDepthField.getText());

		i >> Watch::defaultWatchDepth;

		if (Watch::defaultWatchDepth <= 0)
			Watch::defaultWatchDepth=1;
		if (Watch::defaultWatchDepth > 99)
			Watch::defaultWatchDepth=99;
	}


	GPG::gpg.extraEncryptSignOptions=gpgopts1Field.getText();
	GPG::gpg.extraDecryptVerifyOptions=gpgopts2Field.getText();

	size_t i;

	for (i=0; i<tagFields.size(); i++)
	{
		if (i+1 < Tags::tags.names.size())
			Tags::tags.names[i+1]=tagFields[i]->getText();
	}

	for (i=0; i<colorButtons.size(); i++)
		colorButtons[i]->colorInfo->fcolor=
			colorButtons[i]->currentColor;
	resetScreenColors();

	Macros *mp=Macros::getRuntimeMacros();

	if (mp)
	{
		vector<Macros::name>::iterator dmb=deletedMacros.begin(),
			dme=deletedMacros.end();

		while (dmb != dme)
		{
			map<Macros::name, string>::iterator ptr=
				mp->macroList.find(*dmb);

			if (ptr != mp->macroList.end())
				mp->macroList.erase(ptr);
			++dmb;
		}
	}

	myServer::saveconfig(true);
	PasswordList::passwordList.save();
	doCancel();
}

void ConfigScreen::doCancel()
{
	keepgoing=false;
	myServer::nextScreen= &mainMenu;
}

void ConfigScreen::doResetRemote()
{
	if (myServer::remoteConfigAccount == NULL)
	{
		statusBar->clearstatus();
		statusBar->status(_("Remote configuration already off"));
		return;
	}

	myServer::promptInfo promptInfo=
		myServer::prompt(myServer::promptInfo(_("Are you sure? (Y/N) ")
						      ).yesno());

	if (promptInfo.abortflag || (string)promptInfo != "Y")
		return;

	myServer::remoteConfigAccount->logout();
	delete myServer::remoteConfigAccount;
	myServer::remoteConfigAccount=NULL;
	myServer::remoteConfigURL="";
	myServer::remoteConfigPassword="";
	myServer::remoteConfigFolder="";
	myServer::saveconfig(true);

	statusBar->clearstatus();
	statusBar->status(_("Remote configuration turned off"));
}


void setupScreen(void *dummy)
{
	ConfigScreen screen(mainScreen);

	screen.setCol(2);
	screen.draw();

	titleBar->setTitles(_("SETUP"), "");
	myServer::eventloop();
}

////////////////////////////////////////////////////////////////////////////////
//
// Pick a certificate

bool ConfigScreen::CertificateButton::processKey(const Curses::Key &key)
{
	return false;
}

ConfigScreen::CertificateButton::CertificateButton(std::string certArg)
	: CursesButton(NULL, ""),
	  CursesKeyHandler(PRI_SCREENHANDLER),
	  cert(certArg)
{
	showtext();
}

void ConfigScreen::CertificateButton::showtext()
{
	setText(cert.size() ? _("Remove certificate"):
		_("Use certificate"));
}

ConfigScreen::CertificateButton::~CertificateButton()
{
}

void ConfigScreen::CertificateButton::clicked()
{
	if (cert.size())
	{
		cert="";
		showtext();
		return;
	}

	cert=ChooseCertificate();
	showtext();
	requestFocus();
}
