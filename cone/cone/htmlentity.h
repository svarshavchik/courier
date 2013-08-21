#ifndef htmlentity_h
#define htmlentity_h

#include "config.h"
#include "unicode/unicode.h"
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

	std::map<unicode_char, unicodeEntity> altlist;
	std::string chset;

public:
	typedef enum {
		none,
		minimum,
		maximum } demoron_t;

	Demoronize(const std::string &chsetArg,
		   demoron_t dodemoronize);
	~Demoronize();

	std::string alt(unicode_char ch) const;
	std::string operator()(const std::vector<unicode_char> &uc,
			       bool &errflag);

	const std::vector<unicode_char>
		&expand(const std::vector<unicode_char> &uc,
			std::vector<unicode_char> &buffer);

};

#endif
