/*
** Copyright 2003, S. Varshavchik.
**
** See COPYING for distribution information.
*/

#ifndef spellcheckerNone_H
#define spellcheckerNone_H

#include <string>
#include <vector>

#include "spellcheckerbase.H"

/////////////////////////////////////////////////////////////////////////
//
// When neither aspell or pspell is available, this class provides a
// stub spell checking interface that always fails.

class SpellChecker : public SpellCheckerBase {

	SpellChecker(const SpellChecker &); // UNDEFINED
	SpellChecker &operator=(const SpellChecker &); // UNDEFINED

public:
	class Manager : public SpellCheckerBase::Manager {

		Manager(const Manager &); // UNDEFINED
		Manager &operator=(const Manager &); // UNDEFINED

	public:
		Manager();
		~Manager();

		std::string search(std::string word, bool &found) override;

		bool suggestions(std::string word,
				 std::vector<std::string> &suggestionList,
				 std::string &errmsg) override;

		std::string replace(std::string word, std::string replacement) override;
		std::string addToPersonal(std::string word) override;
		std::string addToSession(std::string word) override;
	};

	SpellChecker(std::string languageArg, std::string encodingArg);
	virtual ~SpellChecker();

	SpellCheckerBase::Manager *getManager(std::string &errmsg) override;
	void setDictionary(std::string) override;
};

#endif
