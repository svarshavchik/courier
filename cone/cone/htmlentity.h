#ifndef htmlentity_h
#define htmlentity_h

#include "config.h"
#include <courier-unicode.h>
#include <string>
#include <vector>
#include <map>

struct unicodeEntity;

/*
** Rich text demoronization: replace unicode characters not displayable
** in the current charset with their US-ASCII equivalents (where possible).
**
** Step 1: Build a hash table of replacable unicode characters
*/

class Demoronize {

	std::map<char32_t, unicodeEntity> altlist;
	std::string chset;

public:
	typedef enum {
		none,
		minimum,
		maximum } demoron_t;

	Demoronize(const std::string &chsetArg,
		   demoron_t dodemoronize);
	~Demoronize();

	std::string alt(char32_t ch) const;
	std::string operator()(const std::u32string &uc,
			       bool &errflag);

	const std::u32string
		&expand(const std::u32string &uc,
			std::u32string &buffer);

};

#endif
