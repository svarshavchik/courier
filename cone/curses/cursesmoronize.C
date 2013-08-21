/*
** Copyright 2003-2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "curses_config.h"
#include "cursesmoronize.H"

bool CursesMoronize::enabled=false;

static const unicode_char plain_169[]={ ')', 'C', '('};
static const unicode_char plain_174[]={ ')', 'R', '('};
static const unicode_char plain_177[]={ '-', '/', '+'};
static const unicode_char plain_188[]={ ' ', '4', '/', '1'};
static const unicode_char plain_189[]={ ' ', '2', '/', '1'};
static const unicode_char plain_190[]={ ' ', '4', '/', '3'};
static const unicode_char plain_8482[]={ ']', 'm', 't', '['};
static const unicode_char plain_8592[]={ '-', '<'};
static const unicode_char plain_8594[]={ '>', '-'};
static const unicode_char plain_8220[]={ '`', '`'};
static const unicode_char plain_8221[]={ '\'', '\''};
static const unicode_char plain_8226[]={ ' ', '*', ' '};
static const unicode_char plain_8230[]={ '.', '.', '.'};
static const unicode_char plain_8211[]={ ' ', '-', '-', ' '};
static const unicode_char plain_8212[]={ ' ', '-', '-', '-', ' '};


static const unicode_char repl_169[]={ 169, 0};
static const unicode_char repl_174[]={ 174, 0};
static const unicode_char repl_177[]={ 177, 0};
static const unicode_char repl_188[]={ 188, 0};
static const unicode_char repl_189[]={ 189, 0};
static const unicode_char repl_190[]={ 190, 0};
static const unicode_char repl_8482[]={ 8482, 0};
static const unicode_char repl_8592[]={ 8592, 0};
static const unicode_char repl_8594[]={ 8594, 0};
static const unicode_char repl_8220[]={ 8220, 0};
static const unicode_char repl_8221[]={ 8221, 0};
static const unicode_char repl_8226[]={ 8226, 0};
static const unicode_char repl_8230[]={ 8230, 0};
static const unicode_char repl_8211[]={ ' ', 8211, ' ', 0};
static const unicode_char repl_8212[]={ ' ', 8212, ' ', 0};


CursesMoronize::Entry CursesMoronize::moronizationList[] = {
	{ plain_169, 3, repl_169},
	{ plain_174, 3, repl_174},
	{ plain_177, 3, repl_177},
	{ plain_188, 4, repl_188},
	{ plain_189, 4, repl_189},
	{ plain_190, 4, repl_190},
	{ plain_8482, 4, repl_8482},
	{ plain_8592, 2, repl_8592},
	{ plain_8594, 2, repl_8594},
	{ plain_8220, 2, repl_8220},
	{ plain_8221, 2, repl_8221},
	{ plain_8226, 3, repl_8226},
	{ plain_8230, 3, repl_8230},
	{ plain_8211, 4, repl_8211},
	{ plain_8212, 5, repl_8212},
	{ NULL, 0, 0}};

size_t CursesMoronize::moronize(const unicode_char *buf,
				std::vector<unicode_char> &nreplaced)
{
	Entry *e=moronizationList;

	nreplaced.clear();

	while (e->keycode)
	{
		size_t i=0;

		for (i=0; ; ++i)
		{
			if (i == e->keycodeLen)
			{
				if (e->replacements == 0)
					return 0;

				for (i=0; e->replacements[i]; ++i)
					nreplaced.push_back(e->replacements[i]);
				return e->keycodeLen;
			}

			if (i == e->keycodeLen || buf[i] == 0)
				break;

			if (e->keycode[i] != buf[i])
				break;
		}

		++e;
	}
	return 0;
}
