/*
** Copyright 2003, Double Precision Inc.
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

		std::string search(std::string word, bool &found);

		bool suggestions(std::string word,
				 std::vector<std::string> &suggestionList,
				 std::string &errmsg);

		std::string replace(std::string word, std::string replacement);
		std::string addToPersonal(std::string word);
		std::string addToSession(std::string word);
	};

	SpellChecker(std::string languageArg, std::string encodingArg);
	virtual ~SpellChecker();

	SpellCheckerBase::Manager *getManager(std::string &errmsg);
	void setDictionary(std::string);
};

#endif
