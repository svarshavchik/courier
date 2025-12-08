/*
** Copyright 2003, S. Varshavchik.
**
** See COPYING for distribution information.
*/

#ifndef gettext_H
#define gettext_H

#include "config.h"
#include <courier-unicode.h>

#include <libintl.h>
#include <locale.h>

// A helper class for C++ interface to gettext
//
// E.G.  Gettext(_("Foo %1% bar %2%")).arg(string1).arg(string2)

#include <string>
#include <vector>
#include <wchar.h>

#define _(s) gettext(s)
#define N_(s) s

class Gettext {

	std::string paramStr;

	std::vector<std::string> args;

public:
	Gettext(std::string param);
	~Gettext();
	operator std::string() const;

	Gettext &arg(std::string);
	Gettext &arg(const char *);
	Gettext &arg(unsigned long ul);
	Gettext &arg(long l);
	Gettext &arg(unsigned u);
	Gettext &arg(int i);
	Gettext &arg(unsigned short us);
	Gettext &arg(short s);

	Gettext &operator<<(std::string s) { return arg(s); }
	Gettext &operator<<(const char *s) { return arg(s); }
	Gettext &operator<<(unsigned long ul) { return arg(ul); }
	Gettext &operator<<(long l) { return arg(l); }

	Gettext &operator<<(unsigned u) { return arg(u); }
	Gettext &operator<<(int i) { return arg(i); }

	Gettext &operator<<(unsigned short us) { return arg(us); }
	Gettext &operator<<(short s) { return arg(s); }

	// Bonus.  Allow limited reprogramming of keys via gettext.
	//
	// ... keyname(_("DESCRIPTION:key")) returns "key"


	static const char *keyname(const char *k);

	static std::string toutf8(std::string);
	static std::string fromutf8(std::string);


	class Key {
		const char *keyname;
		std::u32string keys;
	public:
		Key(const char *);
		~Key();

		bool operator==(char32_t k);

		operator const std::u32string &()
		{
			init();
			return keys;
		}
	private:
		void init();
	};
};

extern void outofmemory();

#endif

