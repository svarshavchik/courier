/*
** Copyright 2002-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef cursesscreen_H
#define cursesscreen_H

#include "../curses/curses_config.h"
#include <courier-unicode.h>
#include "cursescontainer.H"

#if HAVE_NCURSESW_CURSES_H
#include <ncursesw/curses.h>
#else
#include <curses.h>
#endif

#include <iconv.h>
#include <vector>

//
// A libcurses implementation.  A CursesScreen object is typically the
// root object of the Curses hierarchy.  The constructor initializes libcurses,
// the destructor cleans it up.
//

class CursesScreen : public CursesContainer {

	int save_w, save_h; // Fix some libcurses.a resizing bugs.

	int inputcounter;

	bool underline_hack;

	// Read keyboard input, convert to char32_ts

	class KeyReader {

		iconv_t h;

		std::vector<char> input_buf;
		std::vector<char> winput_buf;

		size_t winput_cnt;

	public:
		KeyReader();
		~KeyReader();

		void operator<<(char);

		bool operator>>(char32_t &);
	};

	KeyReader keyreader;

	class repltabs_spaces;

public:
	CursesScreen();
	~CursesScreen();

	// Calculate max size.
	int getWidth() const override;
	int getHeight() const override;

	void flush() override;
	void draw() override;

	class writetext_iter_helper;

	bool writeText(const char *text, int row, int col,
		       const Curses::CursesAttr &attr) const override;

	bool writeText(const std::u32string &text, int row, int col,
		       const Curses::CursesAttr &attr) const override;

	// Return keyboard input.  Returns Key::nokey() if no keyboard input
	// isavailable.
	Key getKey();

	void beepError() override;
private:

	Key doGetKey();
	Key doGetPlainKey(char32_t);

public:
	void resized() override;
};

#endif
