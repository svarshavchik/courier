/*
** Copyright 2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "curses_config.h"
#include "widechar.H"
#include "cursesflowedline.H"
#include <algorithm>

bool unicoderewrapnone::operator()(size_t n) const
{
	return false;
}

void widecharbuf::tounicode(std::vector<unicode_char> &text) const
{
	text=ustring;
}

void widecharbuf::init_string(const std::string &str)
{
	std::vector<unicode_char> uc;

	mail::iconvert::convert(str, unicode_default_chset(), uc);

	init_unicode(uc.begin(), uc.end());
}

std::pair<std::string, size_t>
widecharbuf::get_string_truncated(size_t maxwidth, ssize_t col) const
{
	std::pair<std::vector<unicode_char>, size_t>
		tempret(get_unicode_truncated(maxwidth, col));

	std::pair<std::string, size_t> ret;

	ret.second=tempret.second;

	mail::iconvert::fromu::convert(tempret.first, unicode_default_chset(),
				       ret.first);
	return ret;
}

std::string widecharbuf::get_substring(size_t first_grapheme,
				       size_t grapheme_cnt) const
{
	return mail::iconvert::fromu
		::convert(get_unicode_substring(first_grapheme,
						grapheme_cnt),
			  unicode_default_chset());
}

std::vector<unicode_char>
widecharbuf::get_unicode_substring(size_t first_grapheme,
				   size_t grapheme_cnt) const
{
	if (first_grapheme >= graphemes.size())
		return std::vector<unicode_char>();

	size_t max_graphemes=graphemes.size() - first_grapheme;

	if (grapheme_cnt > max_graphemes)
		grapheme_cnt=max_graphemes;

	return std::vector<unicode_char>(ustring.begin()
					 +first_grapheme,
					 ustring.begin()
					 +first_grapheme+grapheme_cnt);
}

std::pair<std::vector<unicode_char>, size_t>
widecharbuf::get_unicode_truncated(size_t maxwidth, ssize_t col) const
{
	size_t i;
	size_t cnt=0;
	size_t width=0;

	maxwidth += col;

	for (i=0; i<graphemes.size(); ++i)
	{
		size_t w=graphemes[i].wcwidth(col);

		if (col + w > maxwidth)
			break;

		col += w;
		width += w;
		cnt += graphemes[i].cnt;
	}

	std::string s;

	mail::iconvert::fromu::convert(ustring.begin(),
				       ustring.begin()+cnt,
				       unicode_default_chset(),
				       s);
	return std::make_pair(std::vector<unicode_char>
			      (ustring.begin(), ustring.begin()+cnt), width);
}

std::vector<unicode_char> widecharbuf::get_unicode_fixedwidth(size_t width,
							      ssize_t atcol)
	const
{
	std::pair<std::vector<unicode_char>, size_t>
		truncated=get_unicode_truncated(width, atcol);

	if (truncated.second < width)
		truncated.first.insert(truncated.first.end(),
				       width-truncated.second, ' ');

	return truncated.first;
}

size_t widecharbuf::wcwidth(ssize_t start_col) const
{
	size_t n=0;

	for (std::vector<grapheme_t>::const_iterator
		     b(graphemes.begin()), e(graphemes.end()); b != e; ++b)
	{
		size_t grapheme_width=b->wcwidth(start_col);

		n += grapheme_width;
		start_col += grapheme_width;
	}
	return n;
}

void widecharbuf::resetgraphemes()
{
	graphemes.clear();

	if (ustring.empty())
		return;

	const unicode_char *b=&*ustring.begin(), *e(b+ustring.size());

	do
	{
		const unicode_char *start_pos=b;

		unicode_char first, second=*b;

		do
		{
			first=second;
			++b;

		} while (b != e && (second=*b) != '\t' && first != '\t' &&
			 !unicode_grapheme_break(first, second));

		graphemes.push_back(grapheme_t(start_pos, b-start_pos));
	} while (b != e);
}

widecharbuf &widecharbuf::operator+=(const widecharbuf &o)
{
	ustring.insert(ustring.end(), o.ustring.begin(), o.ustring.end());
	resetgraphemes();
	resetgraphemes();
	return *this;
}

widecharbuf &widecharbuf::operator=(const widecharbuf &o)
{
	ustring=o.ustring;
	resetgraphemes();
	return *this;
}

size_t widecharbuf::charwidth(wchar_t ch, ssize_t atcol)
{
	// We visually multiply dashes, corresponds to code
	// in CursesScreen::writeText()

	if (ch == 0x2013)
		return 2; // EN DASH

	if (ch == 0x2014)
		return 3; // EM DASH

	if (ch != '\t')
	{
		int s=::wcwidth(ch);

		if (s == -1)
			s=1; // Unprintable character, punt
		return s;
	}

	size_t cnt=0;

	do
	{
		++atcol;
		++cnt;
	} while (atcol & 7);
	return cnt;
}


size_t widecharbuf::grapheme_t::wcwidth(ssize_t col) const
{
	std::string s;

	mail::iconvert::fromu::convert(uptr, uptr+cnt,
				       unicode_default_chset(), s);

	return towidechar(s.begin(), s.end(), towidechar_wcwidth_iter(col));
}

ssize_t widecharbuf::expandtabs(ssize_t col)
{
	std::vector<unicode_char> replbuf;

	replbuf.reserve(ustring.size()*2);

	for (std::vector<grapheme_t>::iterator
		     b(graphemes.begin()),
		     e(graphemes.end()); b != e; ++b)
	{
		size_t w=b->wcwidth(col);

		if (*b->uptr == '\t')
		{
			replbuf.insert(replbuf.end(), w, ' ');
		}
		else
		{
			replbuf.insert(replbuf.end(), b->uptr,
				       b->uptr + b->cnt);
		}

		col += w;
	}
	ustring=replbuf;
	resetgraphemes();
	return col;
}

void editablewidechar::set_contents(const std::vector<unicode_char>
				    &before,
				    const std::vector<unicode_char>
				    &after)
{
	before_insert.init_unicode(before.begin(), before.end());
	after_insert.init_unicode(after.begin(), after.end());
	inserted.clear();
}

void editablewidechar::get_contents(std::vector<unicode_char> &before,
				    std::vector<unicode_char> &after)
	const
{
	before.clear();

	before_insert.tounicode(before);
	before.insert(before.end(), inserted.begin(), inserted.end());
	after_insert.tounicode(after);
}

void editablewidechar::contents_cut(size_t cut_pos,
				    std::vector<unicode_char> &cut_text)
{
	if (cut_pos == before_insert.graphemes.size() &&
	    inserted.empty())
		return;

	std::vector<unicode_char> before, after;
						
	if (cut_pos < before_insert.graphemes.size())
	{
		before=before_insert.get_unicode_substring(0, cut_pos);
		cut_text=before_insert
			.get_unicode_substring(cut_pos,
					       before_insert.graphemes.size()
					       -cut_pos);
	}
	else
		before_insert.tounicode(before);

	if (cut_pos > before_insert.graphemes.size())
	{
		cut_pos -= before_insert.graphemes.size();

		cut_text=after_insert.get_unicode_substring(0, cut_pos);
		after=after_insert
			.get_unicode_substring(cut_pos,
					       after_insert.graphemes.size()
					       -cut_pos);
	}
	else
		after_insert.tounicode(after);

	set_contents(before, after);
}

void editablewidechar::insert_to_before()
{
	std::vector<unicode_char> before, after;

	get_contents(before, after);
	set_contents(before, after);
}

void editablewidechar::to_before()
{
	insert_to_before();
	before_insert += after_insert;
	after_insert.clear();
}

void editablewidechar::to_after()
{
	insert_to_before();
	before_insert += after_insert;
	after_insert=before_insert;
	before_insert.clear();
}

void editablewidechar::to_position(size_t o)
{
	insert_to_before();

	std::vector<unicode_char> cut_text, before, after;

	size_t col=0;

	for (std::vector<widecharbuf::grapheme_t>::iterator
		     b(before_insert.graphemes.begin()),
		     e(before_insert.graphemes.end()); b != e; ++b)
	{
		size_t grapheme_width=b->wcwidth(col);

		if (grapheme_width > o)
		{
			contents_cut(b-before_insert.graphemes.begin(),
				     cut_text);
			get_contents(before, after);
			after.insert(after.begin(), cut_text.begin(),
				     cut_text.end());
			set_contents(before, after);
			return;
		}
		o -= grapheme_width;
		col += grapheme_width;
	}

	std::vector<widecharbuf::grapheme_t>::iterator b, e;

	for (b=after_insert.graphemes.begin(),
		     e=after_insert.graphemes.end(); b != e; ++b)
	{
		size_t grapheme_width=b->wcwidth(col);

		if (grapheme_width > o)
			break;
		o -= grapheme_width;
		col += grapheme_width;
	}

	contents_cut(b-after_insert.graphemes.begin(), cut_text);
	get_contents(before, after);
	before.insert(before.end(), cut_text.begin(), cut_text.end());
	set_contents(before, after);
}

size_t editablewidechar::adjust_shift_pos(size_t &shiftoffset,
					  size_t width,
					  widecharbuf &wbefore,
					  widecharbuf &wafter)
{
	{
		std::vector<unicode_char> before, after;
		get_contents(before, after);

		wbefore.init_unicode(before.begin(), before.end());
		wafter.init_unicode(after.begin(), after.end());
	}

	if (wbefore.graphemes.size() < shiftoffset)
		// Cursor position before the current left margin
		shiftoffset=wbefore.graphemes.size();

	size_t last_grapheme_width=0;

	size_t virtual_col=0;

	for (size_t i=0; i<wbefore.graphemes.size(); ++i)
		virtual_col += (last_grapheme_width=wbefore.graphemes[i]
				.wcwidth(virtual_col));
		
	if (shiftoffset > 0
	    && shiftoffset == wbefore.graphemes.size()
	    && width > last_grapheme_width)
		// If the field position is shifted, and the cursor is position
		// on the left margin, and the field width is sufficient to
		// show the last grapheme, shift left. This basically keeps
		// the cursor off the left margin if the field is shifted
		// horizontally.
		--shiftoffset;


	size_t w=0;

	for (std::vector<widecharbuf::grapheme_t>::const_iterator
		     b(wbefore.graphemes.begin()),
		     e(wbefore.graphemes.end()); b != e; ++b)
	{
		if (w >= width)
			break;

		w += b->wcwidth(w);
	}

	for (std::vector<widecharbuf::grapheme_t>::const_iterator
		     b(wafter.graphemes.begin()),
		     e(wafter.graphemes.end()); b != e; ++b)
	{
		if (w >= width)
			break;

		w += b->wcwidth(w);
	}

	if (w < width && shiftoffset != 0)
		// Entire contents fit, so shift offset must be 0
		shiftoffset=0;

	// Compute shift offset for the cursor position to be on the last
	// position in the field.

	w=width;

	if (w)
		--w;

	virtual_col=0;
	for (std::vector<widecharbuf::grapheme_t>::const_iterator
		     b(wbefore.graphemes.begin()),
		     e(wbefore.graphemes.end()); b != e; )
	{
		--e;

		size_t grapheme_width=e->wcwidth(virtual_col);

		if (grapheme_width > w)
		{
			size_t newoffset=e-b+1;

			if (newoffset > shiftoffset)
			{
				// Must shift right.

				shiftoffset=newoffset;
				break;
			}
		}
		w -= grapheme_width;
		virtual_col += w;
	}

	size_t col=0;

	if (shiftoffset < wbefore.graphemes.size())
	{
		virtual_col=0;

		for (std::vector<widecharbuf::grapheme_t>::const_iterator
			     b(wbefore.graphemes.begin()),
			     e(wbefore.graphemes.begin()+shiftoffset);
		     b != e; ++b)
			virtual_col += b->wcwidth(virtual_col);

		for (std::vector<widecharbuf::grapheme_t>::const_iterator
			     b(wbefore.graphemes.begin()+shiftoffset),
			     e(wbefore.graphemes.end()); b != e; ++b)
		{
			size_t grapheme_width=b->wcwidth(virtual_col);

			col += grapheme_width;
			virtual_col += grapheme_width;
		}
	}

	return col;
}

CursesFlowedLine::CursesFlowedLine(const std::vector<unicode_char> &textArg,
				   bool flowedArg)
	: text(mail::iconvert::convert(textArg, "utf-8")), flowed(flowedArg)
{
}

