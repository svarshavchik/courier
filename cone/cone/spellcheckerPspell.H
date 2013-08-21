/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef spellcheckerpspell_H
#define spellcheckerpspell_H

#include <string>
#include <vector>

#include "pspell/pspell.h"
#include "spellcheckerbase.H"

//
// A pspell-based spell checker.
//

class SpellChecker : public SpellCheckerBase {

	std::string language;
	std::string encoding;

	PspellConfig *configClass;

	SpellChecker(const SpellChecker &); // UNDEFINED
	SpellChecker &operator=(const SpellChecker &); // UNDEFINED

public:
	class Manager : public SpellCheckerBase::Manager {
		PspellManager *speller;
		PspellCanHaveError *speller_error;

		Manager(const Manager &); // UNDEFINED
		Manager &operator=(const Manager &); // UNDEFINED

	public:
		Manager(PspellManager *spellerArg,
			PspellCanHaveError *speller_errorArg);
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
