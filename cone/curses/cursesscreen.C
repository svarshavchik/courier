/*
** Copyright 2002-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "curses_config.h"
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>

#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif
#ifndef WIFEXITED
#define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif

#include "cursesscreen.H"
#include "cursesfield.H"

static unsigned char termStopKey= 'Z' & 31;

static RETSIGTYPE bye(int dummy)
{
	endwin();
	kill( getpid(), SIGKILL);
	_exit(0);
}

int Curses::getColorCount()
{
	if (has_colors())
		return COLORS;
	return 0;
}


CursesScreen::KeyReader::KeyReader()
	: h(iconv_open(unicode_u_ucs4_native, unicode_default_chset())),
	  winput_cnt(0)
{
	if (h == (iconv_t)-1)
	{
		std::cerr << "Unable to initiale "
			  << unicode_default_chset()
			  << " mapping to UCS-4" << std::endl;
		exit(1);
	}
}

CursesScreen::KeyReader::~KeyReader()
{
	if (h != (iconv_t)-1)
		iconv_close(h);
}

void CursesScreen::KeyReader::operator<<(char ch)
{
	input_buf.push_back(ch);

	winput_buf.resize(winput_cnt+128);

	while (input_buf.size() > 0)
	{

		char *inbuf=&input_buf[0], *outbuf=&winput_buf[winput_cnt];
		size_t inbytesleft=input_buf.size(),
			outbytesleft=winput_buf.size()-winput_cnt;

		size_t res=iconv(h, &inbuf, &inbytesleft, &outbuf,
				 &outbytesleft);

		if (inbuf != &input_buf[0])
			input_buf.erase(input_buf.begin(),
					input_buf.begin() +
					(inbuf - &input_buf[0]));

		winput_cnt=outbuf - &winput_buf[0];

		if (res == (size_t)-1 && errno == EILSEQ)
		{
			if (input_buf.size() > 0)
				input_buf.erase(input_buf.begin(),
						input_buf.begin()+1);
			continue;
		}

		if (res == (size_t)-1 && errno == E2BIG)
			winput_buf.resize(winput_buf.size()+128);

		if (res == (size_t)-1 && errno == EINVAL)
			break;
	}
}

bool CursesScreen::KeyReader::operator>>(unicode_char &ch)
{
	if (winput_cnt < sizeof(ch))
		return false;

	memcpy(&ch, &winput_buf[0], sizeof(ch));

	winput_buf.erase(winput_buf.begin(),
			 winput_buf.begin()+sizeof(ch));
	winput_cnt -= sizeof(ch);
	return true;
}

CursesScreen::CursesScreen() : inputcounter(0)
{
	initscr();
	start_color();

#if HAS_USE_DEFAULT_COLORS
	use_default_colors();
#endif

	// Assign meaningful colors

	if (has_colors())
	{
		short f;
		short b;

		int i;

		pair_content(0, &f, &b);

		int colorcount=getColorCount();

		if (colorcount > 0)
			--colorcount;

		for (i=1; i<COLOR_PAIRS; i++)
		{
			if (colorcount <= 1)
				f=b=0;
			else
			{
				f=(i / COLORS) % COLORS;
				b=(i % COLORS);
			}

#if HAS_USE_DEFAULT_COLORS
			if (b == 0)
				b= -1;
#endif
			init_pair(i, f, b);
		}
	}

	raw();
	noecho();
	nodelay(stdscr, true);
	timeout(0);
	nonl();
	intrflush(stdscr, FALSE);
	keypad(stdscr, TRUE);

	//	fcntl(0, F_SETFL, O_NONBLOCK);

	signal(SIGHUP, bye);
	signal(SIGTERM, bye);

	save_w=COLS;
	save_h=LINES;
	shiftmode=false;

	const char *underline_hack_env=getenv("UNDERLINE_HACK");

	underline_hack=underline_hack_env && atoi(underline_hack_env);
}

CursesScreen::~CursesScreen()
{
	endwin();
}

int CursesScreen::getWidth() const
{
	return COLS;
}

int CursesScreen::getHeight() const
{
	return LINES;
}

void CursesScreen::draw()
{
	attroff(A_STANDOUT | A_BOLD | A_UNDERLINE | A_REVERSE);
	clear();
	CursesContainer::draw();
}

void CursesScreen::resized()
{
	CursesContainer::resized();
	draw();
}

bool CursesScreen::writeText(const char *ctext, int row, int col,
			     const Curses::CursesAttr &attr) const
{
	std::vector<unicode_char> ubuf;

	unicode::iconvert::convert(ctext, unicode_default_chset(), ubuf);

	return writeText(ubuf, row, col, attr);
}

//
// Eliminate time-consuming expantabs() call in the hot writeText() codepath
// by loading widecharbuf from an iterator that replaces tabs with a single
// space character. writeText() should not get a string with tabs anyway.
//
// As a bonus, any NUL character gets also replaced with a space.
//

class CursesScreen::repltabs_spaces :
	public std::iterator<std::random_access_iterator_tag,
			     const unicode_char> {

	const unicode_char *p;

	unicode_char tmp;

public:
	repltabs_spaces() : p(0), tmp(0) {}

	repltabs_spaces(const unicode_char *pVal) : p(pVal), tmp(0) {}

	bool operator==(const repltabs_spaces &v)
	{
		return p == v.p;
	}

	bool operator!=(const repltabs_spaces &v)
	{
		return p != v.p;
	}

	bool operator<(const repltabs_spaces &v)
	{
		return p < v.p;
	}

	bool operator<=(const repltabs_spaces &v)
	{
		return p <= v.p;
	}

	bool operator>(const repltabs_spaces &v)
	{
		return p < v.p;
	}

	bool operator>=(const repltabs_spaces &v)
	{
		return p <= v.p;
	}

	unicode_char operator*() const { return operator[](0); }

	repltabs_spaces &operator++() { ++p; return *this; }

	const unicode_char *operator++(int) { tmp=operator*(); ++p; return &tmp; }

	repltabs_spaces &operator--() { --p; return *this; }

	const unicode_char *operator--(int) { tmp=operator*(); --p; return &tmp; }

	repltabs_spaces &operator+=(difference_type diff) { p += diff; return *this; }

	repltabs_spaces &operator-=(difference_type diff) { p -= diff; return *this; }

	repltabs_spaces operator+(difference_type diff) const { repltabs_spaces tmp=*this; tmp += diff; return tmp; }
	repltabs_spaces operator-(difference_type diff) const { repltabs_spaces tmp=*this; tmp -= diff; return tmp; }

	difference_type operator-(const repltabs_spaces b) const { return p-b.p; }

	const unicode_char operator[](difference_type diff) const { return p[diff] == 0 || p[diff] == '\t' ? ' ':p[diff]; }
};


//
// When writing EM and EN dashes, use the EM dash character, and write it out
// multiple times. Corresponds to logic in widecharbuf::charwidth().
//

class CursesScreen::writetext_iter_helper
	: public std::iterator<std::input_iterator_tag,
			       char, void, void, void> {

	const unicode_char *uptr;
	size_t multcnt;

public:
	writetext_iter_helper(const unicode_char *uptrArg)
		: uptr(uptrArg), multcnt(0) {}

	unicode_char operator*()
	{
		unicode_char c=*uptr;

		if (c == 0x2014)
			c=0x2013;
		return c;
	}

	writetext_iter_helper &operator++()
	{
		if (multcnt == 0)
			switch (*uptr) {
			case 0x2013:
				multcnt=2;
				break;
			case 0x2014:
				multcnt=3;
				break;
			default:
				multcnt=1;
			}

		if (--multcnt == 0)
			++uptr;
		return *this;
	}

	writetext_iter_helper operator++(int)
	{
		writetext_iter_helper helper=*this;

		operator++();
		return helper;
	}

	bool operator==(const writetext_iter_helper &o) const
	{
		return uptr == o.uptr;
	}

	bool operator!=(const writetext_iter_helper &o) const
	{
		return !operator==(o);
	}
};



bool CursesScreen::writeText(const std::vector<unicode_char> &utext,
			     int row, int col,
			     const Curses::CursesAttr &attr) const
{
	// Truncate text to fit within the display

	if (row < 0 || row >= getHeight() || col >= getWidth())
		return false;

	widecharbuf wc;

	{
		repltabs_spaces ptr(utext.size() ? &*utext.begin():NULL);

		wc.init_unicode(ptr, ptr+utext.size());
	}

	// Advance past any part of the script beyond the left margin

	bool left_margin_crossed=false;

	std::vector<widecharbuf::grapheme_t>::const_iterator
		b(wc.graphemes.begin()), e(wc.graphemes.end());

	if (b != e)
	{
		while (col < 0)
		{
			if (b == e)
				return false;

			col += b->wcwidth(col);
			++b;
			left_margin_crossed=true;
		}

		if (col < 0 || b == e)
			return false;
	}

	if (left_margin_crossed)
	{
		// Clear any cells that contain a partial graphemes that
		// extends from beyond the left margin
		move(row, 0);

		for (int i=0; i<col; i++)
			addstr(" ");
	}
	else
		move(row, col);

	int w=getWidth();

	if (col >= w)
		return false;

	if (b == e)
		return true;

	size_t cells=w-col;

	const unicode_char *uptr=b->uptr;
	size_t ucnt=0;

	bool right_margin_crossed=false;

	while (b != e)
	{
		size_t grapheme_width=b->wcwidth(col);

		if (grapheme_width > cells)
		{
			if (cells)
				right_margin_crossed=true;
			break;
		}

		ucnt += b->cnt;
		cells -= grapheme_width;
		col += grapheme_width;
		++b;
	}

	std::vector<wchar_t> text_buf;

	{
		std::string s;

		unicode::iconvert::fromu::convert(writetext_iter_helper(uptr),
					       writetext_iter_helper(uptr+ucnt),
					       unicode_default_chset(), s);

		towidechar(s.begin(), s.end(), text_buf);
	}
	text_buf.push_back(0);

	wchar_t *text=&text_buf[0];

	int c=0;

	// If color was requested, then the colors will wrap if the requested
	// color number exceeds the # of colors reported by curses.

	int ncolors=getColorCount();

	if (ncolors > 2 && COLOR_PAIRS > 0)
	{
		int bg=attr.getBgColor();
		int fg=attr.getFgColor();

		if (fg || bg)
		{
			if (bg > 0)
				bg= ((bg - 1) % (ncolors-1)) + 1;

			fg %= ncolors;

			if (fg == bg)
				fg = (fg + ncolors/2) % ncolors;
		}

		c=bg + ncolors * fg;

		c %= COLOR_PAIRS;
	}

	if (c > 0)
		attron(COLOR_PAIR(c));

	if (attr.getHighlight())
		attron(A_BOLD);
	if (attr.getReverse())
		attron(A_REVERSE);

	if (attr.getUnderline())
	{
		if (termattrs() & A_UNDERLINE)
		{
			if (underline_hack)
			{
				size_t i=0;

				for (i=0; text[i]; i++)
					if (text[i] == ' ')
						text[i]=0xA0;
			}
			attron(A_UNDERLINE);
		}
		else
		{
			size_t i;

			for (i=0; text[i]; i++)
				if (text[i] == ' ')
					text[i]='_';
		}
	}

	addwstr(text);

	if (c)
		attroff(COLOR_PAIR(c));
	attroff(A_STANDOUT | A_BOLD | A_UNDERLINE | A_REVERSE);

	if (right_margin_crossed)
		clrtoeol();

	return true;
}

void CursesScreen::flush()
{
	refresh();
}

void (*Curses::suspendedHook)(void)=&Curses::suspendedStub;

void Curses::suspendedStub(void)
{
}

void Curses::setSuspendHook(void (*func)(void))
{
	suspendedHook=func;
}

Curses::Key CursesScreen::getKey()
{
	for (;;)
	{
		Key k=doGetKey();

		if (k.plain())
		{
			if (k.plain() && k.ukey == termStopKey)
			{
				if (suspendCommand.size() > 0)
				{
					std::vector<const char *> argv;

					const char *p=getenv("SHELL");

					if (!p || !*p)
						p="/bin/sh";

					argv.push_back(p);
					argv.push_back("-c");
					argv.push_back(suspendCommand.c_str());
					argv.push_back(NULL);
					Curses::runCommand(argv, -1, "");
					draw();
					(*suspendedHook)();
					continue;
				}

				endwin();
				kill( getpid(), SIGSTOP );
				draw();
				refresh();
				(*suspendedHook)();
				continue;
			}
		}
		else if (k == Key::RESIZE)
		{
			save_w=COLS;
			save_h=LINES;

			::erase(); // old window
			resized();
			touchwin(stdscr);
			flush();
			draw();
			flush();
		}
		return k;
	}
}

int Curses::runCommand(std::vector<const char *> &argv,
		       int stdinpipe,
		       std::string continuePrompt)
{
	endwin();
	if (continuePrompt.size() > 0)
	{
		std::cout << std::endl << std::endl;
		std::cout.flush();
	}

	pid_t editor_pid, p2;

#ifdef WIFSTOPPED

	RETSIGTYPE (*save_tstp)(int)=signal(SIGTSTP, SIG_IGN);
	RETSIGTYPE (*save_cont)(int)=signal(SIGCONT, SIG_IGN);
#endif

	editor_pid=fork();
	int waitstat;

	signal(SIGCHLD, SIG_DFL);

	if (editor_pid < 0)
	{
#ifdef WIFSTOPPED
		signal(SIGTSTP, save_tstp);
		signal(SIGCONT, save_cont);
#endif
		beep();
		return -1;
	}

	if (editor_pid == 0)
	{
		signal(SIGTSTP, SIG_DFL);
		signal(SIGCONT, SIG_DFL);
		if (continuePrompt.size() > 0)
		{
			dup2(1, 2);
		}
		signal(SIGINT, SIG_DFL);
		if (stdinpipe >= 0)
		{
			dup2(stdinpipe, 0);
			close(stdinpipe);
		}
		execvp(argv[0], (char **)&argv[0]);
		kill(getpid(), SIGKILL);
		_exit(1);
	}

	if (stdinpipe >= 0)
		close(stdinpipe);

	signal(SIGINT, SIG_IGN);

	for (;;)
	{

#ifdef WIFSTOPPED

		p2=waitpid(editor_pid, &waitstat, WUNTRACED);

		if (p2 != editor_pid)
			continue;

		if (WIFSTOPPED(waitstat))
		{
			kill(getpid(), SIGSTOP);
			kill(editor_pid, SIGCONT);
			continue;
		}
#else

		p2=wait(&waitstat);

		if (p2 != editor_pid)
			continue;
#endif
		break;
	}

	signal(SIGINT, SIG_DFL);

#ifdef WIFSTOPPED
	signal(SIGTSTP, save_tstp);
	signal(SIGCONT, save_cont);
#endif

	if (continuePrompt.size() > 0)
	{
		char buffer[80];

		std::cout << std::endl << continuePrompt;
		std::cout.flush();
		std::cin.getline(buffer, sizeof(buffer));
	}

	refresh();

	return WIFEXITED(waitstat) ? WEXITSTATUS(waitstat):-1;
}

Curses::Key CursesScreen::doGetKey()
{
	// If, for some reason, a unicode key is available, take it

	{
		unicode_char uk;

		if (keyreader >> uk)
			return doGetPlainKey(uk);
	}

	// Position cursor first.

	int row=getHeight();
	int col=getWidth();

	if (row > 0) --row;
	if (col > 0) --col;

	if (currentFocus != NULL)
	{
		int new_row=currentFocus->getHeight();
		if (new_row > 0) --new_row;

		int new_col=currentFocus->getWidth();
		if (new_col > 0) --new_col;
		curs_set(currentFocus->getCursorPosition(new_row, new_col));

		if (new_row < row)
			row=new_row;

		if (new_col < col)
			col=new_col;
	}
	else
		curs_set(0);

	move(row, col);

	// As long as the bytes from the keyboard keep getting read,
	// accumulate them.

	while (1)
	{
		int k=getch();

		if (k == ERR)
		{
			flush();

			if (++inputcounter >= 100)
			{
				// Detect closed xterm without SIGHUP

				if (write(1, "", 1) < 0)
					bye(0);
				inputcounter=0;
			}

			if (LINES != save_h || COLS != save_w)
				return (Key(Key::RESIZE));

			return Key((unicode_char)0);
		}
		inputcounter=0;

#ifdef KEY_SUSPEND

		if (k == KEY_SUSPEND)
			k=termStopKey;
#endif

		static const struct {
			int keyval;
			const char *keycode;
		} keys[]={

#define K(v,c) { v, Curses::Key::c }

			K(KEY_LEFT, LEFT),
			K(KEY_RIGHT, RIGHT),
			K(KEY_SLEFT, SHIFTLEFT),
			K(KEY_SRIGHT, SHIFTRIGHT),
			K(KEY_UP, UP),
			K(KEY_DOWN, DOWN),
			K(KEY_ENTER, ENTER),
			K(KEY_PPAGE, PGUP),
			K(KEY_NPAGE, PGDN),
			K(KEY_HOME, HOME),
			K(KEY_END, END),
			K(KEY_SHOME, SHIFTHOME),
			K(KEY_SEND, SHIFTEND),
			K(KEY_RESIZE, RESIZE),
			K(KEY_BACKSPACE, BACKSPACE),
			K(KEY_DC, DEL),
			K(KEY_EOL, CLREOL),
#undef K
		};

		if (shiftmode)
		{
			switch (k) {
			case KEY_PPAGE:
				return (Key(Key::SHIFTPGUP));
			case KEY_NPAGE:
				return (Key(Key::SHIFTPGDN));
			case KEY_UP:
				return (Key(Key::SHIFTUP));
			case KEY_DOWN:
				return (Key(Key::SHIFTDOWN));
			case KEY_LEFT:
				k=KEY_SLEFT;
				break;
			case KEY_RIGHT:
				k=KEY_SRIGHT;
				break;
			case KEY_HOME:
				k=KEY_SHOME;
				break;
			case KEY_END:
				k=KEY_SEND;
				break;
			default:
				shiftmode=0;
			}
		}

		size_t i;

		for (i=0; i<sizeof(keys)/sizeof(keys[0]); i++)
			if (keys[i].keyval == k)
				return Key(keys[i].keycode);

#if HAS_FUNCTIONKEYS
		if (k >= KEY_F(0) && k <= KEY_F(63))
		{
			Key kk("FKEY");
			kk.ukey=k-KEY_F(0);
			return kk;
		}
#endif

		if ( (unsigned char)k == k)
		{
			// Plain key

			keyreader << k;

			unicode_char uk;

			if (keyreader >> uk)
				return doGetPlainKey(uk);
		}
	}
}

Curses::Key CursesScreen::doGetPlainKey(unicode_char k)
{
	if (k == 0)
	{
		shiftmode= !shiftmode;

		return Key(Key::SHIFT);
	}

	if (k == (unicode_char)CursesField::clrEolKey)
		return Key(Key::CLREOL);

	if (k == '\r')
		return Key(Key::ENTER);

	shiftmode=false;

	return (Key(k));
}

void CursesScreen::beepError()
{
	beep();
}
