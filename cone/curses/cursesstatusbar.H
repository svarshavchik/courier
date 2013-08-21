/*
** Copyright 2002-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef cursesstatusbar_H
#define cursesstatusbar_H

#include "mycurses.H"
#include "curseskeyhandler.H"
#include "cursescontainer.H"
#include "cursesfield.H"

#include "timer.H"

#include <string>
#include <vector>
#include <wchar.h>

//////////////////////////////////////////////////////////////////////////////
//
// The status bar - moves itself to the bottom of the screen.  Occupies three
// last lines: a one line status/help line, and two lines that list current
// keyboard shortcuts.
//
// Status messages are prioritized, and only messages of the same or higher
// priority level are shown.  The clearstatus() clears the current message,
// which is also automatically cleared 10 seconds after the first keypress
//
// Messages that cannot fit on one line are automatically word-wrapped, and
// are shown in a larger popup that comes up from the bottom of the screen,
// which must be cleared with a keypress
//
// The status bar can also be converted to a one-line input field, by calling
// the createPrompt() method, which returns a pointer to the created
// CursesField object (which may be further customized).  The status bar
// automatically destroys the CursesField abort when the input concludes
// (by pressing ENTER, or by manually calling fieldAbort() ).
//
// The first character of the status line may be a busy-throbber (a rotating
// line).  The throbber must be animated by repeatedly calling the busy()
// method (which should be kicked off a 1 second timer).  The throbber is
// cleared by clearstatus().
//
// The status bar also includes a summary of all installed KeyHandler
// objects.  If the list of available function keys exceeds the available
// space, the list is shown in pieces, with a control key cycling through
// to the next status key summary piece.

class CursesScreen;

class CursesStatusBar : public CursesContainer, public CursesKeyHandler {

	std::vector<unicode_char> statusText;
	std::vector<unicode_char> progressText;
	time_t progressTime;

	int busyCounter;

	CursesScreen *parentScreen;

	std::vector< std::vector<unicode_char> > extendedErrorMsg;

	// The status bar is sometimes turned into a one-line field entry.
	// This is done by creating a CursesField child, which is why
	// CursesStatusBar subclasses CursesContainer.
	// We need to subclass CursesField in order to fix up a few things...

	class Field : public CursesField {
		CursesStatusBar *me;
	public:
		Field(CursesStatusBar *parent, size_t widthArg,
		      size_t maxlengthArg,
		      std::string initValue);
		~Field();

		bool processKeyInFocus(const Key &key);

		bool writeText(const char *text, int row, int col,
			       const CursesAttr &attr) const;
		bool writeText(const std::vector<unicode_char> &text,
			       int row, int col,
			       const Curses::CursesAttr &attr) const;
	};

	Field *fieldActive;      // Not NULL when input takes place

	std::string fieldValue;  // Entered input
	bool fieldAborted;       // Whether input was aborted

	TimerRedirect<CursesStatusBar> clearStatusTimer;

	CursesAttr attrStatusBar, attrHotKey, attrHotKeyDescr;

public:
	void fieldEnter();	// Accept input
	void fieldAbort();	// Abort input
private:

	// Shortcut keys.

	size_t max_sc_nlen;	// Largest shortcut name
	size_t max_sc_dlen;	// Largest shortcut description

	std::vector <std::vector <std::vector <
		std::pair< std::vector<unicode_char>,
			   std::vector<unicode_char> > > > > shortcuts;

	// A std::pair of name/description string, inside a two-element vector
	// (the two-row column on the status bar), inside a vector of all
	// columns that fit on one screen, inside a vector of all pages of
	// shortcut descriptions

	size_t currentShortcutPage;	// Current page of shortcuts shown

	void rebuildShortcuts();	// Rebuild the shortcuts array
public:

	static std::string shortcut_next_key;
	static std::string shortcut_next_descr;
	static unicode_char shortcut_next_keycode;

	friend class Field;

	// Status line priorities

	enum statusLevel {
		NORMAL,		// Normal message
		INPROGRESS,	// Something's in progress
		EXPUNGED,	// Folder's been expunged
		SYSERROR,	// System error
		DISCONNECTED,	// Server dropped connection
		SERVERERROR,	// Other server error
		LOGINERROR	// Login error
	};

	CursesStatusBar(CursesScreen *parent);
	~CursesStatusBar();

	static std::string extendedErrorPrompt; // Non empty - wrapped text

	int getWidth() const;
	int getHeight() const;

	void setStatusBarAttr(CursesAttr);
	void setHotKeyAttr(CursesAttr);
	void setHotKeyDescr(CursesAttr);

	void draw();
	void resized();

	void status(std::string statusText, statusLevel level=NORMAL);
	void progress(std::string progress);
	bool progressWanted();

	void clearstatus();

	void busy();	// Throbber animation

	void notbusy(); // Not busy any more

	// Create a one-line input field.

	CursesField *createPrompt(std::string prompt, std::string initvalue="");

	bool prompting() const { return fieldActive != NULL; }
	bool promptAborted() const { return fieldAborted; }

	std::string getPromptValue() const
	{
		return (fieldActive != NULL ? fieldActive->getText()
			: fieldValue);
	}

private:
	statusLevel currentLevel;

	void resetRow();

	bool processKey(const Curses::Key &key);
	bool listKeys( std::vector< std::pair<std::string, std::string> > &list);
};

#endif
