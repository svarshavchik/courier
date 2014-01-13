/*
** Copyright 2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef widechar_H
#define widechar_H

#include "../curses/curses_config.h"
#include <unicode.h>
#include <string.h>

#include <wchar.h>
#include <vector>
#include <iterator>

/*
** Convert a char sequence defined by beg_iter and end_iter iterators into
** a wchar_t sequence, written to the out_iter output iterator
*/

template<typename input_iter, typename output_iter>
output_iter towidechar(input_iter beg_iter,
		       input_iter end_iter,
		       output_iter out_iter)
{
	mbstate_t ps;
	memset(&ps, 0, sizeof(ps));

	std::vector<char> cbuf;
	cbuf.reserve(MB_CUR_MAX*2);

	int cnt=MB_CUR_MAX*2;

	while (1)
	{
		if (cnt == 0 || beg_iter == end_iter)
		{
			const char *b=&cbuf[0];
			const char *e=&cbuf[cbuf.size()];
			size_t cnt=0;

			while (b != e)
			{
				wchar_t wc;
				size_t r=mbrtowc(&wc, b, e-b, &ps);

				if (r == (size_t)-1)
				{
					wc='?';
					memset(&ps, 0, sizeof(ps));
					r=1;
				}

				if (r > (size_t)(e-b))
					// must be (size_t)-2 -
					// Incomplete multibyte sequence
					break;

				if (r == 0)
				{
					wc=0;
					r=1;
				}

				*out_iter++=wc;

				b += r;
				cnt += r;
			}
			cbuf.clear();
			cnt=MB_CUR_MAX;

			if (beg_iter == end_iter)
				break;
		}

		cbuf.push_back(*beg_iter++);
		--cnt;
	}

	return out_iter;
}

/*
** Convenience function.
*/

template<typename input_iter>
void towidechar(input_iter beg_iter,
		input_iter end_iter,
		std::vector<wchar_t> &uc)
{
	uc.clear();

	towidechar(beg_iter, end_iter,
		   std::back_insert_iterator<std::vector<wchar_t> >(uc));
}

/*
** Convert a wchar_t sequence defined by beg_iter and end_iter iterators into
** a char sequence, written to the out_iter output iterator
*/

template<typename input_iter,
	 typename output_iter>
output_iter fromwidechar(input_iter b, input_iter e, output_iter out_iter)
{
	mbstate_t ps;
	std::vector<char> buf;

	buf.resize(MB_CUR_MAX*2);

	memset(&ps, 0, sizeof(ps));

	for (; b != e; ++b)
	{
		size_t n=wcrtomb(&buf[0], *b, &ps);

		if (n == (size_t)-1)
			continue;

		out_iter=std::copy(buf.begin(), buf.begin()+n, out_iter);
	}

	size_t n=wcrtomb(&buf[0], 0, &ps);

	if (n != (size_t)-1)
	{
		if (n > 0 && buf[n-1] == 0)
			--n;
		out_iter=std::copy(buf.begin(), buf.begin()+n, out_iter);
	}
	return out_iter;
}

/*
** Convenience function.
*/

template<typename input_iter>
std::string fromwidechar(input_iter beg_iter,
			 input_iter end_iter)
{
	std::string s;

	fromwidechar(beg_iter, end_iter,
		     std::back_insert_iterator<std::string>(s));

	return s;
}

// A unicode string demarcated at grapheme boundaries

class widecharbuf {

public:
	widecharbuf() {}

	// Initialize from a string

	void init_string(const std::string &str);

	// Given the maximum desired column width of the string,
	// return a pair of string, and the width of the returned
	// string.
	std::pair<std::string, size_t>
	get_string_truncated(size_t maxwidth, ssize_t atcol) const;

	// Return as a unicode string
	std::pair<std::vector<unicode_char>, size_t>
	get_unicode_truncated(size_t maxwidth, ssize_t atcol) const;

	// Return a substring
	std::string get_substring(size_t first_grapheme,
				  size_t grapheme_cnt) const;

	// Return a unicode substring
	std::vector<unicode_char> get_unicode_substring(size_t first_grapheme,
							size_t grapheme_cnt)
		const;

	// Return as a unicode string, truncated or padded to the given width

	std::vector<unicode_char> get_unicode_fixedwidth(size_t width,
							 ssize_t atcol) const;

	// Initialize from a beginning and ending iterator, iterating
	// over unicode_chars.

	template<typename iter_type>
	void init_unicode(iter_type b, iter_type e)
	{
		std::string s;

		unicode::iconvert::fromu::convert(b, e,
					       unicode_default_chset(), s);

		std::vector<wchar_t> wc;

		towidechar(s.begin(), s.end(), wc);

		s=fromwidechar(wc.begin(), wc.end());

		unicode::iconvert::tou::convert(s.begin(), s.end(),
					     unicode_default_chset(),
					     ustring);
		resetgraphemes();
	}

	// The unicode string

	std::vector<unicode_char> ustring;

	// A grapheme: a pointer somewhere in ustring, plus wchar count

	class grapheme_t {

	public:
		const unicode_char *uptr; // Offset into ustring
		size_t cnt; // How many unicode chars in the grapheme

		grapheme_t(const unicode_char *uptrArg, size_t cntArg)
			: uptr(uptrArg), cnt(cntArg) {}

		size_t wcwidth(ssize_t start_col) const;
	};

	std::vector<grapheme_t> graphemes;

	// Reset wchar_t in each grapheme
	void resetgraphemes();

	void clear()
	{
		ustring.clear();
		graphemes.clear();
	}

	ssize_t expandtabs(ssize_t col);

	// Append
	widecharbuf &operator+=(const widecharbuf &);

	// Replace

	widecharbuf(const widecharbuf &o)
	{
		operator=(o);
	}

	widecharbuf &operator=(const widecharbuf &o);

	size_t wcwidth(ssize_t start_col) const;

	void tounicode(std::vector<unicode_char> &text) const;

	static size_t charwidth(wchar_t ch, ssize_t atcol);
};

//
// Output iterator for towidechar() that throws away the wide character,
// but keeps track of its width.
//
// widecharbuf::grapheme_t::wcwidth() is hot. Saving the output of
// towidechar() into a vector is very expensive.

class towidechar_wcwidth_iter
	: public std::iterator<std::output_iterator_tag, void, void, void, void>
{

	size_t col;
	size_t w;

public:

	towidechar_wcwidth_iter(size_t colArg) : col(colArg), w(0) {}

	towidechar_wcwidth_iter &operator++() { return *this; }
	towidechar_wcwidth_iter &operator++(int) { return *this; }
	towidechar_wcwidth_iter &operator*() { return *this; }

	void operator=(wchar_t wc)
	{
		size_t ww=widecharbuf::charwidth(wc, col);

		col += ww;
		w += ww;
	}

	operator size_t() const { return w; }
};

// Editable wchar_ts, together with an insertion point.
//
// The wchar_ts get internally divided into three parts: before the
// insertion point, at the insertion point, and past the insertion
// point. Before and after contain valid graphemes. The current
// insertion point may or may not contain valid graphemes.
//
// set_contents() initializes wchar_ts before and after the
// insertion point, setting at the insertion point to an empty list.
//
// add() adds a wchar_t at the insertion point.
//
// get_contents() retrieves the contents as unicode characters,
// combining the insertion point wchar_ts with wchar_ts before the
// insertion point.
//
// contents_cut() removes graphemes between the cut_pos and the
// insertion point. contents_cut() may only be invoked when the
// insertion point is empty. The removed graphemes get placed into
// cut_text.
//
// insert_to_before() calls get_contents() then set_contents(),
// essentially converting wchar_ts at the insertion point to
// valid graphemes, and moving them to before_insert.
//
// to_before() calls insert_to_before(), then moves after_insert
// to before_insert().
//
// to_after() calls insert_to_before(), then moves before_isnert
// to after_insert().
//
// to_position() calls insert_to_before() then moves the insertion
// point to the given position.
//
// adjust_shift_position() calculates horizontal scrolling. It takes
// the editable field's width, and a reference to the current
// horizontal shift position, which gets adjusted, as necessary, to
// keep the cursor position visible, and returns the cursor position.

class editablewidechar {

public:
	widecharbuf before_insert;

	std::vector<unicode_char> inserted;

	widecharbuf after_insert;

	void set_contents(const std::vector<unicode_char> &before,
			  const std::vector<unicode_char> &after);

	void get_contents(std::vector<unicode_char> &before,
			  std::vector<unicode_char> &after) const;

	void contents_cut(size_t cut_pos,
			  std::vector<unicode_char> &cut_text);

	void insert_char(unicode_char wc) { inserted.push_back(wc); }

	void insert_to_before();

	void to_before();

	void to_after();

	void clear()
	{
		before_insert.clear();
		after_insert.clear();
		inserted.clear();
	}

	void to_position(size_t pos);

	size_t adjust_shift_pos(size_t &shiftoffset, size_t width,

				// wbefore and wafter: wide characters
				// before and after cursor position,
				// as computed by adjust_shift_pos().
				// Useful to the caller
				widecharbuf &wbefore,
				widecharbuf &wafter);
};

/*
** Helper class for the word-wrapping logic.
**
** This class receives unicode_char pieces, that can be broken. The collector
** converts them to system wchars, measures each piece, and collects them
** until the next piece can't fit within the alloted width.
*/

template<typename output_sink>
class wordwrap_collector {

public:
	/* Collected line */
	std::vector<unicode_char> linebuf;

	/* Width of the collected line */
	size_t col;

	/*
	** iter(line) gets invoked for each assembled line, where
	** line is a std::vector<wchar_t>.
	*/

	output_sink &iter;

	/*
	** Desired line width.
	*/

	size_t towidth;

	/*
	** If true, the trailing space on each line gets removed.
	** In all cases, each line is wrapped at towidth() characters,
	** where possible.
	*/

	bool delsp;

	wordwrap_collector(output_sink &iterArg,
			   size_t towidthArg,
			   bool delspArg) : col(0), iter(iterArg),
					    towidth(towidthArg),
					    delsp(delspArg)
	{
	}

	void addsegment(const std::vector<unicode_char> &segment)
	{
		std::vector<wchar_t> wsegment;

		{
			std::string s=unicode::iconvert
				::convert(segment, unicode_default_chset());

			towidechar(s.begin(), s.end(), wsegment);

			if (wsegment.empty())
				return;
		}

		size_t width=0;

		for (std::vector<wchar_t>::const_iterator
			     b(wsegment.begin()),
			     e(wsegment.end()); b != e; ++b)
			width += widecharbuf::charwidth(*b, col+width);

		if (!(delsp && width + col == towidth+1 &&
		      *--wsegment.end() == ' '))
		{
			if (width + col > towidth && !linebuf.empty())
				breakline();
		}

		linebuf.insert(linebuf.end(), segment.begin(), segment.end());
		col += width;
	}

	void breakline()
	{
		if (delsp && !linebuf.empty() &&
		    linebuf[linebuf.size()-1] == ' ')
			linebuf.pop_back();

		*iter++=linebuf;
		col=0;
		linebuf.clear();
	}
};


/*
** A default rewrap helper object that does not rewrap anything.
*/

class unicoderewrapnone {

public:
	bool operator()(size_t n) const;
};

/*
** Unicode-based linewrapping logic.
**
** This template defines an output iterator that takes in unicode_chars.
** The constructor receives a rewrap helper object reference, an output
** iterator, the requested width, and whether to trim the trailing
** space from each wrapped line.
**
** As this iterator is iterated over unicode_chars, it will iterate the
** received output iterator over std::vector<unicode_char>s, representing
** each line wrapped to the requested width.
**
** The trim flag should normally be false. This properly preserves all
** whitespace in the unicode character sequence. The trim flag may be true
** only in contexts where the wrapped text will never be rewrapped again.
** Removal of a trailing space on each line allows an extra character to be
** present instead of the trailing space.
**
** The rewrap helper object instance must define a bool operator()(size_t n)
** const. size_t receives a character offset, and should return true if
** character #n is the first character in an original line of text. If so,
** and the unicode word wrap algorithm does not indicate that there's a
** potential linebreak here, a space gets appended at this point. This is
** used to rewrap existing lines of text which may not end with a space
** character.
**
** After the original unicode chars are iterated over, eof() must be
** invoked in order to output any partially-wrapped content that's still
** held internally in this iterator.
*/

template<typename output_sink_t,
	 typename rewrap_helper_t=unicoderewrapnone> class unicodewordwrapper :
	public std::iterator<std::output_iterator_tag, void, void, void, void> {

	// State maintained by the iterator. This iterator is copyable.
	// The state is associated with only one iterator instance. Copying
	// an iterator copies the state from the original iterator into the
	// new one.

	class buffer : public unicode::linebreakc_callback_save_buf {

	public:
		std::vector<unicode_char> segment; // Current word
		size_t cnt; // Counts characters that are being wrapped.

		wordwrap_collector<output_sink_t> collector;
		// The collector object.

		const rewrap_helper_t &rewrapper;
		// The rewrap helper object.

		buffer(const rewrap_helper_t &rewrapperArg,
		       output_sink_t &out_iter,
		       size_t towidth,
		       bool delsp)
			: cnt(0),
			  collector(out_iter, towidth, delsp),
			  rewrapper(rewrapperArg) {}
	};

	mutable buffer *buf;

	typedef unicodewordwrapper<output_sink_t, rewrap_helper_t> iter_t;

public:
	// Iterator constructor
	unicodewordwrapper(const rewrap_helper_t &rewrap_helper,
			   output_sink_t &out_iter,
			   size_t towidth,
			   bool delsp)
		: buf(new buffer(rewrap_helper, out_iter, towidth, delsp))
	{
		buf->set_opts(UNICODE_LB_OPT_PRBREAK|
			      UNICODE_LB_OPT_SYBREAK|
			      UNICODE_LB_OPT_DASHWJ);
	}

	// End iterator constructor
	unicodewordwrapper() : buf(NULL)
	{
	}

	~unicodewordwrapper()
	{
		eof();
	}

	// Assignment operator moves the state

	iter_t &operator=(const iter_t &o)
	{
		if (buf)
			delete buf;

		buf=o.buf;
		o.buf=NULL;
		return *this;
	}

	// Copy constructor moves the state
	unicodewordwrapper(const iter_t &o) : buf(o.buf)
	{
		o.buf=NULL;
	}

	// Operator implementation

	iter_t &operator++() { return *this; }
	iter_t &operator++(int) { return *this; }
	iter_t &operator*() { return *this; }

	void operator=(unicode_char ch)
	{
		if (!buf)
			return;

		// Feed into the linebreaking algorithm.

		buf->operator<<(ch);

		// Process linebreaking algorithm output.

		while (!buf->lb_buf.empty())
		{
			std::pair<int, unicode_char> ch(buf->lb_buf.front());
			buf->lb_buf.pop_front();

			// If text is being rewrapped, and the linebreaking
			// algorithm prohibits a linebreak here, but this
			// was the first character of the pre-wrapped line,
			// then there must've been a space character here,
			// which allows a break.

			if (ch.first == UNICODE_LB_NONE && buf->cnt > 0 &&
			    (buf->rewrapper)(buf->cnt))
			{
				buf->segment.push_back(' ');
				ch.first=UNICODE_LB_ALLOWED;
			}
			++buf->cnt;

			// Process a potential linebreak.

			if (ch.first != UNICODE_LB_NONE)
			{
				buf->collector.addsegment(buf->segment);

				if (ch.first == UNICODE_LB_MANDATORY)
					buf->collector.breakline();
				buf->segment.clear();
			}

			if (ch.second != '\r' && ch.second != '\n')
				buf->segment.push_back(ch.second);
		}
	}

	// Finish remaining content, and clean up.

	void eof()
	{
		if (buf)
		{
			buf->collector.addsegment(buf->segment);
			if (!buf->collector.linebuf.empty())
				buf->collector.breakline();

			delete buf;
			buf=NULL;
		}
	}
};

// A convenience function to iterate over an arbitrary sequence defined
// by a beginning and an ending iterator, and wrap it.

template<typename input_iter, typename output_sink,
	 typename rewrap_helper_t>
void unicodewordwrap(input_iter beg_iter,
		     input_iter end_iter,
		     const rewrap_helper_t &rewrap_helper,
		     output_sink &out_iter,
		     size_t towidth,
		     bool delsp)
{
	unicodewordwrapper<output_sink, rewrap_helper_t>
		iter(rewrap_helper, out_iter, towidth, delsp);

	while (beg_iter != end_iter)
	{
		iter= *beg_iter;
		++beg_iter;
	}
	iter.eof();
}

#endif
