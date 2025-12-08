/*
** Copyright 2003, S. Varshavchik.
**
** See COPYING for distribution information.
*/
#include <typeahead.H>

Typeahead *Typeahead::typeahead=NULL;

Typeahead::Typeahead()
{
	typeahead=this;
}

Typeahead::~Typeahead()
{
}

