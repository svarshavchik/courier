/*
** Copyright 2002-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "curses_config.h"
#include "cursesstatusbar.H"
#include "cursesscreen.H"
#include "cursescontainer.H"

#include <algorithm>

std::string CursesStatusBar::extendedErrorPrompt;
std::string CursesStatusBar::shortcut_next_key;
std::string CursesStatusBar::shortcut_next_descr;
unicode_char CursesStatusBar::shortcut_next_keycode= 'O' & 31;

CursesStatusBar::CursesStatusBar(CursesScreen *parent) :
	CursesContainer(parent), CursesKeyHandler(PRI_STATUSHANDLER),
	busyCounter(0), parentScreen(parent), fieldActive(NULL),
	currentShortcutPage(0)
{
	clearStatusTimer=this;
	clearStatusTimer= &CursesStatusBar::clearstatus;
	resetRow();
}

// Reposition the status bar after a wrapped text message appeared or
// disappered.

void CursesStatusBar::resetRow()
{
	int h=parentScreen->getHeight();
	int hh=getHeight();

	if (h > hh)
	{
		h -= hh;
		setRow(h);
	}
	else
		setRow(2);
}

CursesStatusBar::~CursesStatusBar()
{
	if (fieldActive)
	{
		delete fieldActive;
		CursesKeyHandler::handlerListModified=true;
		// Rebuild shortcuts
	}
}

int CursesStatusBar::getWidth() const
{
	return getParent()->getWidth();
}

// The height is usually 3 rows, but may be larger when a wrapped message
// is shown

int CursesStatusBar::getHeight() const
{
	int r=3;

	if (extendedErrorMsg.size() > 0)
		r += extendedErrorMsg.size() + 1;

	return r;
}

void CursesStatusBar::resized()
{
	resetRow();
	CursesKeyHandler::handlerListModified=true; // Rebuild shortcuts
	draw();
}

bool CursesStatusBar::progressWanted()
{
	if (extendedErrorMsg.size() > 0)
		return false;

	time_t now=time(NULL);

	if (progressText.size() > 0 && progressTime == now)
		return false; // Too soon

	progressTime=now;
	return true;
}

void CursesStatusBar::progress(std::string progress)
{
	progressText.clear();
	mail::iconvert::convert(progress,
				unicode_default_chset(),
				progressText);

	draw();
	flush();
}

void CursesStatusBar::setStatusBarAttr(CursesAttr a)
{
	attrStatusBar=a;
	draw();
}

void CursesStatusBar::setHotKeyAttr(CursesAttr a)
{
	attrHotKey=a;
	draw();
}

void CursesStatusBar::setHotKeyDescr(CursesAttr a)
{
	attrHotKeyDescr=a;
	draw();
}


void CursesStatusBar::draw()
{
	std::string spaces="";

	// To save some time, call rebuildShortcuts() only when necessary

	if (CursesKeyHandler::handlerListModified)
		rebuildShortcuts();

	static const char throbber[]={'|', '/', '-', '\\'};

	size_t w=getWidth();

	std::vector<unicode_char> statusLine=statusText;

	statusLine.insert(statusLine.begin(), ' ');

	if (busyCounter)
	{
		if (busyCounter > (int)sizeof(throbber))
			busyCounter=1;
		statusLine.insert(statusLine.begin(),
				  (unicode_char)throbber[busyCounter-1]);
	}
	else
		statusLine.insert(statusLine.begin(),
				  ' ');

	if (extendedErrorMsg.size() == 0)
		statusLine.insert(statusLine.end(),
				  progressText.begin(),
				  progressText.end());


	CursesAttr attr=attrStatusBar;

	attr.setReverse();

	{
		widecharbuf wc;

		wc.init_unicode(statusLine.begin(), statusLine.end());

		wc.expandtabs(0);

		std::pair<std::vector<unicode_char>, size_t>
			trunc(wc.get_unicode_truncated(w, 0));

		if (trunc.second < w)
			trunc.first.insert(trunc.first.end(), w-trunc.second,
					   ' ');

		writeText(trunc.first, getHeight()-3, 0, attr);

	}

	if (extendedErrorMsg.size() > 0)
	{
		statusLine.clear();
		statusLine.insert(statusLine.begin(), w, ' ');
		writeText(statusLine, 0, 0, attr);
		size_t n;

		for (n=0; n<extendedErrorMsg.size(); n++)
		{
			writeText(extendedErrorMsg[n],
				  n+1, 0, attrStatusBar);
		}
	}

	// Clear the shortcut area.

	statusLine.clear();
	statusLine.insert(statusLine.begin(), w, ' ');

	int r=1;

	if (extendedErrorMsg.size() > 0)
		r += extendedErrorMsg.size()+1;

	writeText(statusLine, r, 0, CursesAttr());
	writeText(statusLine, r+1, 0, CursesAttr());

	// Draw the current shortcut page.

	if (extendedErrorMsg.size() == 0 &&
	    currentShortcutPage < shortcuts.size())
	{

		std::vector<std::vector<
			std::pair< std::vector<unicode_char>,
				   std::vector<unicode_char> > > >
			&shortcutPage= shortcuts[currentShortcutPage];

		size_t i;

		for (i=0; i < shortcutPage.size(); i++)
		{
			std::vector< std::pair< std::vector<unicode_char>,
						std::vector<unicode_char> > >
				&column=shortcutPage[i];

			size_t j;

			for (j=0; j<column.size(); j++)
			{
				std::pair< std::vector<unicode_char>,
					   std::vector<unicode_char> >
					&row=column[j];

				std::vector<unicode_char> cpy=row.first;

				widecharbuf wc;

				wc.init_unicode(cpy.begin(), cpy.end());
				wc.expandtabs(0);

				if (shortcuts.size() > 1 ||
				    row.first.size() > 1 ||
				    row.second.size() > 1)
				{
					size_t w=wc.wcwidth(0);

					if (w < max_sc_nlen)
						cpy.insert(cpy.begin(),
							   max_sc_nlen - w,
							   ' ');
				}

				CursesAttr h=attrHotKey;

				writeText( cpy, j+1,
					   i * (max_sc_nlen + max_sc_dlen + 2),
					   h.setReverse());

				CursesAttr a;

				if (row.second.size() > 2 &&
				    row.second[0] == '/' &&
				    row.second[1] >= '0' &&
				    row.second[1] <= '9')
				{
					a.setBgColor(row.second[1] - '0');
					row.second.erase(row.second.begin(),
							 row.second.begin()+2);
				}
				else
				{
					a=attrHotKeyDescr;
				}

				writeText( row.second, j+1,
					   i * (max_sc_nlen + max_sc_dlen + 2)
					   + max_sc_nlen+1, a);
			}
		}
	}
	CursesContainer::draw();
}

class CursesStatusBarShortcutSort {
public:
	bool operator()(std::pair<std::string, std::string> a,
			std::pair<std::string, std::string> b)
	{
		return (strcoll( a.first.c_str(), b.first.c_str()) < 0);
	}
};

//
// Create a sorted key shortcut list from all installed keyhandlers.
//

static void createShortcuts(size_t &max_sc_nlen, size_t &max_sc_dlen,
			    std::vector< std::pair< std::vector<unicode_char>,
			    std::vector<unicode_char> > > &keys_w)
{
	std::vector< std::pair<std::string, std::string> > keys;

	std::list<CursesKeyHandler *>::const_iterator
		kb=CursesKeyHandler::begin(),
		ke=CursesKeyHandler::end();

	while (kb != ke)
		if ((*kb++)->listKeys(keys))
			break;

	sort(keys.begin(), keys.end(),
	     CursesStatusBarShortcutSort());

	std::vector< std::pair<std::string, std::string> >::iterator
		b=keys.begin(), e=keys.end();

	while (b != e)
	{
		std::vector<unicode_char> first_w;
		std::vector<unicode_char> second_w;

		mail::iconvert::convert(b->first, unicode_default_chset(),
					first_w);

		mail::iconvert::convert(b->second, unicode_default_chset(),
					second_w);

		b++;

		{
			widecharbuf wc;

			wc.init_unicode(first_w.begin(), first_w.end());
			wc.expandtabs(0);

			size_t w=wc.wcwidth(0);

			if (w > max_sc_nlen)
				max_sc_nlen=w;
		}

		{
			std::vector<unicode_char>::iterator b=second_w.begin();

			if (second_w.size() && second_w[0] == '/' &&
			    second_w[1] >= '0' &&
			    second_w[1] <= '9')
				b += 2;

			widecharbuf wc;

			wc.init_unicode(b, second_w.end());

			wc.expandtabs(0);

			size_t w=wc.wcwidth(0);

			if (w > max_sc_dlen)
				max_sc_dlen=w;
		}

		keys_w.push_back(std::make_pair(first_w, second_w));
	}
}

void CursesStatusBar::rebuildShortcuts()
{
	CursesKeyHandler::handlerListModified=false;

	currentShortcutPage=0;

	std::vector<unicode_char> nextpage_n;
	std::vector<unicode_char> nextpage_d;

	if (shortcut_next_key.size() == 0)
		shortcut_next_key="^O";

	if (shortcut_next_descr.size() == 0)
		shortcut_next_descr="...mOre";

	mail::iconvert::convert(shortcut_next_key, unicode_default_chset(),
				nextpage_n);

	mail::iconvert::convert(shortcut_next_descr, unicode_default_chset(),
				nextpage_d);

	{
		widecharbuf wc;

		wc.init_unicode(nextpage_n.begin(), nextpage_n.end());
		wc.expandtabs(0);
		max_sc_nlen=wc.wcwidth(0);
	}

	{
		widecharbuf wc;

		wc.init_unicode(nextpage_d.begin(), nextpage_d.end());
		wc.expandtabs(0);
		max_sc_dlen=wc.wcwidth(0);
	}

	std::vector< std::pair< std::vector<unicode_char>,
				std::vector<unicode_char> > > keys_w;

	createShortcuts(max_sc_nlen, max_sc_dlen, keys_w);

	// Does everything fit on one page?

	size_t w=getWidth();

	size_t ncols=w / (max_sc_nlen + 2 + max_sc_dlen);

	if (ncols <= 0)
		ncols=1;

	shortcuts.clear();
	bool multiplePages=false;

	size_t n=keys_w.size();
	size_t i=0;

	while (i < n)
	{
		if (n - i > ncols * 2)
			multiplePages=true;

		std::vector< std::vector< std::pair< std::vector<unicode_char>,
						     std::vector<unicode_char>
						     > > >
			columns;

		size_t j;

		for (j=0; j < ncols; j++ )
		{
			std::vector< std::pair< std::vector<unicode_char>,
						std::vector<unicode_char> > >
				column;

			if (i < n)
			{
				column.push_back( keys_w[i++] );
			}
			else
			{
				std::vector<unicode_char> zero;

				column.push_back(std::make_pair(zero, zero));
			}

			// Add ^O to last cell on each page.

			if (j + 1 == ncols && multiplePages)
			{
				std::pair< std::vector<unicode_char>,
					   std::vector<unicode_char> >
					p=std::make_pair(nextpage_n,
							 nextpage_d);
				column.push_back(p);
			}
			else if (i < n)
			{
				column.push_back( keys_w[i++] );
			}
			else
			{
				std::vector<unicode_char> zero;
				column.push_back (make_pair(zero, zero));
			}

			columns.push_back(column);
		}

		shortcuts.push_back(columns);
	}
}

void CursesStatusBar::status(std::string text, statusLevel level)
{
	size_t origSize;

	if (((statusText.size() > 0 || extendedErrorMsg.size() > 0) &&
	     currentLevel > level) || fieldActive != NULL)
	{
		// Message too low of a priority

		if (CursesKeyHandler::handlerListModified || busyCounter)
		{
			busyCounter=0;
			draw(); // Update the list of shortcuts
		}
		return;
	}

	origSize=extendedErrorMsg.size();

	busyCounter=0;

	statusText.clear();
	progressText.clear();
	extendedErrorMsg.clear();

	currentLevel=level;

	if (origSize > 0)
		parentScreen->resized();

	size_t w=getWidth();

	if (w > 10)
		w -= 4;

	{
		std::vector<unicode_char> uc;

		mail::iconvert::convert(text, unicode_default_chset(), uc);

		extendedErrorMsg.clear();

		std::back_insert_iterator
			< std::vector< std::vector<unicode_char> > >
			insert_iter(extendedErrorMsg);

		unicodewordwrap(uc.begin(), uc.end(),
				unicoderewrapnone(),
				insert_iter, w, true);
	}

	switch (extendedErrorMsg.size()) {
	case 0:
		extendedErrorMsg.push_back(std::vector<unicode_char>());
		// FALLTHROUGH
	case 1:
		statusText=extendedErrorMsg.front();
		extendedErrorMsg.clear();
		break;
	default:
		extendedErrorMsg.push_back(std::vector<unicode_char>());

		{
			std::vector<unicode_char> statusLine;

			mail::iconvert::convert(extendedErrorPrompt,
						unicode_default_chset(),
						statusLine);

			extendedErrorMsg.push_back(statusLine);
		}

		clearStatusTimer.cancelTimer();
		statusText.clear();
		parentScreen->resized();
		return;
	}

	clearStatusTimer.cancelTimer();
	draw();
}

void CursesStatusBar::clearstatus()
{
	if (fieldActive != NULL)
		return;

	statusText.clear();
	progressText.clear();
	busyCounter=0;
	if (extendedErrorMsg.size() > 0)
	{
		extendedErrorMsg.clear();
		parentScreen->resized();
		return;
	}
	draw();
}

void CursesStatusBar::busy()
{
	++busyCounter;
	draw();
}

void CursesStatusBar::notbusy()
{
	busyCounter=0;
	draw();
}

bool CursesStatusBar::processKey(const Curses::Key &key)
{
	if (key.plain() && key.ukey == shortcut_next_keycode)
	{
		if (++currentShortcutPage >= shortcuts.size())
			currentShortcutPage=0;
		draw();
		return true;

	}

	if (extendedErrorMsg.size() == 0)
	{
		// Status line messages are erased ten seconds after
		// the first key is received.

		if (statusText.size() > 0 &&
		    clearStatusTimer.expired())
			clearStatusTimer.setTimer(10);

		return false;
	}

	clearstatus(); // Clear wrapped popup
	return true;
}

bool CursesStatusBar::listKeys( std::vector< std::pair<std::string, std::string> > &list)
{
	if (!fieldActive)
		return false;

	list.insert(list.end(), fieldActive->getOptionHelp().begin(),
		    fieldActive->getOptionHelp().end());
	return true;
}

CursesField *CursesStatusBar::createPrompt(std::string prompt, std::string initvalue)
{
	if (fieldActive) // Old prompt?
	{
		Curses *f=fieldActive;

		fieldActive=NULL;
		delete f;
	}

	// The following requestFocus() may end up invoking focusLost()
	// method of another Curses object.  That, in turn, _may_ run some
	// code that creates another dialog prompt.  We'd rather that this
	// happen at THIS point.  Otherwise, the second dialog prompt will
	// now blow away this dialog.

	dropFocus();

	clearstatus();

	size_t maxW=getWidth();

	if (maxW > 20)
		maxW -= 20;
	else
		maxW=0;

	size_t textW;

	{
		std::vector<unicode_char> uc;

		mail::iconvert::convert(prompt, unicode_default_chset(), uc);

		widecharbuf wc;

		wc.init_unicode(uc.begin(), uc.end());

		std::pair<std::vector<unicode_char>, size_t>
			fragment=wc.get_unicode_truncated(maxW, 0);

		statusText=fragment.first;
		textW=fragment.second;
	}

	size_t fieldSize=getWidth()-textW;

	if (fieldSize > 2)
		fieldSize -= 2;
	else
		fieldSize=1;

	fieldActive=new Field(this, fieldSize, 255, initvalue);
	if (!fieldActive)
		return NULL;

	fieldActive->setAttribute(attrStatusBar);

	CursesKeyHandler::handlerListModified=true;
	// Rebuild shortcuts

	fieldActive->setCol(textW+2);
	draw();
	if (fieldActive)
		fieldActive->requestFocus();
	return fieldActive;
}

// Callback from Field, when ENTER is pressed

void CursesStatusBar::fieldEnter()
{
	fieldValue=fieldActive->getText();
	delete fieldActive;
	fieldActive=NULL;
	fieldAborted=false;
	CursesKeyHandler::handlerListModified=true;
	keepgoing=false;
	clearstatus();
}

void CursesStatusBar::fieldAbort()
{
	fieldValue="";
	delete fieldActive;
	fieldActive=NULL;
	fieldAborted=true;
	CursesKeyHandler::handlerListModified=true;
	keepgoing=false;
	clearstatus();
}

//////////////////////////////////////////////////////////////////////
//
// The custom field

CursesStatusBar::Field::Field(CursesStatusBar *parent, size_t widthArg,
			      size_t maxlengthArg,
			      std::string initValue)
	: CursesField(parent, widthArg, maxlengthArg, initValue),
	  me(parent)
{
}

CursesStatusBar::Field::~Field()
{
}

bool CursesStatusBar::Field::processKeyInFocus(const Key &key)
{
	if ((key.plain() && key.ukey == '\t') ||
	    key == key.DOWN || key == key.UP ||
	    key == key.SHIFTDOWN || key == key.SHIFTUP ||
	    key == key.PGDN || key == key.PGUP ||
	    key == key.SHIFTPGDN || key == key.SHIFTPGUP)

		return true;	// No changing focus, please.

	if (key.plain() && key.ukey == '\x03')
	{
		setText("");
		me->fieldAbort();
		return true;
	}

	if (key == key.ENTER)
	{
		me->fieldEnter();
		return true;
	}

	return CursesField::processKeyInFocus(key);
}

bool CursesStatusBar::Field::writeText(const char *text, int row, int col,
				       const CursesAttr &attr) const
{
	CursesAttr attr_cpy=attr;
	attr_cpy.setReverse(!attr_cpy.getReverse());
	attr_cpy.setUnderline(false);

	return CursesField::writeText(text, row, col, attr_cpy);
}

bool CursesStatusBar::Field::writeText(const std::vector<unicode_char> &text,
				       int row, int col,
				       const Curses::CursesAttr &attr) const
{
	CursesAttr attr_cpy=attr;
	attr_cpy.setReverse(!attr_cpy.getReverse());
	attr_cpy.setUnderline(false);

	return CursesField::writeText(text, row, col, attr_cpy);
}
