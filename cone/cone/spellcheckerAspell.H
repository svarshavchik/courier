/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef spellcheckerAspell_H
#define spellcheckerAspell_H

#include <string>
#include <vector>

#if HAVE_ASPELL_CONFIG_HH
#include "aspell/config.hh"
#include "aspell/manager.hh"
#else

#include <aspell.h>
#endif

#include "spellcheckerbase.H"

//
// aspell-based spell checking interface.

class SpellChecker :

#if HAVE_ASPELL_CONFIG_HH
	private aspell::Config,
#endif
  public SpellCheckerBase {

	std::string c_errmsg;
	std::string language;
	std::string encoding;

	SpellChecker(const SpellChecker &); // UNDEFINED
	SpellChecker &operator=(const SpellChecker &); // UNDEFINED

#if HAVE_ASPELL_CONFIG_HH

#else
	AspellConfig *config;
#endif
public:
	class Manager :

#if HAVE_ASPELL_CONFIG_HH
		private aspell::Manager,
#endif
			public SpellCheckerBase::Manager {

		Manager(const Manager &); // UNDEFINED
		Manager &operator=(const Manager &); // UNDEFINED

#if HAVE_ASPELL_CONFIG_HH

#else
		AspellSpeller *speller;
#endif

	public:
		Manager(
#if HAVE_ASPELL_CONFIG_HH
			aspell::Config &spellerArg
#else
			AspellSpeller *spellerArg
#endif
			);
		~Manager();

		std::string search(std::string word, bool &found);

		bool suggestions(std::string word,
				 std::vector<std::string> &suggestionList,
				 std::string &errmsg);

		std::string replace(std::string word, std::string replacement);

		std::string addToPersonal(std::string word);
		std::string addToSession(std::string word);

	};

	SpellChecker(std::string languageArg,
		     std::string encodingArg);
	virtual ~SpellChecker();

	virtual void setDictionary(std::string);

	SpellCheckerBase::Manager *getManager(std::string &errmsg);
};

#endif
