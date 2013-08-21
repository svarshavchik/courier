/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "gettext.H"
#include "colors.H"
#include "curses/mycurses.H"
#include "colors_inc.h"

using namespace std;


struct CustomColorGroup *getColorGroups()
{
	return allGroups;
}

void initColorGroups()
{
	size_t i;
	int cc=Curses::getColorCount();

	for (i=0; allGroups[i].colors; i++)
	{
		struct CustomColor **colors=allGroups[i].colors;
		size_t j=0;

		for (j=0; colors[j]; j++)
		{
			struct CustomColor *c=colors[j];

			c->fcolor=c->defaultFcolor;

			if (cc)
				c->fcolor=cc - 1 - (c->fcolor % cc);
		}
	}
}
