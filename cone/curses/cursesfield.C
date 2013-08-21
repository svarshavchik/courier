/*
** Copyright 2002-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "curses_config.h"
#include "cursesfield.H"
#include "curseskeyhandler.H"
#include "cursesmoronize.H"
#include "widechar.H"
#include <algorithm>
#include <iostream>

std::vector<unicode_char> CursesField::yesKeys, CursesField::noKeys;
unicode_char CursesField::yankKey='\x19';
unicode_char CursesField::clrEolKey='\x0B';

std::list<CursesFlowedLine> CursesField::cutBuffer; // for cut/paste

CursesField::CursesField(CursesContainer *parent,
			 size_t widthArg,
			 size_t maxlengthArg,
			 std::string initValue)
	: Curses(parent), width(widthArg), maxlength(maxlengthArg),
	  shiftoffset(0),
	  selectpos(0), passwordChar(0), yesnoField(false),
	  isUnderlined(true)
{
	if (yesKeys.size() == 0)
		mail::iconvert::convert(std::string("yY"),
					unicode_default_chset(),
					yesKeys);

	if (noKeys.size() == 0)
		mail::iconvert::convert(std::string("nN"),
					unicode_default_chset(),
					yesKeys);

	std::vector<unicode_char> utext;

	mail::iconvert::convert(initValue, unicode_default_chset(), utext);

	text.set_contents(utext, std::vector<unicode_char>());

	selectpos=text.before_insert.graphemes.size();
}

void CursesField::setAttribute(Curses::CursesAttr attr)
{
	attribute=attr;
	draw();
}

CursesField::~CursesField()
{
	if (optionHelp.size() > 0)
		CursesKeyHandler::handlerListModified=true;
}

void CursesField::setOptionHelp(const std::vector< std::pair<std::string,
							     std::string> >
				&help)
{
	CursesKeyHandler::handlerListModified=true;
	optionHelp=help;
}

void CursesField::setRow(int row)
{
	erase();
	Curses::setRow(row);
	draw();
}

void CursesField::setCol(int col)
{
	erase();
	Curses::setCol(col);
	draw();
}

void CursesField::setText(std::string textArg)
{
	std::vector<unicode_char> utext;

	mail::iconvert::convert(textArg, unicode_default_chset(), utext);

	text.set_contents(utext, std::vector<unicode_char>());

	selectpos=text.before_insert.graphemes.size();
	shiftoffset=0;
	draw();
}

std::string CursesField::getText() const
{
	std::vector<unicode_char> b, a;

	text.get_contents(b, a);

	b.insert(b.end(), a.begin(), a.end());

	return mail::iconvert::convert(b, unicode_default_chset());
}

int CursesField::getWidth() const
{
	return width;
}

void CursesField::setWidth(int w)
{
	if (w > 0)
		width=w;
	draw();
}

int CursesField::getHeight() const
{
	return 1;
}

void CursesField::getbeforeafter(widecharbuf &wbefore, widecharbuf &wafter)
{
	std::vector<unicode_char> before, after;
	text.get_contents(before, after);

	wbefore.init_unicode(before.begin(), before.end());
	wafter.init_unicode(after.begin(), after.end());
}

void CursesField::draw()
{
	widecharbuf wbefore, wafter;

	getbeforeafter(wbefore, wafter);

	wafter.expandtabs(wbefore.expandtabs(0));

	size_t a=wbefore.graphemes.size();
	size_t b=selectpos;

	if (a > b)
	{
		size_t c=a; a=b; b=c;
	}

	if (a < shiftoffset)
		a=shiftoffset;

	if (b < shiftoffset)
		b=shiftoffset;

	a -= shiftoffset;
	b -= shiftoffset;

	wbefore += wafter;

	std::vector<unicode_char> ubeforesel, usel, uaftersel;
	size_t usel_pos=0, uaftersel_pos=0;

	size_t p=shiftoffset;
	size_t col=0;

	size_t virtual_col=0;

	for (size_t i=0; i<p && i < wbefore.graphemes.size(); ++i)
		virtual_col += wbefore.graphemes[i].wcwidth(virtual_col);

	std::vector<unicode_char> unicodes;

	while (p < wbefore.graphemes.size() && col < width)
	{
		const widecharbuf::grapheme_t &grapheme=wbefore.graphemes[p];

		size_t grapheme_width=grapheme.wcwidth(virtual_col);

		if (grapheme_width + col > width)
			break;

		std::vector<unicode_char> *vp;

		if (a)
		{
			vp= &ubeforesel;
			--a;
			--b;
			usel_pos += grapheme_width;
			uaftersel_pos += grapheme_width;
		}
		else if (b)
		{
			vp= &usel;
			--b;
			uaftersel_pos += grapheme_width;
		}
		else
			vp= &uaftersel;

		unicodes.clear();

		if (passwordChar)
			vp->insert(vp->end(), grapheme_width, passwordChar);
		else
			vp->insert(vp->end(), grapheme.uptr,
				   grapheme.uptr+grapheme.cnt);

		col += grapheme_width;
		virtual_col += grapheme_width;
		++p;
	}

	uaftersel.insert(uaftersel.end(), width-col, ' ');

	bool underlined=isUnderlined;

	{
		Curses::CursesAttr aa=attribute;

		if (ubeforesel.size())
			writeText(ubeforesel, 0, 0,
				  aa.setUnderline(underlined));
	}

	{
		Curses::CursesAttr aa=attribute;

		if (usel.size())
			writeText(usel, 0, usel_pos,
				  aa.setReverse().setUnderline(underlined));
	}

	{
		Curses::CursesAttr aa=attribute;

		if (uaftersel.size())
			writeText(uaftersel, 0, uaftersel_pos,
				  aa.setUnderline(underlined));
	}
}

void CursesField::erase()
{
	std::vector<unicode_char> v;

	v.insert(v.end(), width, ' ');

	writeText(v, 0, 0, CursesAttr());
}

bool CursesField::isFocusable()
{
	return true;
}

void CursesField::focusGained()
{
	text.to_before();

	selectpos=0;
	shiftoffset=0;
	draw();
}

void CursesField::focusLost()
{
	text.to_after();
	selectpos=shiftoffset=0;
	draw();
}

int CursesField::getCursorPosition(int &row, int &col)
{
	// Automatically scroll field to keep the cursor visible

	row=0;

	size_t save_shiftoffset=shiftoffset;

	widecharbuf wbefore, wafter;

	col=text.adjust_shift_pos(shiftoffset, width, wbefore, wafter);

	if (save_shiftoffset != shiftoffset)
		draw();

	Curses::getCursorPosition(row, col);

	return selectpos == wbefore.graphemes.size();
}

// Set selection position to current position

void CursesField::setselectpos()
{
	text.insert_to_before();

	widecharbuf wbefore, wafter;

	getbeforeafter(wbefore, wafter);

	selectpos=wbefore.graphemes.size();
}

bool CursesField::processKeyInFocus(const Key &key)
{
	if (key == key.HOME)
	{
		text.to_after();
		shiftoffset=0;
		selectpos=0;
		draw();
		return true;
	}

	if (key == key.SHIFTHOME)
	{
		text.to_after();
		shiftoffset=0;
		draw();
		return true;
	}

	if (key == key.END)
	{
		text.to_before();
		setselectpos();
		draw();
		return true;
	}

	if (key == key.SHIFTEND)
	{
		setselectpos();
		text.to_before();
		draw();
		return true;
	}

	if (key == key.LEFT)
	{
		left();
		draw();
		return true;
	}

	if (key == key.SHIFTLEFT)
	{
		size_t savepos=selectpos;
		left();
		selectpos=savepos;
		draw();
		return true;
	}

	if (key == key.RIGHT)
	{
		right();
		draw();
		return true;
	}

	if (key == key.SHIFTRIGHT)
	{
		size_t savepos=selectpos;
		right();

		selectpos=savepos;
		draw();
		return true;
	}

	if ((key.plain() && key.ukey == '\t') ||
	    key == key.DOWN || key == key.UP ||
	    key == key.SHIFTDOWN || key == key.SHIFTUP || key == key.ENTER ||
	    key == key.PGDN || key == key.SHIFTPGDN ||
	    key == key.PGUP || key == key.SHIFTPGUP)
	{
		setselectpos();
		draw();
		return Curses::processKeyInFocus(key);
	}

	std::vector<unicode_char> cut_text;

	if (text.inserted.empty() && key != key.SHIFT)
		text.contents_cut(selectpos, cut_text);

	if (!cut_text.empty())
	{
		if (!(key.plain() && key.ukey == yankKey))
		{
			cutBuffer.clear();
			cutBuffer.push_back(CursesFlowedLine(mail::iconvert
							     ::convert(cut_text,
								       "utf-8"),
							     false));
		}

		selectpos=text.before_insert.graphemes.size();
		draw();

		if (key == key.BACKSPACE || key == key.DEL)
			return true;
	}

	if (key == key.BACKSPACE)
	{
		text.insert_to_before();

		if (text.before_insert.graphemes.size() > 0)
		{
			text.contents_cut(text.before_insert
					  .graphemes.size()-1, cut_text);
			setselectpos();
		}
		draw();
		return true;
	}

	if (key == key.DEL)
	{
		text.insert_to_before();

		if (text.after_insert.graphemes.size() > 0)
		{
			text.contents_cut(text.before_insert
					  .graphemes.size()+1, cut_text);
			setselectpos();
		}
		draw();
		return true;
	}

	if (!key.plain())
	{
		return Curses::processKeyInFocus(key);
	}

	unicode_char k=key.ukey;

	if (k == yankKey && !yesnoField &&
	    optionField.size() == 0 && !cutBuffer.empty())
		; // Will paste later
	else if (k < ' ')
	{
		return Curses::processKeyInFocus(key);
	}

	bool doYesNoField=true;

	if (optionField.size() > 0)
	{
		text.inserted.push_back(k);

		std::vector<unicode_char> before, after;

		text.get_contents(before, after);

		before.insert(before.end(), after.begin(), after.end());

		if (before.empty())
			return true;

		text.inserted.pop_back();

		std::vector<unicode_char>::iterator p=
			find(optionField.begin(),
			     optionField.end(), *before.begin());


		if (p == optionField.end())
		{
			if (!yesnoField)
				return true;
		}
		else doYesNoField=false;

	}

	if (doYesNoField && yesnoField)
	{
		text.inserted.push_back(k);

		std::vector<unicode_char> before, after;

		text.get_contents(before, after);

		before.insert(before.end(), after.begin(), after.end());

		if (before.empty())
			return true;

		std::vector<unicode_char>::iterator p=
			find(yesKeys.begin(),
			     yesKeys.end(), *before.begin());

		if (p != yesKeys.end())
		{
			before.clear();
			after.clear();
			before.push_back('Y');
			text.set_contents(before, after);
			this->processKeyInFocus(Key(Key::ENTER));
			// Automatic enter
			return true;
		}

		p=find(noKeys.begin(), noKeys.end(), *before.begin());

		if (p != noKeys.end())
		{
			before.clear();
			after.clear();
			before.push_back('N');
			text.set_contents(before, after);
			this->processKeyInFocus(Key(Key::ENTER));
			// Automatic enter
			return true;
		}
		
		text.inserted.pop_back();
	}

	if (k == yankKey)
	{
		std::vector<unicode_char> ucut;

		if (!cutBuffer.empty())
			mail::iconvert::convert(cutBuffer.front().text,
						"utf-8",
						ucut);

		text.insert_to_before();

		std::vector<unicode_char> before, after;
		text.get_contents(before, after);

		before.insert(before.end(), ucut.begin(), ucut.end());

		widecharbuf wbefore, wafter;

		wbefore.init_unicode(before.begin(), before.end());
		wafter.init_unicode(after.begin(), after.end());

		size_t wbefore_w=wbefore.wcwidth(0);
		size_t wafter_w=wafter.wcwidth(wbefore_w);

		if (wbefore_w + wafter_w > maxlength)
			beepError();
		else
			text.set_contents(before, after);
	}
	else
	{
		text.inserted.push_back(k);

		widecharbuf wbefore, wafter;

		getbeforeafter(wbefore, wafter);

		size_t wbefore_w=wbefore.wcwidth(0);
		size_t wafter_w=wafter.wcwidth(wbefore_w);

		if (wbefore_w + wafter_w > maxlength)
		{
			beepError();
			text.inserted.pop_back();
		}
	}

	{
		widecharbuf wbefore, wafter;

		getbeforeafter(wbefore, wafter);

		selectpos=wbefore.graphemes.size();
	}

	draw();

	if (yesnoField || optionField.size() > 0)
		this->processKeyInFocus(Key(Key::ENTER)); // Automatic enter
	else if (CursesMoronize::enabled && !passwordChar &&
		 !text.inserted.empty())
	{
		size_t i;
		unicode_char m_buf[CursesMoronize::max_keycode_len+1];
		std::vector<unicode_char>::iterator b(text.inserted.begin()),
			e(text.inserted.end()), p=e;

		for (i=0; i<sizeof(m_buf)/sizeof(m_buf[0])-1; ++i)
		{
			if (b == p)
				break;

			--p;

			m_buf[i]= *p;
		}

		m_buf[i]=0;

		if (m_buf[0] && CursesMoronize::enabled)
		{
			std::vector<unicode_char> repl_c;

			i=CursesMoronize::moronize(m_buf, repl_c);

			if (i > 0)
			{
				while (i > 0)
				{
					processKeyInFocus(Curses::Key
							  (Curses::Key::BACKSPACE));
					--i;
				}

				std::string s(mail::iconvert::convert
					      (repl_c, unicode_default_chset()));

				std::vector<wchar_t> wc;

				towidechar(s.begin(), s.end(), wc);

				for (std::vector<wchar_t>::iterator
					     b(wc.begin()), e(wc.end()); b != e;
				     ++b)
				{
					processKeyInFocus(Curses::Key(*b));
				}
			}
		}
	}

	return true;
}

void CursesField::left()
{
	text.insert_to_before();

	if (text.before_insert.graphemes.size() > 0)
	{
		std::vector<unicode_char> cut_text;

		text.contents_cut(text.before_insert.graphemes.size()-1,
				  cut_text);

		std::vector<unicode_char> before, after;
		text.get_contents(before, after);

		after.insert(after.begin(), cut_text.begin(), cut_text.end());

		text.set_contents(before, after);
	}
	setselectpos();
	draw();
}

void CursesField::right()
{
	text.insert_to_before();

	if (text.after_insert.graphemes.size() > 0)
	{
		std::vector<unicode_char> cut_text;

		text.contents_cut(text.before_insert.graphemes.size()+1,
				  cut_text);

		std::vector<unicode_char> before, after;
		text.get_contents(before, after);

		before.insert(before.end(), cut_text.begin(), cut_text.end());

		text.set_contents(before, after);
	}
	setselectpos();
	draw();
}

void CursesField::setCursorPos(size_t o)
{
	text.to_position(o);
	setselectpos();
	draw();
}
