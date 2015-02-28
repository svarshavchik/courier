/*
** Copyright 2003-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "config.h"
#include "curseseditmessage.H"
#include "fkeytraphandler.H"
#include "curses/cursesstatusbar.H"
#include "curses/cursesmainscreen.H"
#include "curses/cursesmoronize.H"
#include "colors.H"
#include "gettext.H"
#include "htmlentity.h"
#include "myserverpromptinfo.H"
#include "macros.H"
#include "maildir/maildirsearch.h"
#include <courier-unicode.h>
#include "opendialog.H"
#include "libmail/mail.H"

#include <fstream>
#include <vector>
#include <iterator>
#include <algorithm>

#include <errno.h>
#include <string.h>

extern Gettext::Key key_ALL;
extern Gettext::Key key_CUT;
extern Gettext::Key key_JUSTIFY;
extern Gettext::Key key_EDITSEARCH;
extern Gettext::Key key_EDITREPLACE;
extern Gettext::Key key_GETFILE;
extern Gettext::Key key_DICTSPELL;
extern Gettext::Key key_EXTEDITOR;

extern Gettext::Key key_REPLACE0;
extern Gettext::Key key_REPLACE1;
extern Gettext::Key key_REPLACE2;
extern Gettext::Key key_REPLACE3;
extern Gettext::Key key_REPLACE4;
extern Gettext::Key key_REPLACE5;
extern Gettext::Key key_REPLACE6;
extern Gettext::Key key_REPLACE7;
extern Gettext::Key key_REPLACE8;
extern Gettext::Key key_REPLACE9;

extern Gettext::Key key_ABORT;
extern Gettext::Key key_MACRO;

extern Gettext::Key key_REPLACE_K;
extern Gettext::Key key_IGNORE_K;
extern Gettext::Key key_IGNOREALL_K;

extern unicode_char ularr, urarr;
extern unicode_char ucwrap;

extern CursesStatusBar *statusBar;
extern CursesMainScreen *mainScreen;

extern Demoronize *currentDemoronizer;

extern SpellCheckerBase *spellCheckerBase;

time_t CursesEditMessage::autosaveInterval=60;
std::string CursesEditMessage::externalEditor;

extern struct CustomColor color_md_headerName;

////////////////////////////////////////////////////////////////////////////
//
// Helper class instantiated when spell checking begins.
// When this object goes out of scope, the spell checker object gets
// automatically cleaned up.
//

class CursesEditMessage::SpellCheckerManager {

public:
	SpellCheckerBase::Manager *manager;

	SpellCheckerManager();
	~SpellCheckerManager();

	operator SpellCheckerBase::Manager *();

	SpellCheckerBase::Manager *operator ->();
};

CursesEditMessage::SpellCheckerManager::SpellCheckerManager()
	: manager(NULL)
{
}

CursesEditMessage::SpellCheckerManager::~SpellCheckerManager()
{
	if (manager)
		delete manager;
}

CursesEditMessage::SpellCheckerManager::operator SpellCheckerBase::Manager *()
{
	return manager;
}

SpellCheckerBase::Manager *CursesEditMessage::SpellCheckerManager::operator->()
{
	return manager;
}

CursesEditMessage::CursesEditMessage(CursesContainer *parent)
	: Curses(parent), CursesKeyHandler(PRI_SCREENHANDLER),
	  cursorCol(0),
	  cursorLineHorizShift(0), marked(false),
	  lastKey((unicode_char)0)
{
	text_UTF8.push_back(CursesFlowedLine());
	// Always have at least one line in text_UTF8
	cursorRow.me=this;
}

CursesEditMessage::~CursesEditMessage()
{
}

void CursesEditMessage::cursorRowChanged()
{
}

int CursesEditMessage::getWidth() const
{
	return getScreenWidth();
}

int CursesEditMessage::getHeight() const
{
	size_t n=numLines();

	size_t h=getScreenHeight();

	if (h > n)
		n=h;

	return n;
}

bool CursesEditMessage::isFocusable()
{
	return true;
}

//
// Get a line of text as unicode characters
//

void CursesEditMessage::getText(size_t line,
				std::vector<unicode_char> &textRef)
{
	bool dummy;

	return getText(line, textRef, dummy);
}

void CursesEditMessage::getText(size_t line,
				std::vector<unicode_char> &textRef,
				bool &flowedRet)
{
	textRef.clear();

	if (line >= numLines())
		return;

	const CursesFlowedLine &l=text_UTF8[line];

	unicode::iconvert::convert(l.text, "utf-8", textRef);
	flowedRet=l.flowed;
}

std::string CursesEditMessage::getUTF8Text(size_t i,
					   bool useflowedformat)
{
	CursesFlowedLine line;

	getText(i, line);

	if (useflowedformat)
	{
		if (line.flowed)
			line.text += " ";
		else
		{
			std::string::iterator b, e;

			for (b=line.text.begin(), e=line.text.end();
			     b != e; --e)
			{
				if (e[-1] != ' ')
					break;
			}

			line.text.erase(e, line.text.end());
		}
	}

	return line.text;
}

//
// Save line of text as unicode chars.
//

void CursesEditMessage::setText(size_t line,
				const std::vector<unicode_char> &textRef)
{
	if (line >= numLines())
		abort();

	text_UTF8[line]=CursesFlowedLine(textRef, false);
}

void CursesEditMessage::setText(size_t line,
				const CursesFlowedLine &textRef)
{
	if (line >= numLines())
		abort();

	text_UTF8[line]=textRef;
}

void CursesEditMessage::getTextBeforeAfter(std::vector<unicode_char> &before,
					   std::vector<unicode_char> &after)
{
	bool dummy;

	getTextBeforeAfter(before, after, dummy);
}

void CursesEditMessage::getTextBeforeAfter(std::vector<unicode_char> &before,
					   std::vector<unicode_char> &after,
					   bool &flowed)
{
	// Get current line

	{
		CursesFlowedLine line;

		getText(cursorRow, line);

		flowed=line.flowed;
		unicode::iconvert::convert(line.text, "utf-8", before);
	}

	// Find character cursor is at.

	std::vector<unicode_char>::iterator pos
		=getIndexOfCol(before, cursorCol);

	// Split the line at that point.

	after.clear();
	after.insert(after.end(), pos, before.end());

	before.erase(pos, before.end());
}

//
// Set the contents of the current line by concatenating two unicode characters
// and position the cursor between the two parts.
//
void CursesEditMessage
::setTextBeforeAfter(const std::vector<unicode_char> &before,
		     const std::vector<unicode_char> &after,
		     bool flowed)
{
	widecharbuf origline;

	origline.init_unicode(before.begin(), before.end());

	std::vector<unicode_char> newline;

	newline.reserve(before.size()+after.size());

	newline.insert(newline.end(), before.begin(), before.end());
	newline.insert(newline.end(), after.begin(), after.end());

	setText(cursorRow, CursesFlowedLine(newline, flowed));

	cursorCol=origline.wcwidth(0);
}

template<>
void CursesEditMessage::replaceTextLine(size_t line,
					const std::vector<unicode_char> &value)
{
	text_UTF8[line]=CursesFlowedLine(value, false);
}

template<>
void CursesEditMessage::replaceTextLine(size_t line,
					const CursesFlowedLine &value)
{
	text_UTF8[line]=value;
}

//
// Replace a range of existing text lines. New content is defined by
// a beginning and ending iterator, that must iterate over
// vector<unicode_char>.
//
// The iterators must be random access iterators.

template<typename iter_type>
void CursesEditMessage::replaceTextLinesImpl(size_t start, size_t cnt,
					     iter_type beg_iter,
					     iter_type end_iter)
{
	if (start > text_UTF8.size() || text_UTF8.size() - start < cnt)
		return;

	modified();

	text_UTF8.erase(text_UTF8.begin() + start,
			text_UTF8.begin() + start + cnt);

	cnt=end_iter - beg_iter;

	text_UTF8.reserve(text_UTF8.size()+cnt);

	text_UTF8.insert(text_UTF8.begin() + start, cnt,
			 CursesFlowedLine());

	while (cnt)
	{
		--cnt;
		replaceTextLine(start, *beg_iter);
		++start;
		++beg_iter;
	}

	cursorRow=start;
	cursorCol=0;
}

template<>
class CursesEditMessage::replace_text_lines_helper
<std::random_access_iterator_tag> {

public:

	template<typename iter_type>
	static void do_replace(CursesEditMessage *obj,
			       size_t start, size_t cnt,
			       iter_type beg_iter,
			       iter_type end_iter)
	{
		obj->replaceTextLinesImpl(start, cnt, beg_iter, end_iter);
	}
};

template<typename non_random_access_iterator_tag>
class CursesEditMessage::replace_text_lines_helper {

public:

	template<typename iter_type>
	static void do_replace(CursesEditMessage *obj,
			       size_t start, size_t cnt,
			       iter_type beg_iter,
			       iter_type end_iter)
	{
		size_t n=256;

		do
		{
			std::vector<typename std::iterator_traits<iter_type>
				    ::value_type> lines;

			lines.reserve(n);

			while (beg_iter != end_iter && lines.size() < n)
			{
				lines.push_back(*beg_iter);
				++beg_iter;
			}

			replace_text_lines_helper
				<std::random_access_iterator_tag>
				::do_replace(obj, start, cnt,
					     lines.begin(), lines.end());
			cnt=0;
			start += lines.size();
		} while (beg_iter != end_iter);
	}
};

template<typename iter_type>
void CursesEditMessage::replaceTextLines(size_t start, size_t cnt,
					 iter_type beg_iter,
					 iter_type end_iter)
{
	typedef replace_text_lines_helper<typename std::iterator_traits<iter_type>
					  ::iterator_category> helper_t;

	cursorRow=start+1;

	helper_t::do_replace(this, start, cnt, beg_iter, end_iter);
	if (text_UTF8.size() == 0)
		text_UTF8.push_back(CursesFlowedLine());
	// Always at least one line

	/*
	** Required semantics: cursor is always on the last line of
	** the inserted text.
	*/
	if (cursorRow >= text_UTF8.size())
		cursorRow=text_UTF8.size();

	if (cursorRow > 0)
		--cursorRow;
}

bool CursesEditMessage::justifiable(size_t lineNum)
{
	if (lineNum < text_UTF8.size())
	{
		const CursesFlowedLine &l=text_UTF8[lineNum];

		if (l.text.size() > 0)
			switch (*l.text.begin()) {
			case '>':
			case ' ':
			case '\t':
				break;
			default:
				return true;
			}
	}
	return false;
}

void CursesEditMessage::insertKeyPos(unicode_char k)
{
	if ( k >= 0 && k < ' ' && k != '\t')
		return;

	std::vector<unicode_char> before, after;
	bool origWrapped;

	getTextBeforeAfter(before, after, origWrapped);

	before.insert(before.end(), k);

	if (CursesMoronize::enabled)
	{
		unicode_char buf[CursesMoronize::max_keycode_len+1];

		size_t i;

		std::vector<unicode_char>::iterator
			b(before.begin()),
			e(before.end());

		for (i=0; i < sizeof(buf)/sizeof(buf[0])-1; ++i)
		{
			if (b == e)
				break;
			--e;

			buf[i]=*e;
		}

		buf[i]=0;

		if (i > 0)
		{
			std::vector<unicode_char> repl_c;

			buf[i]=0;
			i=CursesMoronize::moronize(buf, repl_c);

			if (i > 0)
			{
				before.erase(before.end()-i, before.end());
				before.insert(before.end(), repl_c.begin(),
					      repl_c.end());
			}
		}
	}

	std::string *macroptr=NULL;

	Macros *mp=Macros::getRuntimeMacros();

	if (mp)
	{
		std::map<Macros::name, std::string> &macroMap=mp->macroList;

		std::map<Macros::name, std::string>::iterator b=macroMap.begin(),
			e=macroMap.end();

		for ( ; b != e; ++b)
		{
			if (b->first.f)
				continue;


			if (b->first.n.size() > before.size() ||
			    !std::equal(b->first.n.begin(),
					b->first.n.end(),
					before.end() - b->first.n.size()))
				continue;

			before.erase(before.end() - b->first.n.size(),
				     before.end());
				     
			macroptr= &b->second;
			break;
		}
	}

	{
		widecharbuf wc;

		wc.init_unicode(before.begin(), before.end());
		cursorCol=wc.wcwidth(0);
	}

	before.insert(before.end(), after.begin(),
		      after.end());
	setText(cursorRow, CursesFlowedLine(before, origWrapped));

	if (macroptr)
	{
		processMacroKey(*macroptr);
		return;
	}

	inserted();
}

//
// A bare-bones iterator that retrieves lines from a file and converts
// them to unicode characters.
//

class CursesEditMessage::get_file_helper
	: public std::iterator<std::input_iterator_tag, CursesFlowedLine> {

	std::istream *i;

	CursesFlowedLine line;

	bool isflowed;
	bool delsp;

	void binaryfile()
	{
		statusBar->clearstatus();
		statusBar->status(_("Binary file? You must be joking."),
				  statusBar->SYSERROR);
		statusBar->beepError();
		i=NULL;
	}

	void nextline(bool first)
	{
		std::string linetxt;

		if (i == NULL)
			return;

		getline(*i, linetxt);

		if (i->fail() && !i->eof())
		{
			i=NULL;
			return;
		}

		if (i->eof() && linetxt.size() == 0)
		{
			i=NULL;
			return;
		}

		if (first) // Check if it's a binary file
			for (std::string::iterator
				     b(linetxt.begin()),
				     e(linetxt.end()); b != e;
			     ++b)
			{
				if (*b == 0)
				{
					binaryfile();
					return;
				}
			}

		bool flowedflag=false;

		std::vector<unicode_char> uline;

		unicode::iconvert::convert(linetxt,
					unicode_default_chset(),
					uline);

		if (isflowed)
		{
			std::vector<unicode_char>::iterator b(uline.begin()),
				e(uline.end());

			while (b != e && *b == '>')
				++b;

			if (b != e && *b == ' ')
				++b;

			if (b != e && e[-1] == ' ')
			{
				flowedflag=true;
				if (delsp)
					uline.pop_back();
			}
		}

		// Edit out control characters
		std::vector<unicode_char>::iterator
			b=uline.begin(), e=uline.end(), c=b;

		for ( ; b != e; ++b)
		{
			if (*b < ' ' && *b != '\t')
				continue;

			*c++=*b;
		}

		uline.erase(c, e);

		line=CursesFlowedLine(uline, flowedflag);
	}

public:
	get_file_helper(std::istream &iArg,
			bool isflowedArg,
			bool delspArg) : i(&iArg),
					 isflowed(isflowedArg),
					 delsp(delspArg)
	{
		nextline(true);
	}

	get_file_helper() : i(NULL), isflowed(false), delsp(false)
	{
	}

	const CursesFlowedLine &operator*()
	{
		return line;
	}

	get_file_helper &operator++()
	{
		nextline(false);
		return *this;
	}

	bool operator==(const get_file_helper &o)
	{
		return i == NULL && o.i == NULL;
	}

	bool operator!=(const get_file_helper &o)
	{
		return !operator==(o);
	}
};

//
// Justification rewrap helper. unicode_wordwrap_iterator saves the starting
// offset of each line into starting_offsets, as it iterates over the
// unjustified text.

class CursesEditMessage::unicode_wordwrap_rewrapper {

public:
	mutable std::list<size_t> starting_offsets;

	bool operator()(size_t n) const
	{
		while (!starting_offsets.empty() &&
		       starting_offsets.front() < n)
			starting_offsets.pop_front();

		return !starting_offsets.empty() &&
			starting_offsets.front() == n;
	}
};

//
// Iterator over unjustified text, for unicodewordwrap()
//

class CursesEditMessage::unicode_wordwrap_iterator
	: public std::iterator<std::input_iterator_tag,
			       unicode_char> {

	CursesEditMessage *msg;
	size_t start_row, end_row;
	unicode_wordwrap_rewrapper *rewrapper;

	size_t cnt;

public:
	unicode_wordwrap_iterator() // Ending iterator
		: msg(NULL), start_row(0), end_row(0),
		  rewrapper(NULL), cnt(0)
	{
	}

	unicode_wordwrap_iterator(// The message object
				  CursesEditMessage *msgArg,

				  // First row to iterate over
				  size_t start_rowArg,

				  // Last row+1 to iterate over
				  size_t end_rowArg,

				  // Save starting offsets of each row
				  // in here.
				  unicode_wordwrap_rewrapper &rewrapperArg)
		: msg(msgArg), start_row(start_rowArg),
		  end_row(end_rowArg),
		  rewrapper(&rewrapperArg),
		  cnt(0)
	{
		newline();
	}

	// Test for begin=end equality

	bool operator==(const unicode_wordwrap_iterator &o) const
	{
		return start_row == end_row && o.start_row == o.end_row;
	}

	bool operator!=(const unicode_wordwrap_iterator &o) const
	{
		return !operator==(o);
	}

private:

	// Current unjustified line
	std::vector<unicode_char> line;

	// Current iterator over the line
	std::vector<unicode_char>::iterator b, e;

	// Advanced to the next line in the unjustified text. Set up the
	// beginning and the ending iterator
	void newline()
	{
		while (start_row < end_row)
		{
			msg->getText(start_row, line);

			b=line.begin();
			e=line.end();

			// Trim any trailing space

			while (b != e)
			{
				if (e[-1] != ' ')
					break;
				--e;
			}

			if (b != e)
			{
				rewrapper->starting_offsets.push_back(cnt);
				break;
			}

			// Ignore empty lines
			++start_row;
		}
	}

public:

	// Iterator operation
	unicode_char operator*() const
	{
		return *b;
	}

	// Increment iterator
	unicode_wordwrap_iterator &operator++()
	{
		if (start_row < end_row)
		{
			++cnt;

			if (++b == e)
			{
				++start_row;
				newline();
			}
		}
		return *this;
	}
};

// Collect wrapped content into a flowed line list

class CursesEditMessage::unicode_wordwrap_oiterator
	: public std::iterator<std::output_iterator_tag, void, void, void, void>
{

	unicode_wordwrap_oiterator(const unicode_wordwrap_oiterator &);
	unicode_wordwrap_oiterator &operator=(const unicode_wordwrap_oiterator
					      &);

public:
	std::list<CursesFlowedLine> wrapped;

	unicode_wordwrap_oiterator() {}

	unicode_wordwrap_oiterator &operator++() { return *this; }
	unicode_wordwrap_oiterator &operator++(int) { return *this; }
	unicode_wordwrap_oiterator &operator*() { return *this; }

	unicode_wordwrap_oiterator &operator=(const std::vector<unicode_char>
					      &u)
	{
		wrapped.push_back(CursesFlowedLine(u, true));
		return *this;
	}
       
};

bool CursesEditMessage::processKeyInFocus(const Key &key)
{
	Key lastKeyProcessed=lastKey;

	lastKey=key;

	if (cursorRow >= numLines())
		abort();

	// TODO: Check for plain key

	if (key == key.SHIFTLEFT || key == key.SHIFTRIGHT ||
	    key == key.SHIFTUP || key == key.SHIFTDOWN ||
	    key == key.SHIFTHOME || key == key.SHIFTEND ||
	    key == key.SHIFTPGUP || key == key.SHIFTPGDN)
	{
		if (!marked)
			mark();
	}
	else if (marked && key != key.DEL && key != key.BACKSPACE &&
		 key != key_CUT && key != key.CLREOL && key != key_MACRO)
	{
		marked=false;
		draw();
	}

	if (key == key.UP || key == key.SHIFTUP)
	{
		if (cursorRow == 0)
			transferPrevFocus();
		else
		{
			--cursorRow;
			drawLine(cursorRow+1);
		}
		drawLine(cursorRow);
		return true;
	}

	if (key == key.DOWN || key == key.SHIFTDOWN)
	{
		if (cursorRow + 1 >= numLines())
			transferNextFocus();
		else
		{
			++cursorRow;
			drawLine(cursorRow-1);
		}
		drawLine(cursorRow);
		return true;
	}

	if (key == key.LEFT || key == key.SHIFTLEFT)
	{
		left(true);
		return true;
	}

	if (key == key.RIGHT || key == key.SHIFTRIGHT)
	{
		right();
		return true;
	}

	if (key == key.HOME || key == key.SHIFTHOME)
	{
		cursorCol=0;
		cursorLineHorizShift=0;
		drawLine(cursorRow);
		return true;
	}

	if (key == key.END || key == key.SHIFTEND)
	{
		end();
		return true;
	}

	if (key == key.PGUP || key == key.SHIFTPGUP)
	{
		return Curses::processKeyInFocus(key);
#if 0
		size_t dummy;
		size_t h;

		getVerticalViewport(dummy, h);

		drawLine(cursorRow);
		cursorRow -= cursorRow >= h ? h:cursorRow;
		drawLine(cursorRow);
		return true;
#endif
	}

	if (key == key.PGDN || key == key.SHIFTPGDN)
	{
		return Curses::processKeyInFocus(key);
	}

	if (key == key.ENTER)
	{
		enterKey();
		draw();
		return true;
	}

	if (key == key.BACKSPACE)
	{
		left(false);
		deleteChar(false);
		return true;
	}

	if (key == key.DEL || (marked && key == key_CUT))
	{
		deleteChar(false);
		return true;
	}

	if (key == key_JUSTIFY)
	{
		if (lastKeyProcessed == key_JUSTIFY) // Undo justification
		{
			if (CursesField::cutBuffer.empty())
				return true;

			// Find where the justified paragraph begins.

			size_t r=cursorRow+1;

			while (r > 0)
			{
				--r;

				if (!justifiable(r))
				{
					++r;
					break;
				}
			}

			erase(); // Clear the screen.

			replaceTextLines(r, cursorRow-r+1,
					 CursesField::cutBuffer.begin(),
					 CursesField::cutBuffer.end());
			CursesField::cutBuffer.clear();
			lastKey=Key((unicode_char)0); // Not really
			draw();
			return true;
		}

		// We can only justify if the cursor is on a non-empty
		// line that does not begin with quoted text.

		if (!justifiable(cursorRow))
		{
			lastKey=Key((unicode_char)0); // Not really
			return true;
		}

		// Find start of paragraph

		while (cursorRow > 0)
		{
			if (!justifiable(--cursorRow))
			{
				++cursorRow;
				break;
			}
		}

		size_t startingRow=cursorRow;

		CursesField::cutBuffer.clear();

		// Original, unjustified text goes here.

		while (cursorRow < numLines())
		{
			if (!justifiable(cursorRow))
				break;

			CursesFlowedLine line;
			getText(cursorRow, line);
			CursesField::cutBuffer.push_back(line);
			++cursorRow;
		}
		erase();

		unicode_wordwrap_oiterator insert_iter;

		unicode_wordwrap_rewrapper rewraphelper;

		unicodewordwrap(unicode_wordwrap_iterator(this,
							  startingRow,
							  cursorRow+1,
							  rewraphelper),
				unicode_wordwrap_iterator(this,
							  startingRow,
							  startingRow,
							  rewraphelper),
				rewraphelper,
				insert_iter,
				LINEW, false);

		if (insert_iter.wrapped.empty())
			insert_iter.wrapped.push_back(CursesFlowedLine());

		insert_iter.wrapped.back().flowed=false;
		// Last line does not flow

		replaceTextLines(startingRow, cursorRow-startingRow,
				 insert_iter.wrapped.begin(),
				 insert_iter.wrapped.end());

		marked=false;
		draw();
		statusBar->status(_("^J: undo justification"));
		return true;
	}

	if (key == key.CLREOL)
	{
		if (lastKeyProcessed != key.CLREOL)
			CursesField::cutBuffer.clear();

		mark();
		end();
		deleteChar(true);
		return true;
	}

	if (key == key.SHIFT)
	{
		statusBar->clearstatus();
		if (shiftmode)
			statusBar->status(_("Mark set"));

		return true;
	}

	if (key == key_EDITSEARCH || key == key_EDITREPLACE)
	{
		MONITOR(CursesEditMessage);

		myServer::promptInfo response=
			myServer
			::prompt(myServer
				 ::promptInfo(Gettext(_("Search: ")))
				 .initialValue(defaultSearchStr));

		if (DESTROYED() || response.abortflag ||
		    response.value.size() == 0)
			return true;

		defaultSearchStr=response.value;

		// We match in UTF-8, convert the search string to UTF8,
		// and initialize the search engine.

		mail::Search searchEngine;

		bool doSmartReplace=true;

		// If the search string is completely in lowercase, do a
		// "smart" replace (replace wholly uppercase strings with
		// uppercase replacement text, and replace title-cased string
		// with title-cased replacement text).

		{
			std::vector<unicode_char> ubuf;

			unicode::iconvert::convert(defaultSearchStr,
						unicode_default_chset(),
						ubuf);

			for (std::vector<unicode_char>::iterator
				     b(ubuf.begin()), e(ubuf.end()); b != e;
			     ++b)
			{
				if (*b != unicode_lc(*b))
				{
					doSmartReplace=false;
					break;
				}
			}
		}

		if (!searchEngine.setString(defaultSearchStr.c_str(),
					    unicode_default_chset()))
			return false;

		if (searchEngine.getSearchLen() == 0)
			return true;

		if (key == key_EDITSEARCH) // A single search
		{
			search(true, true, searchEngine);
			return true;
		}

		// Search/replace

		response=myServer
			::prompt(myServer
				 ::promptInfo(Gettext(_("Replace: ")))
				 .initialValue(defaultReplaceStr));

		if (DESTROYED() || response.abortflag)
			return true;

		defaultReplaceStr=response.value;

		std::string upperReplaceStr=defaultReplaceStr;
		std::string titleReplaceStr=defaultReplaceStr;

		{
			std::vector<unicode_char> ubuf;

			if (unicode::iconvert::convert(defaultReplaceStr,
						    unicode_default_chset(),
						    ubuf))
			{
				std::vector<unicode_char> uc=ubuf;

				for (std::vector<unicode_char>::iterator
					     b(uc.begin()),
					     e(uc.end());
				     b != e; ++b)
					*b=unicode_uc(*b);

				bool err;

				std::string r(unicode::iconvert::convert
					      (uc, unicode_default_chset(),
					       err));

				if (!err)
					upperReplaceStr=r;

				uc=ubuf;
				if (uc.size() != 0)
					uc[0]=unicode_tc(uc[0]);

				r=unicode::iconvert::convert
					(uc, unicode_default_chset(), err);

				if (!err)
					titleReplaceStr=r;
			}
		}

		size_t replaceCnt=0;
		bool globalReplace=false;

		while (search(!globalReplace, false, searchEngine))
		{
			if (!globalReplace)
			{
				response=
					myServer
					::promptInfo(Gettext(_("Replace"
							       "? (Y/N) ")))
					.yesno()
					.option( key_ALL,
						 Gettext::keyname(_("ALL_K:A")
								  ),
						 _("Replace All"));

				response=myServer::prompt(response);

				if (DESTROYED() || response.abortflag)
					return true;

				if (key_ALL == response.firstChar())
					globalReplace=true;
				else if (response.value != "Y")
					continue;
			}

			CursesField::cutBuffer.clear();
			deleteChar(false); // Found search string is marked.

			std::string replaceStr=defaultReplaceStr;

			if (doSmartReplace && !CursesField::cutBuffer.empty())
			{
				std::vector<unicode_char> ubuf;

				if (unicode::iconvert::
				    convert(CursesField::cutBuffer.front().text,
					    "utf-8", ubuf))
				{
					std::vector<unicode_char>::iterator
						b, e;

					for (b=ubuf.begin(),
						     e=ubuf.end(); b != e; ++b)
						if (unicode_uc(*b) != *b)
							break;

					if (b == e)
						replaceStr=upperReplaceStr;
					else if (ubuf.size() > 0 &&
						 ubuf[0] == unicode_tc(ubuf[0]))
						replaceStr=titleReplaceStr;
				}
			}

			CursesField::cutBuffer.clear();
			CursesField::cutBuffer.push_back(replaceStr);

			yank(CursesField::cutBuffer,
			     unicode_default_chset(),
			     false);

			++replaceCnt;
		}
		draw();
		statusBar->clearstatus();
		statusBar->status(Gettext(_("Replaced %1% occurences."))
				  << replaceCnt);
		return true;
	}

	if (key == key_GETFILE)
	{
		std::string filename;

		{
			OpenDialog open_dialog;

			open_dialog.noMultiples();

			open_dialog.requestFocus();
			myServer::eventloop();

			std::vector<std::string> &filenameList=
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

		std::ifstream i(filename.c_str());

		if (!i.is_open())
		{
			statusBar->clearstatus();
			statusBar->status(strerror(errno),
					  statusBar->SYSERROR);
			statusBar->beepError();
			return true;
		}

		get_file_helper beg_iter(i, false, false), end_iter;

		replaceTextLines(cursorRow < numLines() ?
				 cursorRow:numLines(), 0,
				 beg_iter, end_iter);
		draw();
		return true;
	}

	if (key == key_DICTSPELL)
	{
		std::vector<unicode_char> currentLine;
		size_t row=cursorRow;

		getText(row, currentLine);

		size_t pos=getIndexOfCol(currentLine, cursorCol)
			- currentLine.begin();

		std::string errmsg;

		SpellCheckerBase::Manager *managerPtr=
			spellCheckerBase->getManager(errmsg);

		if (!managerPtr)
		{
			statusBar->status(errmsg);
			statusBar->beepError();
			return true;
		}

		SpellCheckerManager manager;

		manager.manager=managerPtr;

		MONITOR(CursesEditMessage);

		bool needPrompt=true;

		while (!DESTROYED())
		{
			if (needPrompt)
			{
				statusBar->clearstatus();
				statusBar->status(_("Spell checking..."));
				statusBar->flush();
				needPrompt=false;
			}

			if ((currentLine.size() > 0 && currentLine[0] == '>')
			    // Do not spellcheck quoted content
			    ||
			    pos >= currentLine.size())
			{
				if (++row >= numLines())
					break;

				currentLine.clear();
				getText(row, currentLine);
				pos=0;
				continue;
			}

			// Grab the next word

			size_t i;

			{
				unicode::wordbreakscan scanner;

				for (size_t j=pos; j<currentLine.size(); ++j)
					if (scanner << currentLine[j])
						break;

				i=pos + scanner.finish();
			}

			if (i == pos)
			{
				++pos;
				continue;
			}

			// Convert word to utf-8

			std::string word_c;

			{
				std::vector<unicode_char> word;

				word.insert(word.end(),
					    currentLine.begin()+pos,
					    currentLine.begin()+i);
				word.push_back(0);

				word_c=unicode::iconvert::convert(word, "utf-8");
			}

			bool found_flag;
			std::string errmsg;

			// Spell check.

			if (word_c.size() == 0 ||
			    (errmsg=manager->search(word_c, found_flag))
			    .size() > 0)
			{
				statusBar->clearstatus();
				statusBar->status(errmsg);
				statusBar->beepError();
				break;
			}

			if (found_flag)
			{
				pos=i;
				continue;
			}

			// Highlight misspelled word

			cursorRow=row;
			cursorCol=getTextHorizPos(currentLine, i);
			mark();

			int dummy1, dummy2;
			getCursorPosition(dummy1, dummy2);
			// Make sure starting position is shown.

			cursorCol=getTextHorizPos(currentLine, pos);
			drawLine(cursorRow);

			bool doReplace;
			std::string origWord=word_c;

			needPrompt=true;

			if (!checkReplace(doReplace, word_c, manager.manager))
			{
				marked=false;
				drawLine(row);
				statusBar->draw();
				statusBar->clearstatus();
				return true;
			}

			std::vector<unicode_char> uc;


			if (doReplace &&
			    unicode::iconvert::convert(word_c, "utf-8",
						    uc))
			{
				modified();

				currentLine.erase(currentLine.begin() + pos,
						  currentLine.begin()+i);

				currentLine.insert(currentLine.begin() + pos,
						   uc.begin(), uc.end());

				setText(row, currentLine);

				pos += uc.size();

				cursorCol=getTextHorizPos(currentLine, pos);
				manager->replace(origWord, word_c);
			}
			else
			{
				pos=i;
			}
			marked=false;
			drawLine(row);
		}

		statusBar->status(_("Spell checking completed."));
		statusBar->draw();
		return true;
	}

	if (key == key_EXTEDITOR)
	{
		if (externalEditor.size() == 0)
			return true; // leaf

		statusBar->clearstatus();
		statusBar->status(_("Starting editor..."));
		statusBar->flush();

		std::string msgfile=getConfigDir() + "/message.tmp";

		std::ofstream o(msgfile.c_str());

		if (!o.is_open())
		{
			statusBar->clearstatus();
			statusBar->status(strerror(errno));
			statusBar->beepError();
			return true;
		}

		save(o, true);
		o.flush();
		if (o.fail() || o.bad())
		{
			statusBar->clearstatus();
			statusBar->status(strerror(errno));
			statusBar->beepError();
			return true;
		}
		o.close();

		std::vector<const char *> args;

		{
			const char *p=getenv("SHELL");

			if (!p || !*p)
				p="/bin/sh";

			args.push_back(p);
		}

		args.push_back("-c");

		erase();
		flush();
		int rc;

		{
			std::string cmd=externalEditor + " " + msgfile;

			args.push_back(cmd.c_str());
			args.push_back(0);

			rc=Curses::runCommand(args, -1, "");
		}
		draw();
		cursorRow=0;
		cursorCol=0;
		{
			int dummy1, dummy2;

			getCursorPosition(dummy1, dummy2);
		}

		extedited();
		if (rc)
		{
			unlink(msgfile.c_str());

			statusBar->clearstatus();
			statusBar->
				status(Gettext(_("The external editor command "
						 "failed.  Check that the "
						 "configured external editor, "
						 "\"%1%\", exists."))
				       << externalEditor);
			statusBar->beepError();
			return true;
		}

		std::ifstream i(msgfile.c_str());

		if (!i.is_open())
		{
			unlink(msgfile.c_str());
			statusBar->clearstatus();
			statusBar->status(strerror(errno));
			statusBar->beepError();
			return true;
		}

		get_file_helper beg_iter(i, true, true), end_iter;

		replaceTextLines(0, numLines(), beg_iter, end_iter);
		cursorRow=0;
		i.close();
		unlink(msgfile.c_str());
		draw();
		return true;
	}

	if (key == key_MACRO)
	{
		Macros *m=Macros::getRuntimeMacros();

		if (!m)
			return true; // Leaf

		myServer::promptInfo macroNamePrompt=
			myServer::promptInfo(marked ?
					     _("Define macro shortcut: ")
					     :
					     _("Undefine macro shortcut: "));

		macroNamePrompt.optionHelp
			.push_back(std::make_pair(Gettext
					     ::keyname(_("ENTER:Enter")),
					     marked ? 
					     _("Define shortcut")
					     :
					     _("Clear shortcut")
					     ));
		macroNamePrompt.optionHelp
			.push_back(std::make_pair(Gettext::keyname(_("FKEY:Fn")),
					     marked ?
					     _("Assign shortcut to"
					       " function key")
					     :
					     _("Undefine function key shortcut")
					     ));

		Macros::name macroName(0);

		{
		        FKeyTrapHandler macroHelper;

			macroNamePrompt=myServer::prompt(macroNamePrompt);

			if (macroHelper.defineFkeyFlag)
			{
				macroName=Macros::name(macroHelper.fkeyNum);
			}
			else
			{
				if (macroNamePrompt.abortflag)
					return true;

				std::vector<unicode_char> v;

				unicode::iconvert::convert(((std::string)
							 macroNamePrompt),
							unicode_default_chset(),
							v);

				std::vector<unicode_char>::iterator b, e;

				for (b=v.begin(), e=v.end(); b != e; ++b)
				{
					if ((unsigned char)*b == *b &&
					    strchr(" \t\r\n",
						   (unsigned char)*b))
					{
						statusBar->clearstatus();
						statusBar->status(_("Macro name may not contain spaces."));
						statusBar->beepError();
						return true;
					}
				}

				if (v.size() == 0)
					return true;

				macroName=Macros::name(v);
			}
		}

		if (myServer::nextScreen)
			return true;

		if (!marked)
		{
			std::map<Macros::name, std::string>::iterator p=
				m->macroList.find(macroName);

			if (p != m->macroList.end())
				m->macroList.erase(p);

			macroDefined();
			statusBar->clearstatus();
			statusBar->status(_("Macro undefined."));
			return true;
		}

		size_t row1, pos1, row2, pos2;

		getMarkedRegion(row1, row2, pos1, pos2);

		if (row1 == row2 && pos1 == pos2)
			return true; // Shouldn't happen


		std::string cutText;

		size_t i;

		for (i=row1; i <= row2; i++)
		{
			std::vector<unicode_char> u;

			getText(i, u);

			if (i == row2)
				u.erase(u.begin() + pos2, u.end());

			if (i == row1)
				u.erase(u.begin(), u.begin() + pos1);

			if (i < row2)
				u.push_back('\n');

			cutText += unicode::iconvert::convert(u, "utf-8");
		}

		std::map<Macros::name, std::string>::iterator p=
			m->macroList.find(macroName);

		if (p != m->macroList.end())
			m->macroList.erase(p);

		m->macroList.insert(std::make_pair(macroName, cutText));

		macroDefined();
		statusBar->clearstatus();
		statusBar->status(_("Macro defined."));
		return true;
	}

	if (key.fkey())
	{
		Macros::name fk(key.fkeynum());

		Macros *mp=Macros::getRuntimeMacros();

		if (!mp)
			return true;

		std::map<Macros::name, std::string> &m=mp->macroList;

		std::map<Macros::name, std::string>::iterator p=
			m.find(fk);

		if (p != m.end())
			processMacroKey(p->second);
		return true;
	}

	if (key.plain())
	{
		if (key.ukey == '\x03')
			return false;

		if (key.ukey == CursesField::yankKey)
		{
			yank(CursesField::cutBuffer,
			     unicode_default_chset(),
			     true);

			return true;
		}

		insertKeyPos(key.ukey);
		return true;
	}

	return false;
}

void CursesEditMessage::macroDefined()
{
}

void CursesEditMessage::processMacroKey(std::string &repl_utf8)
{
	std::list<CursesFlowedLine> flowedlist;

	std::string::const_iterator b(repl_utf8.begin()), e(repl_utf8.end());

	while (b != e)
	{
		std::string::const_iterator p=std::find(b, e, '\n');

		flowedlist.push_back(std::string(b, p));

		if ((b=p) != e)
			++b;
	}

	yank(flowedlist, "utf-8", true);
}

std::string CursesEditMessage::getConfigDir()
{
	return "";
}

void CursesEditMessage::extedited()
{
}

//////////////////////////////////////////////////////////////////////////
//
// Spell check replace prompt: suggestions, or manual choices.

class CursesEditMessage::ReplacePrompt : public CursesKeyHandler {

	std::vector<std::string> &wordlist;

	bool processKey(const Curses::Key &key);
	bool listKeys( std::vector< std::pair<std::string, std::string> > &list);

public:

	enum ReplaceAction {
		replaceCustom,
		replaceWord,
		replaceIgnore,
		replaceIgnoreAll,
		replaceAbort} replaceAction;

	std::string replaceWord_UTF8;

	ReplacePrompt( std::vector<std::string> &wordlistArg);
	~ReplacePrompt();

	ReplaceAction prompt();
};

bool CursesEditMessage::checkReplace(bool &doReplace, std::string &replaceWord,
				     SpellCheckerBase::Manager *manager)
{
	std::vector<std::string> suggestions;
	std::string errmsg;

	if (!manager->suggestions(replaceWord, suggestions, errmsg))
	{
		statusBar->clearstatus();
		statusBar->status(errmsg);
		statusBar->beepError();
		return false;
	}

	MONITOR(CursesEditMessage);

	for (;;)
	{
		ReplacePrompt::ReplaceAction action;
		std::string actionWord;

		{
			statusBar->status(_("Replace misspelled word with:"));
			ReplacePrompt prompt(suggestions);

			action=prompt.prompt();
			actionWord=prompt.replaceWord_UTF8;
		}

		if (myServer::nextScreen || DESTROYED())
			return false; // Something happened...

		Curses::keepgoing=true;

		switch (action) {
		case ReplacePrompt::replaceWord:
			replaceWord=actionWord;
			doReplace=true;
			return true;
		case ReplacePrompt::replaceIgnore:
			doReplace=false;
			return true;
		case ReplacePrompt::replaceIgnoreAll:
			if ((errmsg=manager->addToSession(replaceWord)).size()
			    > 0)
			{
				statusBar->clearstatus();
				statusBar->status(errmsg);
				statusBar->beepError();
				return false;
			}
			doReplace=false;
			return true;
		case ReplacePrompt::replaceAbort:
			return false;
		default:
			break;
		}

		myServer::promptInfo response=
			myServer
			::prompt(myServer
				 ::promptInfo(Gettext(_("Replace with: "))));

		if (response.abortflag ||
		    response.value.size() == 0)
			continue;

		replaceWord=unicode::iconvert::convert(std::string(response),
						    unicode_default_chset(),
						    "utf-8");
		break;
	}

	doReplace=true;
	return true;
}

CursesEditMessage::ReplacePrompt::ReplacePrompt( std::vector<std::string> &wordlistArg)
	: CursesKeyHandler(PRI_STATUSHANDLER), wordlist(wordlistArg),
	  replaceAction(replaceCustom)
{
}

CursesEditMessage::ReplacePrompt::~ReplacePrompt()
{
}

bool CursesEditMessage::ReplacePrompt::listKeys( std::vector< std::pair<std::string, std::string> >
						 &list)
	// List of suggestions.
{
	std::string keyname[10];

	keyname[0]=Gettext::keyname(_("REPLACE0:0"));
	keyname[1]=Gettext::keyname(_("REPLACE1:1"));
	keyname[2]=Gettext::keyname(_("REPLACE2:2"));
	keyname[3]=Gettext::keyname(_("REPLACE3:3"));
	keyname[4]=Gettext::keyname(_("REPLACE4:4"));
	keyname[5]=Gettext::keyname(_("REPLACE5:5"));
	keyname[6]=Gettext::keyname(_("REPLACE6:6"));
	keyname[7]=Gettext::keyname(_("REPLACE7:7"));
	keyname[8]=Gettext::keyname(_("REPLACE8:8"));
	keyname[9]=Gettext::keyname(_("REPLACE9:9"));

	size_t i;

	for (i=0; i<10 && i<wordlist.size(); i++)
	{
		bool errflag;

		std::string s=unicode::iconvert::convert(wordlist[i], "utf-8",
						      unicode_default_chset(),
						      errflag);

		if (errflag)
			continue;

		list.push_back(std::make_pair(keyname[i], s));
	}
	list.push_back(std::make_pair(Gettext::keyname(_("REPLACE:R")),
				 _("Replace")));
	list.push_back(std::make_pair(Gettext::keyname(_("IGNORE:I")),
				 _("Ignore")));
	list.push_back(std::make_pair(Gettext::keyname(_("IGNOREALL:A")),
				 _("Ignore All")));

	return true;
}

bool CursesEditMessage::ReplacePrompt::processKey(const Curses::Key &key)
{
	Gettext::Key *keys[10];

	keys[0]=&key_REPLACE0;
	keys[1]=&key_REPLACE1;
	keys[2]=&key_REPLACE2;
	keys[3]=&key_REPLACE3;
	keys[4]=&key_REPLACE4;
	keys[5]=&key_REPLACE5;
	keys[6]=&key_REPLACE6;
	keys[7]=&key_REPLACE7;
	keys[8]=&key_REPLACE8;
	keys[9]=&key_REPLACE9;

	size_t i;

	for (i=0; i<10 && i<wordlist.size(); i++)
	{
		if (key == *keys[i])
		{
			replaceAction=replaceWord;
			replaceWord_UTF8=wordlist[i];
			Curses::keepgoing=false;
			return true;
		}
	}

	if (key == key_REPLACE_K)
	{
		replaceAction=replaceCustom;
		Curses::keepgoing=false;
		return true;
	}

	if (key == key_IGNORE_K)
	{
		replaceAction=replaceIgnore;
		Curses::keepgoing=false;
		return true;
	}

	if (key == key_IGNOREALL_K)
	{
		replaceAction=replaceIgnoreAll;
		Curses::keepgoing=false;
		return true;
	}


	if (key == key_ABORT)
	{
		replaceAction=replaceAbort;
		Curses::keepgoing=false;
		return true;
	}

	statusBar->beepError();
	return true;
}

CursesEditMessage::ReplacePrompt::ReplaceAction
CursesEditMessage::ReplacePrompt::prompt()
{
	Curses::keepgoing=true;
	myServer::nextScreen=NULL;
	myServer::nextScreenArg=NULL;

	myServer::eventloop();

	if (myServer::nextScreen)
		return replaceAbort;

	return replaceAction;
}

//////////////////////////////////////////////////////////////////////////


std::vector<unicode_char>::iterator
CursesEditMessage::getIndexOfCol(std::vector<unicode_char> &line,
				 size_t colNum)
{
	widecharbuf wc;

	size_t col=0;

	std::vector<unicode_char>::iterator b(line.begin()),
		e(line.end()), p=b,
		retvec=b;

	while (1)
	{
		if (b != e)
		{
			if (b == p || !unicode_grapheme_break(b[-1], *b))
			{
				++b;
				continue;
			}
		}

		if (b > p)
		{
			wc.init_unicode(p, b);

			size_t grapheme_width=wc.wcwidth(col);

			col += grapheme_width;

			if (col > colNum)
				break;
			retvec=b;
		}

		p=b;

		if (b == e)
			break;
	}

	return retvec;
}

std::vector<unicode_char>::iterator
CursesEditMessage::getIndexOfCol(std::vector<unicode_char> &line,
				 std::vector<size_t> &pos,
				 size_t colNum)
{
	size_t n=pos.size()-1;

	while (n > 0)
	{
		if (pos[n] <= colNum)
			break;
		--n;
	}

	return line.begin() + n;

#if 0
	std::vector<unicode_char>::iterator indexp=line.begin();

	std::vector<unicode_char>::iterator b=line.begin(), e=line.end();

	size_t col=0;

	while (b != e)
	{
		if (col <= colNum)
			indexp=b;

		if (*b == '\t')
		{
			col += WRAPTABSIZE;
			col -= (col % WRAPTABSIZE);
		}
		else
			col++;
		b++;
	}
	if (col <= colNum)
		indexp=e;

	return indexp;
#endif
}

void CursesEditMessage::mark()
{
	std::vector<unicode_char> line;

	getText(cursorRow, line);

	markRow=cursorRow;
	markCursorPos= getIndexOfCol(line, cursorCol) - line.begin();
	marked=true;
}

void CursesEditMessage::end()
{
	std::vector<unicode_char> line;

	getText(cursorRow, line);

	widecharbuf wc;

	wc.init_unicode(line.begin(), line.end());
	
	cursorCol=wc.wcwidth(0);
	drawLine(cursorRow);
}

void CursesEditMessage::yank(const std::list<CursesFlowedLine> &yankText,
			     const std::string &chset,
			     bool doUpdate)
{
	size_t origRow=cursorRow;

	std::vector<unicode_char> before, after;
	bool flowed;

	getTextBeforeAfter(before, after, flowed);

	if (yankText.empty())
		return;

	replaceTextLines(origRow, 1, yankText.begin(), yankText.end());

	std::vector<unicode_char> line;
	bool pasteflowed;

	getText(origRow, line, pasteflowed);
	line.insert(line.begin(), before.begin(), before.end());
	setText(origRow, CursesFlowedLine(line, pasteflowed));

	getText(cursorRow, line, pasteflowed);
	setTextBeforeAfter(line, after, flowed);
	marked=false;

	if (doUpdate)
		draw();
}

bool CursesEditMessage::search(bool doUpdate, bool doWrap,
			       mail::Search &searchEngine)
{
	searchEngine.reset();

	std::vector<unicode_char> line;

	getText(cursorRow, line);

	size_t cursorPos=getIndexOfCol(line, cursorCol) - line.begin();

	size_t searchRow=cursorRow;
	size_t searchPos=cursorPos;

	size_t startRow=searchRow;
	size_t startPos=searchPos;

	std::vector<unicode_char> lineuc;

	// We begin the search in the middle of the line, so load a partial
	// line into lineuc, but make sure the cursor position doesn't change.

	lineuc.insert(lineuc.end(), cursorPos, ' ');
	lineuc.insert(lineuc.end(), line.begin() + cursorPos, line.end());

	size_t rowNum=numLines()+1;
	// Make sure the whole text buffer gets searched

	bool wrappedAround=false;

	while ( !searchEngine )
	{
		if (searchEngine.atstart())
		{
			startRow=searchRow;
			startPos=searchPos;
		}

		if (searchPos >= lineuc.size())
		{
			searchEngine << (unicode_char)' ';
			// Simulated whitespace

			if (++searchRow >= numLines())
			{
				wrappedAround=true;
				searchRow=0;
				if (!doWrap)
					return false;
			}

			lineuc.clear();

			std::vector<unicode_char> uc;

			getText(searchRow, uc);

			lineuc.insert(lineuc.end(), uc.begin(), uc.end());

			searchPos=0;

			if (--rowNum == 0)
			{
				statusBar->clearstatus();
				statusBar->status(_("Not found"));
				return false;
			}
		}
		else
		{
			searchEngine << lineuc[searchPos];
			++searchPos;
		}
	}

	// Found the search string, select it

	if (wrappedAround)
	{
		statusBar->clearstatus();
		statusBar->status(_("Wrapped around from the beginning"));
	}

	marked=true;
	markRow=startRow;
	markCursorPos=startPos;

	cursorRow=searchRow;

	getText(cursorRow, line);

	{
		widecharbuf wc;

		wc.init_unicode(line.begin(),
				searchPos < line.size() ?
				line.begin()+searchPos:line.end());
		cursorCol=wc.wcwidth(0);
	}

	int dummy1, dummy2;
	getCursorPosition(dummy1, dummy2); // Make sure on screen
	if (doUpdate)
		draw();
	return true;
}


bool CursesEditMessage::getMarkedRegion(size_t &row1, size_t &row2,
					size_t &pos1, size_t &pos2)
{
	bool swapped=false;

	row1=markRow;
	pos1=markCursorPos;

	std::vector<unicode_char> before, after;

	getTextBeforeAfter(before, after);

	row2=cursorRow;
	pos2=before.size();

	if (row2 < row1 || (row2 == row1 && pos2 < pos1))
	{
		size_t swap;

		swap=row1;
		row1=row2;
		row2=swap;

		swap=pos1;
		pos1=pos2;
		pos2=swap;

		swapped=true;
	}

	return swapped;
}

void CursesEditMessage::deleteChar(bool is_clreol_key)
{
	modified();

	if (marked)
	{
		// Delete currently selected region.

		marked=false;

		size_t row1, pos1, row2, pos2;

		getMarkedRegion(row1, row2, pos1, pos2);

		if (row1 != row2 || pos1 != pos2)
		{
			erase();

			// save text in the cut buffer

			if (!is_clreol_key)
				CursesField::cutBuffer.clear();

			size_t i;

			CursesFlowedLine line1, line2;
			std::vector<unicode_char> u;

			for (i=row1; i <= row2; i++)
			{
				getText(i, line1);

				unicode::iconvert::convert(line1.text, "utf-8", u);

				if (i == row2)
					u.erase(u.begin() + pos2, u.end());

				if (i == row1)
					u.erase(u.begin(), u.begin() + pos1);

				line1.text=unicode::iconvert::convert(u, "utf-8");

				if (is_clreol_key)
				{
					if (CursesField::cutBuffer.empty())
						CursesField::cutBuffer
							.push_back(line1);

					CursesField::cutBuffer.back()=line1;
				}
				else
					CursesField::cutBuffer.push_back(line1);
			}

			// Get the first and the last line in the cut region
			// Splice the before and after text, together.

			std::vector<unicode_char> uline1, uline2;

			getText(row1, line1);
			getText(row2, line2);

			unicode::iconvert::convert(line1.text, "utf-8", uline1);
			unicode::iconvert::convert(line2.text, "utf-8", uline2);

			line1.flowed=line2.flowed;

			uline1.erase(uline1.begin() + pos1, uline1.end());

			widecharbuf line1wc;

			line1wc.init_unicode(uline1.begin(), uline1.end());

			uline1.insert(uline1.end(),
				      uline2.begin() + pos2, uline2.end());

			line1.text=unicode::iconvert::convert(uline1, "utf-8");

			replaceTextLines(row1, row2+1-row1,
					 &line1, &line1+1);

			cursorRow=row1;
			cursorCol=line1wc.wcwidth(0);

			draw();

			int dummy1, dummy2;
			getCursorPosition(dummy1, dummy2);
			return;
		}
	}

	// Delete grapheme under cursor

	std::vector<unicode_char> before, after;
	bool flowed;

	getTextBeforeAfter(before, after, flowed);

	if (is_clreol_key)
		CursesField::cutBuffer.push_back(CursesFlowedLine("",
								  flowed));

	widecharbuf wc;

	wc.init_unicode(after.begin(), after.end());

	if (wc.graphemes.size() > 0) // Cursor in the middle of the line.
	{
		after=wc.get_unicode_substring(1, wc.graphemes.size()-1);
		setTextBeforeAfter(before, after, flowed);
		drawLine(cursorRow);
		return;
	}

	erase();
	// Cursor at the end of the line

	if (cursorRow+1 < numLines())
	{
		// Another row below, splice it into this row.

		size_t save_row=cursorRow;

		CursesFlowedLine l;

		getText(save_row+1, l);

		unicode::iconvert::convert(l.text, "utf-8", after);

		replaceTextLines(save_row+1, 1, &l, &l);

		cursorRow=save_row;
		setTextBeforeAfter(before, after, l.flowed);
	}
	draw();
}

// Inserted text, see if the line needs to be wrapped.

void CursesEditMessage::inserted()
{
	size_t row=cursorRow;

	editablewidechar current_buffer;
	bool wrappedLine;

	{
		std::vector<unicode_char> before, after;

		getTextBeforeAfter(before, after, wrappedLine);

		current_buffer.set_contents(before, after);
	}

	unicode_wordwrap_oiterator insert_iter;

	{
		std::vector<unicode_char> line;

		current_buffer.before_insert.tounicode(line);

		unicodewordwrap(line.begin(),
				line.end(),
				unicoderewrapnone(),
				insert_iter,
				LINEW, false);
	}

	if (insert_iter.wrapped.size() > 1)
	{
		std::list<CursesFlowedLine>::iterator
			lastLine=--insert_iter.wrapped.end();

		replaceTextLines(row, 0,
				 insert_iter.wrapped.begin(), lastLine);

		++cursorRow;

		std::vector<unicode_char> before, after;

		current_buffer.get_contents(before, after);

		before.clear();
		unicode::iconvert::convert(lastLine->text, "utf-8", before);

		setTextBeforeAfter(before, after, wrappedLine);
		cursorLineHorizShift=0;
		draw();
	}
	else
		drawLine(row);
}

void CursesEditMessage::left(bool moveOff)
{
	std::vector<unicode_char> line;

	getText(cursorRow, line);

	std::vector<unicode_char>::iterator beg(line.begin()),
		cursorPos=getIndexOfCol(line, cursorCol);

	if (cursorPos == line.begin())
	{
		if (cursorRow == 0)
		{
			if (moveOff)
				transferPrevFocus();
		}
		else
		{
			--cursorRow;
			end();
			drawLine(cursorRow+1);
			return;
		}
	}
	else
	{
		do
		{
			--cursorPos;
		} while (cursorPos > beg &&
			 !unicode_grapheme_break(cursorPos[-1], *cursorPos));

		widecharbuf wc;

		wc.init_unicode(line.begin(), cursorPos);

		cursorCol=wc.wcwidth(0);
	}

	drawLine(cursorRow);
}

void CursesEditMessage::right()
{
	std::vector<unicode_char> line;

	getText(cursorRow, line);

	std::vector<unicode_char>::iterator
		cursorPos=getIndexOfCol(line, cursorCol), end(line.end());

	if (cursorPos == line.end())
	{
		if (cursorRow + 1 < numLines())
		{
			cursorCol=0;
			cursorLineHorizShift=0;
			++cursorRow;
			drawLine(cursorRow - 1);
		}
	}
	else
	{
		do
		{
			++cursorPos;

		} while (cursorPos < end &&
			 !unicode_grapheme_break(cursorPos[-1], *cursorPos));

		widecharbuf wc;

		wc.init_unicode(line.begin(), cursorPos);

		cursorCol=wc.wcwidth(0);
	}
	drawLine(cursorRow);
}

int CursesEditMessage::getCursorPosition(int &row, int &col)
{
	std::vector<unicode_char> uc_expanded;

	getText(cursorRow, uc_expanded);

	size_t w=getWidth();

	if (w < 1)
		w=1;

	size_t virtual_horiz_pos=getIndexOfCol(uc_expanded, cursorCol) -
		uc_expanded.begin();
	size_t virtual_horiz_col_adjusted;

	widecharbuf virtual_line;

	virtual_line.init_unicode(uc_expanded.begin(),
				  uc_expanded.begin()+virtual_horiz_pos);

	virtual_horiz_col_adjusted=virtual_line.wcwidth(0);

	virtual_line.init_unicode(uc_expanded.begin(), uc_expanded.end());

	size_t virtual_width=virtual_line.wcwidth(0);

	if (cursorLineHorizShift >= virtual_horiz_pos)
	{
		size_t new_shift_pos=virtual_horiz_pos;

		while (new_shift_pos > 0)
		{
			--new_shift_pos;

			if (new_shift_pos > 0 &&
			    unicode_grapheme_break(uc_expanded[new_shift_pos-1],
						   uc_expanded[new_shift_pos]))
				break;
		}

		if (new_shift_pos != cursorLineHorizShift)
		{
			cursorLineHorizShift=new_shift_pos;
			drawLine(cursorRow);
		}
	}

	if (virtual_width < w)
	{
		if (cursorLineHorizShift > 0)
		{
			cursorLineHorizShift=0;
			drawLine(cursorRow);
		}
	}
	else
	{
		size_t cursor_grapheme_width=1;

		if (virtual_horiz_pos < uc_expanded.size())
		{
			size_t p=virtual_horiz_pos;

			do
			{
				++p;
			} while (p < uc_expanded.size() &&
				 !unicode_grapheme_break(uc_expanded[p-1],
							 uc_expanded[p]));

			widecharbuf wc;

			wc.init_unicode(uc_expanded.begin()+virtual_horiz_pos,
					uc_expanded.begin()+p);

			cursor_grapheme_width=
				wc.wcwidth(virtual_horiz_col_adjusted);
		}

		size_t col_after_cursor=virtual_horiz_col_adjusted +
			cursor_grapheme_width;

		size_t minimum_col_shift=
			col_after_cursor >= (w-1) ? col_after_cursor-(w-1):0;

		size_t minimum_horiz_shift=getIndexOfCol(uc_expanded,
							 minimum_col_shift)-
			uc_expanded.begin();

		while (minimum_horiz_shift < uc_expanded.size())
		{
			widecharbuf wc;

			wc.init_unicode(uc_expanded.begin(),
					uc_expanded.begin()
					+minimum_horiz_shift);

			minimum_col_shift=wc.wcwidth(0);

			if (minimum_col_shift+(w-1) >= col_after_cursor)
				break;

			while (++minimum_horiz_shift < uc_expanded.size())
			{
				if (unicode_grapheme_break
				    (uc_expanded[minimum_horiz_shift-1],
				     uc_expanded[minimum_horiz_shift]))
					break;
			}
		}

		if (cursorLineHorizShift < minimum_horiz_shift)
		{
			cursorLineHorizShift=minimum_horiz_shift;
			drawLine(cursorRow);
		}
	}

	row=cursorRow;

	size_t virtual_horiz_shift_pos;

	{
		widecharbuf wc;

		wc.init_unicode(uc_expanded.begin(),
				uc_expanded.begin() + cursorLineHorizShift);

		virtual_horiz_shift_pos=wc.wcwidth(0);
	}

	col=virtual_horiz_col_adjusted - virtual_horiz_shift_pos;

	Curses::getCursorPosition(row, col);

	if (marked)
	{
		size_t row1, col1, row2, col2;

		if (getMarkedRegion(row1, row2, col1, col2))
			return 0;
	}

	return 1;
}

void CursesEditMessage::draw()
{
	size_t firstRow, nrows;

	getVerticalViewport(firstRow, nrows);

	size_t i;

	for (i=0; i<nrows; i++)
		drawLine(firstRow + i);
}

void CursesEditMessage::erase()
{
	size_t firstRow, nrows;

	getVerticalViewport(firstRow, nrows);

	size_t i;

	std::vector<unicode_char> spaces;

	spaces.insert(spaces.begin(), getWidth(), ' ');

	for (i=0; i<nrows; i++)
		writeText(spaces, i+firstRow, 0, CursesAttr());
}

void CursesEditMessage::enterKey()
{
	size_t row=cursorRow;

	std::vector<unicode_char> currentLine, nextLine;
	bool flowed;

	getTextBeforeAfter(currentLine, nextLine, flowed);

	CursesFlowedLine newlines[2];

	newlines[0]=CursesFlowedLine(unicode::iconvert::convert(currentLine,
							     "utf-8"), false);
	newlines[1]=CursesFlowedLine(unicode::iconvert::convert(nextLine, "utf-8"),
				     flowed);

	replaceTextLines(cursorRow, 1, newlines, newlines+2);
	cursorRow=row+1;
	cursorCol=0;
}

// Draw the indicated line.

void CursesEditMessage::drawLine(size_t lineNum)
{
	std::vector<unicode_char> chars;
	bool wrapped;

	{
		CursesFlowedLine line;

		getText(lineNum, line);

		unicode::iconvert::convert(line.text, "utf-8", chars);
		wrapped=line.flowed;
	}

	size_t w=getWidth();

	//
	// Compute selectedFirst-selectedLast range to show in inverse video

	size_t selectedFirst=0; // First char to highlight
	size_t selectedLast=0;  // First char to highlight no more

	if (marked)
	{
		size_t row1, col1;
		size_t row2, col2;

		getMarkedRegion(row1, row2, col1, col2);

		if (row1 <= lineNum && lineNum <= row2)
		{
			if (row1 == lineNum)
				selectedFirst=col1;

			if (row2 == lineNum)
				selectedLast=col2;
			else
				selectedLast=chars.size();
		}
	}

	/*
	** If cursor leaves the edit area we don't want to leave the
	** text display shifted, UNLESS the cursor is on the status line.
	*/

	bool show_line_shifted=lineNum==cursorRow && (hasFocus() ||
						     statusBar->prompting());

	bool past_right_margin=false;

	widecharbuf shiftedsel; // Stuff shifted past the left margin

	if (show_line_shifted && cursorLineHorizShift > 0)
	{
		if (cursorLineHorizShift > chars.size())
			cursorLineHorizShift=chars.size(); // Sanity check

		// Move the shifted contents from chars into shiftedsel

		shiftedsel.init_unicode(chars.begin(),
					chars.begin() + cursorLineHorizShift);

		chars.erase(chars.begin(),
			    chars.begin() + cursorLineHorizShift);

		// Update highlighted range to account for the shift.

		if (selectedFirst > cursorLineHorizShift)
			selectedFirst -= cursorLineHorizShift;
		else
			selectedFirst=0;

		if (selectedLast > cursorLineHorizShift)
			selectedLast -= cursorLineHorizShift;
		else
			selectedLast=0;
	}

	// Sanity check on the highlighted range

	if (selectedFirst > chars.size())
		selectedFirst=chars.size();

	if (selectedLast > chars.size())
		selectedLast=chars.size();

	// Split the contents into separate before/selection/after parts.

	widecharbuf beforesel, sel, aftersel;

	beforesel.init_unicode(chars.begin(), chars.begin() + selectedFirst);
	sel.init_unicode(chars.begin() + selectedFirst,
			 chars.begin() + selectedLast);
	aftersel.init_unicode(chars.begin() + selectedLast, chars.end());

	// Perform tab expansion

	{
		size_t w=shiftedsel.expandtabs(0);

		w = beforesel.expandtabs(w);
		w = sel.expandtabs(w);
		aftersel.expandtabs(w);
	}

	// Truncate to right margin

	size_t pos=0;

	if (beforesel.wcwidth(0) + pos > w)
	{
		std::pair<std::vector<unicode_char>, size_t>
			res=beforesel.get_unicode_truncated(w-pos, pos);

		beforesel.init_unicode(res.first.begin(), res.first.end());
	}

	pos += beforesel.wcwidth(0);

	if (sel.wcwidth(pos) + pos > w)
	{
		std::pair<std::vector<unicode_char>, size_t>
			res=sel.get_unicode_truncated(w-pos, pos);

		sel.init_unicode(res.first.begin(), res.first.end());
	}

	pos += sel.wcwidth(pos);

	if (aftersel.wcwidth(pos) + pos > w)
	{
		std::pair<std::vector<unicode_char>, size_t>
			res=aftersel.get_unicode_truncated(w-pos, pos);

		aftersel.init_unicode(res.first.begin(), res.first.end());
		past_right_margin=true;
	}

	pos += aftersel.wcwidth(pos);

	// Now, re-extract all the pieces, and set their horizontal positions

	CursesAttr attr, rev, rwrapattr;

	rwrapattr.setFgColor(color_md_headerName.fcolor);

	rev.setReverse();

	std::vector<unicode_char> larr_shown, rarr_shown, rwrap_shown;
	size_t before_pos, sel_pos, after_pos;
	size_t rarr_pos=w;

	// Ignore left-right shift indications, for now.

	before_pos=0;
	sel_pos=before_pos + beforesel.wcwidth(before_pos);
	after_pos=sel_pos + sel.wcwidth(sel_pos);

	if (wrapped)
		rwrap_shown.push_back(ucwrap);

	if (show_line_shifted)
	{
		if (cursorLineHorizShift > 0)
		{
			// Shifted past the left margin. Find the first buffer
			// that contains at least one grapheme

			widecharbuf *ptr=&beforesel;
			size_t *posptr=&before_pos;

			if (ptr->ustring.empty())
			{
				ptr=&sel;
				posptr=&sel_pos;
			}

			if (ptr->ustring.empty())
			{
				ptr=&aftersel;
				posptr=&after_pos;
			}

			size_t first_grapheme_width=1;

			if (!ptr->ustring.empty())
			{
				// Remove first grapheme from the buffer,
				// and adjust its position, accordingly.

				first_grapheme_width=ptr->graphemes
					.begin()->wcwidth(*posptr);

				ptr->init_string(ptr->
						 get_substring(1,
							       ptr->graphemes
							       .size()-1));
				*posptr=first_grapheme_width;
			}

			larr_shown.push_back(ularr);
		}

		if (past_right_margin)
		{
			// Shifted past the right margin. Find the last
			// buffer with at least one grapheme.

			widecharbuf *ptr=&aftersel;
			size_t *posptr=&after_pos;

			if (ptr->ustring.empty())
			{
				ptr=&sel;
				posptr=&sel_pos;
			}

			if (ptr->ustring.empty())
			{
				ptr=&beforesel;
				posptr=&before_pos;
			}

			// Truncate the buffer

			rarr_shown.push_back(urarr);
			rarr_pos=w-1;
			ptr->init_string(ptr->get_string_truncated(rarr_pos
								   -*posptr,
								   *posptr)
					 .first);
		}
	}

	std::vector<unicode_char>
		before_shown, sel_shown, after_shown;
	beforesel.tounicode(before_shown);
	sel.tounicode(sel_shown);
	after_shown=aftersel.get_unicode_fixedwidth(rarr_pos-after_pos,
						    after_pos);

	if (!rwrap_shown.empty() && !after_shown.empty() &&
	    after_shown.back() == ' ')
		after_shown.pop_back(); // Make room for the wrap indication

	if (!larr_shown.empty())
		writeText(larr_shown, lineNum, 0, rev);

	if (!rwrap_shown.empty())
		writeText(rwrap_shown, lineNum, w-1, rwrapattr);

	if (!before_shown.empty())
		writeText(before_shown, lineNum, before_pos, attr);

	if (!sel_shown.empty())
		writeText(sel_shown, lineNum, sel_pos, rev);

	if (!after_shown.empty())
		writeText(after_shown, lineNum, after_pos, attr);

	if (!rarr_shown.empty())
		writeText(rarr_shown, lineNum, rarr_pos, rev);
}

void CursesEditMessage::load(std::istream &i, bool isflowed, bool delsp)
{
	get_file_helper beg_iter(i, isflowed, delsp), end_iter;

	replaceTextLines(0, numLines(), beg_iter, end_iter);
	cursorRow=0;
	draw();
}

void CursesEditMessage::save(std::ostream &o, bool isflowed)
{
	for (size_t i=0, n=numLines(); i<n; ++i)
	{
		o << unicode::iconvert::convert(getUTF8Text(i, isflowed), "utf-8",
					     unicode_default_chset())
		  << std::endl;
	}
}

bool CursesEditMessage::processKey(const Curses::Key &key)
{
	return false;
}

bool CursesEditMessage::listKeys( std::vector< std::pair<std::string, std::string> > &list)
{
	list.push_back(std::make_pair(Gettext::keyname(_("MARK_K:^ ")),
				 _("Mark")));
	list.push_back(std::make_pair(Gettext::keyname(_("JUSTIFY_K:^J")),
				 _("Justify")));
	list.push_back(std::make_pair(Gettext::keyname(_("CLREOL_K:^K")),
				 _("Line Clear")));
	list.push_back(std::make_pair(Gettext::keyname(_("SEARCH_K:^S")),
				 _("Search")));
	list.push_back(std::make_pair(Gettext::keyname(_("REPLACE_K:^R")),
				 _("Srch/Rplce")));
	list.push_back(std::make_pair(Gettext::keyname(_("CUT_K:^W")),
				 _("Cut")));
	list.push_back(std::make_pair(Gettext::keyname(_("YANK_K:^Y")),
				 _("Paste")));
	list.push_back(std::make_pair(Gettext::keyname(_("INSERT_K:^G")),
				 _("Insert File")));
	list.push_back(std::make_pair(Gettext::keyname(_("DICTSPELL_K:^D")),
				 _("Dict Spell")));
	list.push_back(std::make_pair(Gettext::keyname(_("CANCEL_K:^C")),
				 _("Cancel/Exit")));

	if (Macros::getRuntimeMacros() != NULL)
	{
		list.push_back(std::make_pair(Gettext::keyname(_("MACRO_K:^N")),
					 _("New/Del Macro")));
	}

	if (externalEditor.size() > 0)
	{
		list.push_back(std::make_pair(Gettext::keyname(_("EDITOR_K:^U")),
					 _("Ext Editor")));
	}
	return false;
}

size_t CursesEditMessage::getTextHorizPos(const std::vector<unicode_char> &line,
					  size_t column)
{
	widecharbuf wc;

	wc.init_unicode(line.begin(), line.begin()+column);

	return wc.wcwidth(0);
}
