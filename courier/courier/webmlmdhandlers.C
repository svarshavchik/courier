/*
**
** Copyright 2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "webmlmdhandlers.H"

#undef HANDLER
#define HANDLER(name, funcname) void funcname(std::list<std::string> &);

namespace webmlmd {

#include "webmlmdhandlerslist.H"

}

using namespace webmlmd;

const struct handler_list::init_tab_s handler_list::init_tab[]={

#undef HANDLER
#define HANDLER(name, funcname) { name, &funcname },

#include "webmlmdhandlerslist.H"

};

handler_list::handler_list()
{
	size_t i;

	for (i=0; i<sizeof(init_tab)/sizeof(init_tab[0]); ++i)
	{
		(*this)[ init_tab[i].name]= init_tab[i].handler;
	}
}

handler_list::~handler_list()
{
}
