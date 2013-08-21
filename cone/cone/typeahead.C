/*
** Copyright 2003, Double Precision Inc.
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

