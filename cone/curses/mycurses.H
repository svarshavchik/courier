/*
** Copyright 2002-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef curses_H
#define curses_H

#include "../curses/curses_config.h"
#include <unicode.h>
#include <string.h>

#include <wchar.h>
#include <string>
#include <vector>

#include "cursesobject.H"
#include "widechar.H"

class Curses;
class CursesContainer;

///////////////////////////////////////////////////////////////////////////
//
// A simple OO interface to libcurses.
//
// The Curses object is the common abstract superclass.  All other objects
// subclass from Curses.
//
// Curses objects are arranged in a tree hierarchy.  The root of the tree
// is the CursesScreen objects, which implements the WriteText() methods
// that use libcurses.  All Curses objects are anchored at some row/column
// pair, relative to its hierarchy parent.  The Curses object provides a
// default WriteText() implementation that adds the given text position to
// its row/column position, and then calls its parent's WriteText() method.
//
// Two WriteText methods are provided: one for text in the current
// (possibly multibyte) character set; and one for wide-character text.
//
// The remaining methods generally follow a similar path - a default
// Curses implementation that performs local processing, then calls its
// parent's method.

class Curses : public CursesObj {

	// My parent, possibly NULL;

	CursesContainer *parent;

	int row, col;	// My location.

	// Comparison function compares Curses objects based on their
	// location.  This is used to determine input focus tabbing.

	static bool childPositionCompareFunc(Curses *, Curses *);

public:

	// Curses object that receives keyboard input at this time.
	// May be NULL

	static Curses *currentFocus;

	// Keepgoing is initialized to true.  Some objects, (FileReq,
	// StatusBar) reset it to false to indicate that they're done.
	// keepgoing must be manually reset to true.

	static bool keepgoing;

	// Emulate SHIFT keys for keyboards that can't directly generate
	// then.  shiftmode is set to true if the previous keypress was the
	// SHIFT keypress.

	static bool shiftmode;

	static std::string suspendCommand; // Run this instead of SIGSTOP

	static int runCommand(std::vector<const char *> &argv,
			      int stdinpipe,
			      std::string continuePrompt);
	// Disable curses, run the command, reenable curses

	// CursesAttr encapsulates generic text attributes, for WriteText

	class CursesAttr {

		int bgcolor;
		int fgcolor;
		bool highlight;
		bool reverse;
		bool underline;
	public:
		CursesAttr() : bgcolor(0),
			       fgcolor(0),
			       highlight(0), reverse(0), underline(0)
		{
		}
		~CursesAttr()
		{
		}

		CursesAttr &setBgColor(int c)
		{
			bgcolor=c;
			return *this;
		}

		CursesAttr &setFgColor(int c)
		{
			fgcolor=c;
			return *this;
		}

		CursesAttr &setHighlight(bool h=true)
		{
			highlight=h;
			return *this;
		}

		CursesAttr &setReverse(bool r=1)
		{
			reverse=r;
			return *this;
		}

		CursesAttr &setUnderline(bool u=1)
		{
			underline=u;
			return *this;
		}

		int getBgColor() const { return bgcolor; }
		int getFgColor() const { return fgcolor; }
		bool getHighlight() const { return highlight; }
		bool getReverse() const { return reverse; }
		bool getUnderline() const { return underline; }
	};

	// Generic encapsulation of keyboard input.  Keyboard input is either
	// a key, wchar_t, or a special function key, like a cursor key.
	// Rather than pull in all the baggage of curses.h, just to get
	// the key definition, we define our own constants.  Which also
	// makes it possible to define magic keys that are not defined by
	// curses.h

	class Key {
	public:
		unicode_char ukey;
		const char *keycode;

		Key(unicode_char ch) : ukey(ch), keycode(0) {}

		Key(const char *k) : ukey(0), keycode(k) {}

		bool plain() const { return keycode == 0; }

		bool nokey() const { return ukey == 0 && keycode == 0; }

		bool fkey() const { return keycode &&
				strcmp(keycode, "FKEY") == 0; }
		// This is a function key

		int fkeynum() const { return (int)ukey; }

		static const char LEFT[],
			RIGHT[],
			SHIFTLEFT[],
			SHIFTRIGHT[],
			UP[],
			DOWN[],
			SHIFTUP[],
			SHIFTDOWN[],
			DEL[],
			CLREOL[],
			BACKSPACE[],
			ENTER[],
			PGUP[],
			PGDN[],
			SHIFTPGUP[],
			SHIFTPGDN[],
			HOME[],
			END[],
			SHIFTHOME[],
			SHIFTEND[],
			SHIFT[],
			RESIZE[];

		bool operator==(const char *p) const
		{
			return keycode != 0 && strcmp(keycode, p) == 0;
		}

#if 0
		bool operator==(unicode_char k) const
		{
			return keycode == 0 && key == k;
		}
#endif

		bool operator==(const Key &k) const
		{
			return strcmp(keycode ? keycode:"",
				      k.keycode ? k.keycode:"") == 0 &&
				ukey == k.ukey;
		}

		bool operator==(const std::vector<unicode_char> &v) const;

		bool operator!=(const char *p) const
		{
			return !operator==(p);
		}

#if 0
		bool operator!=(wchar_t k) const
		{
			return !operator==(k);
		}
#endif
		bool operator!=(const std::vector<unicode_char> &v) const
		{
			return !operator==(v);
		}
	};

	// This humble function received and handles keyboard input.
	// processKey() runs either an installed key handler, or runs
	// currentFocus->processKeyInFocus().  When a file requester, or
	// StatusBar input prompt is active, at exit keepgoing may be set
	// to false (ENTER key closes the file requester or status bar input
	// field).

	static bool processKey(const Key &k);

	// How to interpret Curses::col

	enum Alignment {
		LEFT,		// col is the left corner
		CENTER,		// col is the center of this Curses object
		RIGHT,		// col is the right corner
		PARENTCENTER};	// Ignore col, center this object in its parent

	Curses(CursesContainer *parent=0);
	virtual ~Curses();

	// Subclasses must define getWidth() and getHeight() to provide their
	// sizes.

	virtual int getWidth() const=0;
	virtual int getHeight() const=0;

	// Children of CursesVScroll may not have their entire contents
	// shown.  To optimize their draw() implementation, instead of
	// writing out their contents in entire, getVerticalViewport may
	// be called to obtain the first line of this object that's actually
	// viewable, and the total number of rows that are viewable.
	// Calls to write text to lines outside this range will be no-oped.
	// nrows may be 0, if this object is entirely off-screen.
	//
	// The default Curses implementation sets first_row to 0, and
	// nrows to getHeight(), then calls the getVerticalViewPort() method
	// of its parent.  The results of parent's GetVerticalViewPort(),
	// combined with this object's getRow() method, is used to potentially
	// narrow down the first_row/nrows range.  For example, if the parent's
	// vertical viewport is rows 5-9, and this object's row is 4, and
	// its height is 10, then the resulting first_row/nrows will be 1/5
	// (instead of 0/10).

	virtual void getVerticalViewport(size_t &first_row,
					 size_t &nrows);

	// The default implementation of getScreenWidth()/getScreenHeight()
	// walk up the parent chain, and invoke the hierarchy root's Curses
	// object's getWidth/getHeight methods, which should reflect the
	// should be the actual screen size.

	int getScreenWidth() const;
	int getScreenHeight() const;

	// get/set row/col methods have an obvious default implementation,
	// that some subclasses may override (the usual reason is to
	// automatically redraw the curses object if it moves)

	virtual int getRow() const;
	virtual int getCol() const;
	virtual void setRow(int row);
	virtual void setCol(int col);

	// The default implementation of scrollTo adds the supplied row number
	// to the starting row of this curses object, and recursively
	// calls the parent's scrollTo() method.  CursesVScroll overrides
	// this method to make sure that the indicated row is currently
	// visible.

	virtual void scrollTo(size_t row);

	CursesContainer *getParent() { return (parent); }
	const CursesContainer *getParent() const { return (parent); }
	void setParent(CursesContainer *p) { parent=p; }

	// If CursesContainer sees an isDialog() child, it will get drawn
	// instead of all the other children.  Uses by CursesFileReq, for
	// example, to take over the display.

	virtual bool isDialog() const;

	// Find my dialog child

	virtual Curses *getDialogChild() const;

	// draw() must be subclassed to invoked WriteText() to actually
	// print the Curses object's contents.

	virtual void draw()=0;

	// The default implementation of erase() calls getWidth()/getHeight()
	// then writes out a bunch of whitespace to clear everything out.

	virtual void erase();

	// The default implementation of flush() calls the parent's flush()
	// method.  The top level CursesScreen object calls libcurses.a
	// instead.

	virtual void flush();

	//  The screen has been resized.
	//  The default implementation of resized() just calls draw()
	//

	virtual void resized();

	static int getColorCount();
	// # of colors supported by display (0 if no colors)

	//  See the beginning of this file for a description of writeText().

	virtual bool writeText(const char *text, int row, int col,
			       const CursesAttr &attr) const;
	virtual bool writeText(const std::vector<unicode_char> &text,
			       int row, int col,
			       const Curses::CursesAttr &attr) const;

	void writeText(std::string text, int row, int col,
		       const CursesAttr &attr) const;

	// Beep the terminal.

	virtual void beepError();

private:
	Alignment alignment;

public:
	// Return the coordinates of the top/left corner of this object,
	// in the context of this parent, after taking into account all
	// the alignment information.

	int getRowAligned() const;
	int getColAligned() const;

	virtual void setAlignment(Alignment alignmentArg);
	virtual Alignment getAlignment();

	// Indicate whether this object can handle keyboard input
	// (default: no).

	virtual bool isFocusable();

	// If this object is handling keyboard input, return the next or the
	// previous object, in the natural tabbing order, that should receive
	// keyboard input.

	virtual Curses *getNextFocus();
	virtual Curses *getPrevFocus();

	// Actually move the keyboard focus to the next or prev curses object.

	virtual void transferNextFocus();
	virtual void transferPrevFocus();

	// Explicitly request keyboard input

	virtual void requestFocus();

	// Callback function that is invoked whenever this object begins
	// handling keyboard input.  The default implementation just calls
	// draw()

	virtual void focusGained();

	// Callback function that is invoked whenever this object stops
	// handling keyboard input.  The default implementation just calls
	// draw()

	virtual void focusLost();

	// Remove keyboard input from any object that's currently receiving
	// keyboard input (explicitly calling focusLost()).  This can be
	// useful to force focusLost() processing (to make sure that any
	// further popup notice activity won't cause any side effects).
	// Eventually, somebody's requestFocus() method should be called
	// to reenable input processing

	static void dropFocus();

	// Return whether this object is handling keyboard input

	virtual bool hasFocus();

	// Handle keyboard input.  The default implementation changes
	// keyboard input focus in response to basic cursor movement
	// keys.  Subclasses typically override, but also call the superclass's
	// method for the default action.
	// processKeyInFocus returns true if the key was processed, or if the
	// function did not recognize and process the key.

	virtual bool processKeyInFocus(const Key &key);

	// Map the coordinates of a character sell in this Curses object to
	// the screen coordinates.  The default implementation just adds
	// row/col to the top/left starting coords of this object, then runs
	// the parent's getCursorPosition() method.

	virtual int getCursorPosition(int &row, int &col);


	// Set a callback function invoked after a suspend

	static void setSuspendHook( void (*)(void) );

	static void (*suspendedHook)(void);
private:
	static void suspendedStub(void);

};

#endif
