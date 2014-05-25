/*
**
** Copyright 2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef webmlmd_H
#define webmlmd_H

#include "config.h"
#include <list>
#include <string>
#include <map>
#include <algorithm>

namespace webmlmd {
	std::string list_name();

	void emit_lists(std::list<std::string> &dummy);
	void emit_list_name(std::list<std::string> &dummy);
	void emit_list_descr(std::list<std::string> &dummy);
	void emit_list_url(std::list<std::string> &dummy);
	void emit_repeat_url(std::list<std::string> &dummy);

	inline void emit_list_url()
	{
		std::list<std::string> dummy;

		emit_list_url(dummy);
	}

	// Common template functions

	inline std::string basename(std::string n)
	{
		static const char slash[]="/";

		n=std::string(std::find_end(n.begin(), n.end(),
					    slash, slash+1),
			      n.end());

		if (n.size() > 0)
			n=n.substr(1);
		return n;
	}

	template<typename T> T html_escape(T s)
	{
		T t;

		t.reserve(s.size() * 6 / 5);
		typename T::iterator b=s.begin(), e=s.end(), p=b;

		while (b != e)
		{
			if (*b == '&')
			{
				t += T(p, b);
				t.push_back('&');
				t.push_back('a');
				t.push_back('m');
				t.push_back('p');
				t.push_back(';');
				p= ++b;
				continue;
			}
			
			if (*b == '<')
			{
				t += T(p, b);
				t.push_back('&');
				t.push_back('l');
				t.push_back('t');
				t.push_back(';');
				p= ++b;
				continue;
			}

			if (*b == '>')
			{
				t += T(p, b);
				t.push_back('&');
				t.push_back('g');
				t.push_back('t');
				t.push_back(';');
				p= ++b;
				continue;
			}

			++b;
		}

		t += T(p, b);

		return t;
	}

	inline std::string html_escape(const char *a)
	{
		return html_escape<std::string>(a);
	}

	inline std::string html_escape(char *a)
	{
		return html_escape<std::string>(a);
	}

	void showhtmlform(const char *formname,
			  std::map<std::string, std::string> &parms);
	void showhtmlform(const char *formname);

	std::string get_next_mod_filename(std::string curfilename,
					  size_t &mod_count);

	std::string do_mod_accept(std::string filename);
	std::string do_mod_reject(std::string filename, bool returntosender,
				  std::string msg);

}

#endif
