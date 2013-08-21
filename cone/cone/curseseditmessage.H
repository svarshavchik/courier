/*
** Copyright 2003-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef curseseditmessage_H
#define curseseditmessage_H

///////////////////////////////////////////////////////////////////////////////
//
// The message editor

#include "config.h"
#include "curses/cursescontainer.H"
#include "curses/curseskeyhandler.H"
#include "curses/cursesflowedline.H"
#include "libmail/objectmonitor.H"
#include "unicode/unicode.h"
#include "spellcheckerbase.H"
#include <time.h>

#include <vector>
#include <string>

#define LINEW 77

namespace mail {
	class Search;
}

class CursesEditMessage : public Curses,
			  public CursesKeyHandler,
			  public mail::obj {

	// Text that's being edited

	std::vector<CursesFlowedLine> text_UTF8;

public:
	size_t numLines() const { return text_UTF8.size(); }
	void getText(size_t line, std::vector<unicode_char> &textRef);
	void getText(size_t line, std::vector<unicode_char> &textRef,
		     bool &wrapped);
	void getText(size_t line, CursesFlowedLine &textRef)
	{
		textRef=line >= text_UTF8.size() ? CursesFlowedLine()
			: text_UTF8[line];
	}

	// Retrieve text in its native UTF-8 format. Optionally append a
	// space if the line of text is flowed.

	std::string getUTF8Text(size_t line,
				bool useflowedformat);

private:
	void setText(size_t line, const std::vector<unicode_char> &textRef);
	void setText(size_t line, const CursesFlowedLine &textRef);

	void getTextBeforeAfter(std::vector<unicode_char> &before,
				std::vector<unicode_char> &after);

	void getTextBeforeAfter(std::vector<unicode_char> &before,
				std::vector<unicode_char> &after,
				bool &flowed);

	void setTextBeforeAfter(const std::vector<unicode_char> &before,
				const std::vector<unicode_char> &after,
				bool flowed=false);

	bool justifiable(size_t lineNum);

	void insertKeyPos(unicode_char key);

	template<typename iter_type>
	void replaceTextLines(size_t start, size_t cnt,
			      iter_type beg_iter, iter_type end_iter);

	template<typename iter_type>
	void replaceTextLinesImpl(size_t start, size_t cnt,
				  iter_type beg_iter, iter_type end_iter);

	template<typename value_type>
	void replaceTextLine(size_t line, const value_type &value);

	template<typename iter_category>
	class replace_text_lines_helper;

	class get_file_helper;

	class unicode_wordwrap_iterator;
	class unicode_wordwrap_rewrapper;

	class unicode_wordwrap_oiterator;
public:
	template<typename iter_category>
	friend class replace_text_lines_helper;
private:
	//
	// Whenever cursor row changes, leaf wants to update the status bar.
	// Rather then remembering to call a function each time the cursor
	// row variable gets modified, the cursor row # is this object, which
	// automatically invokes cursorRowChanged() each time it's poked.
	//

	class CursorRowUpdate {

		size_t r;
	public:
		CursesEditMessage *me;

		CursorRowUpdate() : r(0), me(NULL) {}
		~CursorRowUpdate() {}

		operator size_t()
		{
			return r;
		}

		size_t operator=(size_t newRow)
		{
			r=newRow;
			if (me)
				me->cursorRowChanged();
			return r;
		}

		size_t operator--()
		{
			return ((*this)=r-1);
		}

		size_t operator++()
		{
			return ((*this)=r+1);
		}

	};

	CursorRowUpdate cursorRow;
	size_t cursorCol; // Current cursor column
	size_t cursorLineHorizShift; // Current horiz scroll offset

	bool marked;	// Region being marked
	size_t markRow;	// The other end of the region.
	size_t markCursorPos;

	std::string defaultSearchStr; // Prev search string
	std::string defaultReplaceStr; // Prev replace string

	//
	// Given horizontal position #col, return an iterator to the character
	// in "line" that's shown at the given horizontal position.
	//
	std::vector<unicode_char>
	::iterator getIndexOfCol(std::vector<unicode_char> &line,
				 size_t col);

	//
	// Same thing, except that the starting position of each character
	// in 'line' is precomputed in pos
	//
	static std::vector<unicode_char>
	::iterator getIndexOfCol(std::vector<unicode_char> &line,
				 std::vector<size_t> &pos,
				 size_t colNum);

	// Return the horizontal location of the given character

	static size_t getTextHorizPos(const std::vector<unicode_char> &line,
				      size_t column);

	class SpellCheckerManager;

public:
	virtual void cursorRowChanged();

	CursesEditMessage(CursesContainer *parent);
	~CursesEditMessage();

	static time_t autosaveInterval;

	int getWidth() const;
	int getHeight() const;

	void load(std::istream &i, bool isflowed, bool delsp);
	void save(std::ostream &o, bool isflowed);
	bool isFocusable();
	bool processKeyInFocus(const Key &key);
	int getCursorPosition(int &row, int &col);

	void draw();
	void erase();

	void enterKey();

	void setCurrentLine(size_t row)
	{
		if (row < numLines())
		{
			cursorRow=row;
			cursorCol=0;
		}
	}

	size_t currentLine() { return cursorRow; }

private:
	virtual void macroDefined();
	void processMacroKey(std::string &repl_utf8);

	void inserted();
	void drawLine(size_t lineNum);
	void left(bool);
	void right();
	void deleteChar(bool is_clreol_key);
	bool checkReplace(bool &, std::string &, SpellCheckerBase::Manager *);
	class ReplacePrompt;

	bool search(bool doUpdate, bool doWrap, mail::Search &engine);
	void mark();
	void end();

	void yank(const std::list<CursesFlowedLine> &yankText,
		  const std::string &chset,
		  bool doUpdate);

	bool getMarkedRegion(size_t &row1, size_t &row2,
			     size_t &col1, size_t &col2);

	static size_t colToPos(std::string &, std::vector<size_t> &, size_t,
			       bool);

	Curses::Key lastKey;
	bool processKey(const Curses::Key &key);
	bool listKeys( std::vector< std::pair<std::string, std::string> >
		       &list);

	virtual void modified()=0;
	virtual std::string getConfigDir();
	virtual void extedited();

public:
	static std::string externalEditor;
};

#endif
