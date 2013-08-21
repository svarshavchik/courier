/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "encryptionmenu.H"

#include "unicode/unicode.h"

#include "gettext.H"
#include "init.H"
#include "myserver.H"
#include "myserverpromptinfo.H"
#include "gpg.H"
#include "gpglib/gpglib.h"
#include <curses/curseschoicebutton.H>
#include "libmail/rfcaddr.H"
#include "outputdialog.H"
#include "passwordlist.H"

#include <sstream>
#include <errno.h>

extern const char *gpgprog();

using namespace std;

extern Gettext::Key key_PRIVATEKEY;

extern Gettext::Key key_GPGNEWKEY;
extern Gettext::Key key_GPGDELKEY;
extern Gettext::Key key_GPGSIGNKEY;
extern Gettext::Key key_GPGEDITKEY;

extern Gettext::Key key_GPGNOANSWER;
extern Gettext::Key key_GPGNOCHECKING;
extern Gettext::Key key_GPGCASUALCHECK;
extern Gettext::Key key_GPGCAREFULCHECK;

void encryptionMenu(void *dummy);
extern void mainMenu(void *);


EncryptionMenu::EncryptionMenu(CursesContainer *parent)
	: MenuScreen(parent, _("ENCRYPTION"),
		     encryptionMenu,
		     GlobalKeys::ENCRYPTIONMENU)
{
	myAction.push_back(MenuActionCallback<EncryptionMenu>(this, &EncryptionMenu::mainmenu_s));
	myAction.push_back(MenuActionCallback<EncryptionMenu>(this, &EncryptionMenu::genkey_s));
	myAction.push_back(MenuActionCallback<EncryptionMenu>(this, &EncryptionMenu::delkey_s));
	myAction.push_back(MenuActionCallback<EncryptionMenu>(this, &EncryptionMenu::signkey_s));
	myAction.push_back(MenuActionCallback<EncryptionMenu>(this, &EncryptionMenu::editkey_s));

	myEntries.push_back(MenuScreen::MenuEntry(_("M - MAIN MENU         "),
						  myAction[0]));
	myEntries.push_back(MenuScreen::MenuEntry(_("N - NEW ENCRYPTION KEY"),
						  myAction[1]));
	myEntries.push_back(MenuScreen::MenuEntry(_("D - DELETE KEY        "),
						  myAction[2]));
	myEntries.push_back(MenuScreen::MenuEntry(_("S - SIGN KEY          "),
						  myAction[3]));
	myEntries.push_back(MenuScreen::MenuEntry(_("E - EDIT KEY          "),
						  myAction[4]));


	myKeys.push_back(MenuScreen::ShortcutKey(&myEntries[1],
						 Gettext::keyname(_("NEWKEY_K:N")),
						 _("New Encryption Key"),
						 key_GPGNEWKEY));

	myKeys.push_back(MenuScreen::ShortcutKey(&myEntries[2],
						 Gettext::keyname(_("DELKEY_K:D")),
						 _("Delete Key"),
						 key_GPGDELKEY));

	myKeys.push_back(MenuScreen::ShortcutKey(&myEntries[3],
						 Gettext::keyname(_("SIGNKEY_K:S")),
						 _("Sign Key"),
						 key_GPGSIGNKEY));

	myKeys.push_back(MenuScreen::ShortcutKey(&myEntries[2],
						 Gettext::keyname(_("EDITKEY_K:E")),
						 _("Edit Key"),
						 key_GPGEDITKEY));
	init();
}

EncryptionMenu::~EncryptionMenu()
{
}

vector<MenuScreen::MenuEntry> &EncryptionMenu::getMenuVector()
{
	return myEntries;
}

vector<MenuScreen::ShortcutKey> &EncryptionMenu::getKeyVector()
{
	return myKeys;
}

void EncryptionMenu::dummy()
{
}

void EncryptionMenu::mainmenu_s()
{
	keepgoing=false;
	myServer::nextScreen= &mainMenu;
	myServer::nextScreenArg=NULL;
}

// Helper class for catching libmail_gpg errors

class EncryptionMenu::DumpFuncHelper {

	string errmsg;

public:
	DumpFuncHelper();
	~DumpFuncHelper();

	static int dump_func(const char *, size_t, void *);

	operator string() const;
};

EncryptionMenu::DumpFuncHelper::DumpFuncHelper()
{
}

EncryptionMenu::DumpFuncHelper::~DumpFuncHelper()
{
}

int EncryptionMenu::DumpFuncHelper::dump_func(const char *msg,
					      size_t cnt, void *vp)
{
	((EncryptionMenu::DumpFuncHelper *)vp)->errmsg += string(msg, msg+cnt);
	return 0;
}

EncryptionMenu::DumpFuncHelper::operator string() const
{
	if (errmsg.size() == 0)
		return strerror(errno);
	return errmsg;
}

void EncryptionMenu::delkey_s()
{
	myServer::promptInfo prompt=
		myServer::promptInfo(_("Delete public key? (Y/N) "))
		.yesno()
		.option(key_PRIVATEKEY,
			Gettext::keyname(_("PRIVATEKEY_K:P")),
			_("Delete private key"));

	prompt=myServer::prompt(prompt);

	if (prompt.abortflag || myServer::nextScreen)
		return;

	bool privateKey=false;

	if ((string)prompt == "N")
		return;

	vector<unicode_char> ka;

	mail::iconvert::convert((string)prompt, unicode_default_chset(), ka);

	if (ka.size() > 0 &&
	    (key_PRIVATEKEY == ka[0]))
		privateKey=true;

	string fingerprint= (GPG::gpg.*(privateKey ?
					&GPG::select_private_key:
					&GPG::select_public_key))
		("", _("DELETE KEY"),
		 _("Select key to delete, then press ENTER"),
		 _("Delete key"), _("Cancel, do not delete")
		 );
	mainScreen->draw();
	statusBar->draw();
	requestFocus();

	if (fingerprint.size() == 0)
		return;

	GPG::Key_iterator ki= privateKey ? GPG::gpg.get_secret_key(fingerprint)
		: GPG::gpg.get_public_key(fingerprint);

	if (!GPG::confirmKeySelection( privateKey ?
				       _("You are about to delete this PRIVATE key:")
				       :
				       _("You are about to delete this public key:"),
				       *ki,
				       NULL,
				       privateKey ?
				       _("Delete this PRIVATE key"):
				       _("Delete this public key"),
				       _("Cancel, do not delete")))
	{
		requestFocus();
		return;
	}
	requestFocus();

	EncryptionMenu::DumpFuncHelper errmsg;

	if (libmail_gpg_deletekey("", privateKey ? 1:0,
				  fingerprint.c_str(),
				  &EncryptionMenu::DumpFuncHelper::dump_func,
				  &errmsg))
	{
		statusBar->clearstatus();
		statusBar->status(_("An attempt to delete this key has "
				    "failed:\n\n") + (string)errmsg);
		statusBar->beepError();
	}
	else
	{
		statusBar->clearstatus();
		statusBar->status(_("Key deleted."));
	}

	GPG::gpg.init();
}

void EncryptionMenu::signkey_s()
{
	string pubkey=GPG::gpg.select_public_key("",
						 _("SELECT PUBLIC KEY"),
						 _("Select key to sign, "
						   "then press ENTER"),
						 _("Sign this key"),
						 _("Cancel, do not sign"));

	mainScreen->draw();
	statusBar->draw();
	requestFocus();

	if (pubkey.size() == 0)
		return;

	string privkey=GPG::gpg.select_private_key("",
						   _("SELECT PRIVATE KEY"),
						   _("Select private key to "
						     "sign the public key with"
						     ),
						   _("Use this key"),
						   _("Cancel, do not sign"));

	mainScreen->draw();
	statusBar->draw();
	requestFocus();

	if (privkey.size() == 0)
		return;

	if (!GPG::confirmKeySelection( _("You are going to sign this public key, with the following private key:"),
				       *GPG::gpg.get_public_key(pubkey),

				       &*GPG::gpg.get_secret_key(privkey),
				       _("Sign the public key with the private key"),
				       _("Cancel, do not sign")))

	{
		requestFocus();
		return;
	}
	requestFocus();

	myServer::promptInfo prompt=
		myServer::promptInfo(_("How carefully did you check the "
				       "private key's identity? "))
		.option(key_GPGNOANSWER, Gettext::keyname(_("GPG_NOANSWER_K:0")),
			_("No answer"))
		.option(key_GPGNOCHECKING,
			Gettext::keyname(_("GPG_NOCHECKING_K:1")),
			_("No checking at all"))
		.option(key_GPGCASUALCHECK,
			Gettext::keyname(_("GPG_CASUALCHECK_K:2")),
			_("Casual checking"))
		.option(key_GPGCAREFULCHECK,
			Gettext::keyname(_("GPG_CAREFULCHECK_K:3")),
			_("Very careful checking"));

	prompt=myServer::prompt(prompt);

	if (prompt.abortflag || myServer::nextScreen)
		return;

	int trustlevel=atoi(((string)prompt).c_str());

	string passphrase;
	string gpgurl="gpg:" + privkey;

	if (!PasswordList::passwordList.check(gpgurl, passphrase))
	{
		myServer::promptInfo passphrasePrompt=
			myServer
			::prompt( myServer
				  ::promptInfo(_("Private key's passphrase "
						 " (if required): "))
				  .password());

		if (passphrasePrompt.abortflag)
			return;

		passphrase=passphrasePrompt;

		PasswordList::passwordList.save(gpgurl, passphrasePrompt);
	}

	FILE *fp=NULL;

	if (passphrase.size() > 0)
	{
		if ((fp=tmpfile()) == NULL ||
		    fprintf(fp, "%s", passphrase.c_str()) < 0 ||
		    fflush(fp) < 0 || fseek(fp, 0L, SEEK_SET) < 0)
		{
			if (fp)
				fclose(fp);
			statusBar->clearstatus();
			statusBar->status(strerror(errno));
			statusBar->beepError();
			return;
		}
	}

	try {
		EncryptionMenu::DumpFuncHelper errmsg;

		if (libmail_gpg_signkey("",
					pubkey.c_str(),
					privkey.c_str(),
					fp ? fileno(fp):-1,
					&EncryptionMenu::DumpFuncHelper
					::dump_func,
					trustlevel,
					&errmsg))
		{
			PasswordList::passwordList.remove(gpgurl);

			statusBar->clearstatus();
			statusBar->status(_("An attempt to sign this key has "
					    "failed:\n\n") + (string)errmsg);
			statusBar->beepError();
		}
		else
		{
			statusBar->clearstatus();
			statusBar->status(_("Key signed."));
		}

		fclose(fp);
	} catch (...) {
		fclose(fp);
		throw;
	}

#if 0
	vector<const char *> argv;

	argv.push_back(gpgprog());
	argv.push_back("--default-key");
	argv.push_back(privkey.c_str());
	argv.push_back("--sign-key");
	argv.push_back(pubkey.c_str());
	argv.push_back(NULL);
	Curses::runCommand(argv, _("Press ENTER to continue: "));
#endif
	GPG::gpg.init();
}

void EncryptionMenu::editkey_s()
{
	string pubkey=GPG::gpg.select_public_key("",
						 _("SELECT KEY TO EDIT"),
						 _("Select key to edit, "
						   "then press ENTER"),
						 _("Edit this key"),
						 _("Cancel, do not edit"));

	mainScreen->draw();
	statusBar->draw();
	requestFocus();

	if (pubkey.size() == 0)
		return;

	vector<const char *> argv;

	argv.push_back(gpgprog());
	argv.push_back("--edit-key");
	argv.push_back(pubkey.c_str());
	argv.push_back(NULL);
	Curses::runCommand(argv, -1, _("Press ENTER to continue: "));
	GPG::gpg.init();
	mail::account::resume();
}

void encryptionMenu(void *dummy)
{
	EncryptionMenu encryptionMenu(mainScreen);

	titleBar->setTitles(_("ENCRYPTION MENU"), "");

	encryptionMenu.requestFocus();

	myServer::eventloop();
}

//////////////////////////////////////////////////////////////////////////////
//
// Generate new key

class EncryptionMenu::NewKey : public CursesContainer {

	CursesLabel title;
	CursesDialog dialog;

	void doGenerate();
	void doCancel();

	CursesLabel nameLabel, addressLabel, commentLabel, sigLengthLabel,
		encLengthLabel, expLabel, passphrase1Label, passphrase2Label;
	CursesField nameField, addressField, commentField;
	CursesChoiceButton sigLengthField, encLengthField;

	CursesContainer expContainer;
	CursesField expNumField;
	CursesChoiceButton expUnitsField;

	CursesField passphraseField1;
	CursesField passphraseField2;

	CursesButtonRedirect<NewKey> generate, cancel;


public:
	NewKey(CursesContainer *parent);
	~NewKey();

	static void newKeyScreen(void *);

private:
	static int dump_func(const char *, size_t, void *);
	static int timeout_func(void *);
};

EncryptionMenu::NewKey::NewKey(CursesContainer *parent)
	: CursesContainer(parent),
	  title(this, _("New Encryption Key")),
	  dialog(this),

	  nameLabel(NULL, "Name: "),
	  addressLabel(NULL, "E-mail address: "),
	  commentLabel(NULL, "Comment: "),
	  sigLengthLabel(NULL, "Signature key length: "),
	  encLengthLabel(NULL, "Encryption key length: "),
	  expLabel(NULL, "Expiration: "),
	  passphrase1Label(NULL, "Passphrase: "),
	  passphrase2Label(NULL, "Re-enter passphrase: "),

	  nameField(NULL),
	  addressField(NULL),
	  commentField(NULL),
	  sigLengthField(NULL),
	  encLengthField(NULL),
	  expContainer(NULL),
	  expNumField(&expContainer, 4, 3),
	  expUnitsField(&expContainer),
	  passphraseField1(NULL),
	  passphraseField2(NULL),
	  generate(NULL, _("Create key")),
	  cancel(NULL, _("Cancel"))
{
	title.setAlignment(title.PARENTCENTER);
	dialog.setAlignment(dialog.PARENTCENTER);

	title.setRow(1);
	dialog.setRow(3);

	size_t r=0;

	dialog.addPrompt(&nameLabel, &nameField, r); ++r;
	dialog.addPrompt(&addressLabel, &addressField, r); ++r;
	dialog.addPrompt(&commentLabel, &commentField, r); r += 2;

	dialog.addPrompt(&sigLengthLabel, &sigLengthField, r); ++r;
	dialog.addPrompt(&encLengthLabel, &encLengthField, r); r += 2;

	expUnitsField.setCol(expNumField.getWidth()+1);
	dialog.addPrompt(&expLabel, &expContainer, r); r += 2;

	passphraseField1.setPasswordChar();
	passphraseField2.setPasswordChar();

	dialog.addPrompt(&passphrase1Label, &passphraseField1, r); ++r;
	dialog.addPrompt(&passphrase2Label, &passphraseField2, r); r += 2;

	generate= &NewKey::doGenerate;
	cancel= &NewKey::doCancel;

	generate=this;
	cancel=this;

	dialog.addPrompt(NULL, &generate, r); r += 2;
	dialog.addPrompt(NULL, &cancel, r);

	{
		vector<string> sigLength;

		sigLength.push_back(_(" 512 bits"));
		sigLength.push_back(_(" 768 bits"));
		sigLength.push_back(_("1024 bits"));

		sigLengthField.setOptions(sigLength, 2);
	}

	{
		vector<string> encLength;

		encLength.push_back(_(" 512 bits"));
		encLength.push_back(_(" 768 bits"));
		encLength.push_back(_("1024 bits"));
		encLength.push_back(_("2048 bits"));

		encLengthField.setOptions(encLength, 2);
	}

	{
		vector<string> expUnits;

		expUnits.push_back(_("-- no expiration --"));
		expUnits.push_back(_("days"));
		expUnits.push_back(_("weeks"));
		expUnits.push_back(_("months"));
		expUnits.push_back(_("years"));

		expUnitsField.setOptions(expUnits, 0);
	}

	nameField.requestFocus();
}

EncryptionMenu::NewKey::~NewKey()
{
}

void EncryptionMenu::NewKey::doGenerate()
{
	string name=nameField.getText();
	string address=addressField.getText();
	string comment=commentField.getText();
	string passphrase1=passphraseField1.getText();
	string passphrase2=passphraseField2.getText();

	if (address.size() == 0)
	{
		addressField.requestFocus();
		statusBar->clearstatus();
		statusBar->status(_("E-mail address is required."));
		statusBar->beepError();
		return;
	}

	if (passphrase1 != passphrase2)
	{
		passphraseField1.requestFocus();
		passphraseField1.setText("");
		passphraseField2.setText("");
		statusBar->clearstatus();
		statusBar->status(_("Enter the same passphrase twice.  Both "
				    "times it must be identical (it wasn't).")
				  );
		statusBar->beepError();
		return;
	}

	int skeylen= 512 + sigLengthField.getSelectedOption() * 256;

	int ekeylen= 1024;

	switch (encLengthField.getSelectedOption()) {
	case 0:
		ekeylen=512;
		break;
	case 1:
		ekeylen=768;
		break;
	case 3:
		ekeylen=2048;
		break;
	}

	unsigned expire=0;

	{
		string expText=expNumField.getText();
		istringstream i(expText);

		i >> expire;
	}

	char expire_unit="\x00dwmy"[encLengthField.getSelectedOption()];


	{
		OutputDialog outputDialog(mainScreen);

		mainScreen->erase();
		mainScreen->draw();

		outputDialog.output(_("Creating new encryption key, please ignore any messages below:\n\n"));

		libmail_gpg_genkey("",
			       unicode_default_chset(),
			       name.c_str(),
			       address.c_str(),
			       comment.c_str(),
			       skeylen,
			       ekeylen,
			       expire,
			       expire_unit,
			       passphrase1.c_str(),
			       &NewKey::dump_func,
			       &NewKey::timeout_func,
			       &outputDialog);

		outputDialog.outputDone();

		if (myServer::nextScreen)
			return;

		myServer::eventloop();
		GPG::gpg.init();
		if (myServer::nextScreen)
			return;
	}

	myServer::nextScreen= &encryptionMenu;
	myServer::nextScreenArg=NULL;
	Curses::keepgoing=false;
	PreviousScreen::previousScreen();
}

void EncryptionMenu::NewKey::doCancel()
{
	myServer::nextScreen= &encryptionMenu;
	myServer::nextScreenArg=NULL;
	Curses::keepgoing=false;
	PreviousScreen::previousScreen();
}

void EncryptionMenu::NewKey::newKeyScreen(void *dummy)
{
	NewKey newKey(mainScreen);

	newKey.setAlignment(newKey.PARENTCENTER);

	titleBar->setTitles(_("NEW ENCRYPTION KEY"), "");

	myServer::eventloop();
}

int EncryptionMenu::NewKey::dump_func(const char *p, size_t n, void *vp)
{
	((OutputDialog *)vp)->output(string(p, p+n));
	mainScreen->flush();
	return 0;
}

int EncryptionMenu::NewKey::timeout_func(void *vp)
{
	((OutputDialog *)vp)->output(".");
	mainScreen->flush();
	return 0;
}

void EncryptionMenu::genkey_s()
{
	myServer::nextScreen= &NewKey::newKeyScreen;
	myServer::nextScreenArg=NULL;
	Curses::keepgoing=false;
}
