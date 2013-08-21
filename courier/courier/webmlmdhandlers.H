/*
**
** Copyright 2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef webmlmdhandlers_H
#define webmlmdhandlers_H

#include "config.h"
#include <map>
#include <list>
#include <string>

namespace webmlmd {

#define HANDLER(name,funcname) \
	namespace webmlmd { \
		void funcname(std::list<std::string> &args); \
	} \
		void webmlmd::funcname(std::list<std::string> &args)

	class handler_list
		: public std::map<std::string, void (*)(std::list<std::string> &)>
	{

		struct init_tab_s {
			const char *name;
			void (*handler)(std::list<std::string> &);
		};

		static const init_tab_s init_tab[];

	public:
		handler_list();
		~handler_list();
	};
}

#endif
