/*
** Copyright 2023, S. Varshavchik.
**
** See COPYING for distribution information.
*/

#ifndef spellcheckerHunspell_H
#define spellcheckerHunspell_H

#include <string>
#include <vector>

#include "config.h"

#include "spellcheckerbase.H"

#if USE_HUNSPELL

#include <string>
#include <vector>

class SpellChecker : SpellCheckerBase {

	struct pipe_guard;

public:

	class Manager : public SpellCheckerBase::Manager {

		FILE *stdin_fp;
		FILE *stdout_fp;

		std::string lastword;
		bool lastword_good=true;
		std::vector<std::string> hints;
	public:
		Manager(FILE *stdin_fp,
			FILE *stdout_fp);
		~Manager();

		std::string search(std::string, bool &) override;
		bool suggestions(std::string,
				 std::vector<std::string> &,
				 std::string &errmsg) override;

		std::string replace(std::string, std::string) override;

		std::string addToPersonal(std::string) override;
		std::string addToSession(std::string) override;
	};

	SpellChecker(const SpellChecker &)=delete;
	SpellChecker &operator=(const SpellChecker &)=delete;


	SpellChecker(std::string languageArg,
		     std::string encodingArg);

	~SpellChecker();

	void setDictionary(std::string) override;

	SpellCheckerBase::Manager *getManager(std::string &errmsg) override;
};
#endif

#endif
