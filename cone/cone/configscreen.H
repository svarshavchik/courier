/*
** Copyright 2003-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef configscreen_H
#define configscreen_H

#include "config.h"

///////////////////////////////////////////////////////////////////////////
//
// The configuration screen

#include "curses/mycurses.H"
#include "curses/cursesdialog.H"
#include "curses/curseslabel.H"
#include "curses/cursesfield.H"
#include "curses/cursesbutton.H"
#include "curses/curseskeyhandler.H"
#include "curses/curseschoicebutton.H"

#include "macros.H"

#include <set>
#include <string>

struct CustomColor;

class ConfigScreen : public CursesDialog,
		     public CursesKeyHandler {

public:

	class CertificateButton : public CursesButton, public CursesKeyHandler {

		bool processKey(const Curses::Key &key);

		void showtext();
	public:
		std::string cert;

		CertificateButton(std::string certArg);
		~CertificateButton();

		void clicked();
	};

private:
	// Buttons used for addresses, folders, headers, etc...

	class Button : public CursesButton {

		ConfigScreen *parent;
		void (ConfigScreen::*deleteCallback)(Button *);

	public:
		std::string address;

		Button(ConfigScreen *parentArg,
		       void (ConfigScreen::*deleteCallbackArg)(Button *),
		       std::string textArg);
		~Button();
		bool processKeyInFocus(const Key &key);
	};

	class MacroButton : public CursesButton {
		ConfigScreen *parent;

	public:
		Macros::name macroName;

		MacroButton(ConfigScreen *parentArg,
			    Macros::name &macroNameArg,
			    std::string macroNameAsStr);
		~MacroButton();
		bool processKeyInFocus(const Key &key);
	};

	class MacroButtonSortHelper {
	public:
		MacroButtonSortHelper() {}
		~MacroButtonSortHelper() {}
		bool operator()(MacroButton *a,
				MacroButton *b)
		{
			return a->macroName < b->macroName;
		}
	};

	// Various fields:

	CursesLabel myAddressLabel;
	std::set<Button *> addresses;

	CursesFieldRedirect<ConfigScreen> myAddressAdd;

	CursesChoiceButton htmlDemoronization;
	CursesLabel postAndMailLabel;
	CursesChoiceButton postAndMail;

	CursesLabel myListAddressLabel;
	std::set<Button *> listAddresses;

	CursesFieldRedirect<ConfigScreen> myListAddressAdd;

	CursesLabel myHeadersLabel;
	std::set<Button *> headers;

	CursesFieldRedirect<ConfigScreen> myHeadersAdd;


	CursesLabel sentFoldersLabel;
	std::set<Button *> sentFolders;

	CursesLabel dictionaryLabel;
	CursesField dictionaryField;

	CursesLabel smtpServerLabel;
	CursesField smtpServer;

	CursesLabel smtpCertificateLabel;
	CertificateButton smtpCertificateButton;

	CursesLabel smtpServerUIDLabel;
	CursesField smtpServerUID;

	CursesButton smtpServerCRAM;
	CursesButton smtpUseIMAP;
	CursesButton smtpUseSSL;

	CursesLabel nntpCommandLabel;
	CursesField nntpCommand;

	CursesLabel suspendLabel;
	CursesField suspendCommand;

	CursesLabel editorLabel;
	CursesField editorCommand;

	CursesButton moronizationEnabled;
	CursesButton quitNoPrompt;

	CursesLabel autosaveLabel;
	CursesField autosaveField;

	CursesLabel watchDaysLabel;
	CursesField watchDaysField;

	CursesLabel watchDepthLabel;
	CursesField watchDepthField;

	CursesLabel gpgopts1Label;
	CursesField gpgopts1Field;

	CursesLabel gpgopts2Label;
	CursesField gpgopts2Field;

	std::vector<CursesLabel *> tagLabels;
	std::vector<CursesField *> tagFields;


	// Custom colors

	std::vector<CursesLabel *> colorLabels;

	class ColorButton : public CursesButton {

		ConfigScreen *parent;
	public:

		struct CustomColor *colorInfo;
		int currentColor;

		ColorButton(ConfigScreen *parent, struct CustomColor *);
		~ColorButton();

		void clicked();
	};

	std::vector<ColorButton *> colorButtons;

	std::set<MacroButton *> macroButtons;
	std::vector<Macros::name> deletedMacros;

	CursesButtonRedirect<ConfigScreen> resetRemote;

	CursesLabel macrosLabel;

	CursesButtonRedirect<ConfigScreen> save, cancel;

	void addAddress();
	void addListAddress();

	void delAddress(Button *);
	void delListAddress(Button *);
	void delSentFolder(Button *);
	void delMacro(MacroButton *);

	void addHeader();
	void delHeader(Button *);
public:
	ConfigScreen(CursesContainer *parent);
	~ConfigScreen();

	static std::string suspendCommandStr;

private:
	void init();

	void addPrompt(CursesLabel *label,
		       CursesField *field,
		       size_t atRow);
	void addPrompt(CursesLabel *label,
		       Curses *field,
		       size_t atRow);

	// Inherited from CursesKeyHandler:

	bool processKey(const Curses::Key &key);
	bool listKeys( std::vector< std::pair<std::string, std::string> >
		       &list);

	void doSave();
	void doCancel();
	void doResetRemote();
};

#endif

