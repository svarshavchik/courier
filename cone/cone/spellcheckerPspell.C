/*
** Copyright 2003-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "spellcheckerPspell.H"
#include "libmail/mail.H"
#include <errno.h>
#include <cstring>

using namespace std;

SpellChecker::SpellChecker(string languageArg,
			   string encodingArg)
	: language(languageArg),
	  encoding(encodingArg),
	  configClass(new_pspell_config())
{
	if (configClass)
	{
		if (language.size() > 0)
		{
			pspell_config_replace(configClass, 
					      "language-tag", language.c_str()
					      );
		}

		if (encoding.size() > 0)
			pspell_config_replace(configClass,
					      "encoding", encoding.c_str());
	}
}

SpellChecker::~SpellChecker()
{
	if (configClass)
		delete_pspell_config(configClass);
}

void SpellChecker::setDictionary(std::string dictionaryNameArg)
{
	dictionaryName=dictionaryNameArg;

	pspell_config_replace(configClass, "master", dictionaryName.c_str());
}

SpellChecker::Manager::Manager(PspellManager *spellerArg,
			       PspellCanHaveError *speller_errorArg)
	: speller(spellerArg), speller_error(speller_errorArg)
{
}

SpellChecker::Manager::~Manager()
{
	if (speller)
		delete_pspell_manager(speller);
}

SpellCheckerBase::Manager *SpellChecker::getManager(string &errmsg)
{
	if (!configClass)
	{
		errmsg=strerror(ENOMEM);
		return NULL;
	}

	PspellCanHaveError *manager=new_pspell_manager(configClass);

	if (!manager)
	{
		errmsg=strerror(errno);
		return NULL;
	}

	if (pspell_error_number(manager))
	{
		errmsg=pspell_error_message(manager);

		delete_pspell_can_have_error(manager);
		return NULL;
	}

	PspellManager *speller=to_pspell_manager(manager);

	Manager *m=new Manager(speller, manager);

	if (!m)
	{
		delete_pspell_manager(speller);
		errmsg=strerror(errno);
		return NULL;
	}

	return m;
}

string SpellChecker::Manager::search(string word, bool &found)
{
	int rc=pspell_manager_check(speller, word.c_str(), word.size());

	found= rc != 0;

	return "";
}

bool SpellChecker::Manager::suggestions(string word,
					vector<string>
					&suggestionList,
					string &errmsg)
{
	const PspellWordList *wordList=pspell_manager_suggest(speller,
							      word.c_str(),
							      word.size());

	if (!wordList)
	{
		errmsg=strerror(errno);
		return false;
	}

	PspellStringEmulation *elements=pspell_word_list_elements(wordList);

	if (!elements)
	{
		errmsg=strerror(errno);
		return false;
	}

	try {
		const char *word;

		while ((word=pspell_string_emulation_next(elements)) != NULL)
			suggestionList.push_back(string(word));

		delete_pspell_string_emulation(elements);
	} catch (...) {
		delete_pspell_string_emulation(elements);
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	return true;
}

string SpellChecker::Manager::replace(std::string word,
				    std::string replacement)
{
	if (!pspell_manager_store_replacement(speller, word.c_str(),
					      word.size(),
					      replacement.c_str(),
					      replacement.size()))
		return pspell_error_message(speller_error);

	return "";
}

string SpellChecker::Manager::addToPersonal(string word)
{
	if (!pspell_manager_add_to_personal(speller, word.c_str(),
					    word.size()))
		return pspell_error_message(speller_error);
	return "";
}

string SpellChecker::Manager::addToSession(string word)
{
	if (!pspell_manager_add_to_session(speller, word.c_str(), word.size()))
		return pspell_error_message(speller_error);
	return "";
}
