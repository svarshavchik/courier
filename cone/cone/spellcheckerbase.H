/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef spellcheckerbase_H
#define spellcheckerbase_H

#include "config.h"

#include <string>
#include <vector>

//
// A abstract spell-checking interface.  Two subclasses implement the interface
// based on aspell or pspell.
//
// There's a single instance of SpellCheckerBase.  The subclass implements
// setDictionary(), that sets the name of the active spell checking dictionary.
//
// A spell check begins by invoking getManager(), which instantiates a
// subclass of Manager().  getManager()'s argument is an error message
// string, which will be initialized appropriately when getManager() returns
// a NULL.
//
// At the conclusion of the spell checking session, the Manager object should
// be destroyed.
//

class SpellCheckerBase {
public:
	class Manager {
	public:
		Manager();
		virtual ~Manager();

		//
		// Look up a word in a dictionary.  Returns an error message,
		// or an empty string; in which case the bool arg indicates
		// whether the word is found in the dictionary.

		virtual std::string search(std::string, bool &)=0;

		//
		// Try to get a list of suggested replacements.
		// errmsg gets initialized if suggestions() returns false
		// (a fatal error).

		virtual bool suggestions(std::string,
					 std::vector<std::string> &,
					 std::string &errmsg)=0;

		// Inform the spell checker that a replacement took place.

		virtual std::string replace(std::string, std::string)=0;

		// Add to personal dictionary, or ignore the word for the
		// remainder of this spell checking session.

		virtual std::string addToPersonal(std::string)=0;
		virtual std::string addToSession(std::string)=0;

	};

	SpellCheckerBase();
	virtual ~SpellCheckerBase();
		
	virtual Manager *getManager(std::string &)=0;

	std::string dictionaryName;

	virtual void setDictionary(std::string)=0;
};

#endif
