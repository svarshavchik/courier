/*
** Copyright 2003-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "spellcheckerAspell.H"
#include "libmail/mail.H"
#include <errno.h>
#include <cstring>

#if HAVE_ASPELL_CONFIG_HH
#include "aspell/suggest.hh"
#endif

using namespace std;

SpellChecker::SpellChecker(string languageArg, string encodingArg)
	: language(languageArg),
	  encoding(encodingArg)
#if HAVE_ASPELL_CONFIG_HH

#else
	, config(NULL)
#endif
{
	c_errmsg="";
	try {

#if HAVE_ASPELL_CONFIG_HH

		if (language.size() > 0)
			aspell::Config::replace("language-tag",
						language.c_str());
		if (encoding.size() > 0)
			aspell::Config::replace("charset",
						encoding.c_str());
#else
		config=new_aspell_config();

		if (!config)
		{
			c_errmsg=strerror(errno);
		}
		else
		{
			if (language.size() > 0)
				aspell_config_replace(config, "lang",
						      language.c_str());
			if (encoding.size() > 0)
				aspell_config_replace(config, "encoding",
						      encoding.c_str());
		}
#endif
	} catch ( std::exception &e)
	{
		c_errmsg=e.what();
	}
}

void SpellChecker::setDictionary(std::string dictionaryNameArg)
{
	dictionaryName=dictionaryNameArg;

#if HAVE_ASPELL_CONFIG_HH
	aspell::Config::replace("master", dictionaryName.c_str());
#else
	if (config)
		aspell_config_replace(config, "master", dictionaryName.c_str());
#endif

}

SpellChecker::~SpellChecker()
{
#if HAVE_ASPELL_CONFIG_HH

#else
	if (config)
		delete_aspell_config(config);
#endif
}

#if HAVE_ASPELL_CONFIG_HH
SpellChecker::Manager::Manager(aspell::Config &spellerArg)
	: aspell::Manager(spellerArg)
{
}

SpellChecker::Manager::~Manager()
{
}
#else

SpellChecker::Manager::Manager(AspellSpeller *spellerArg)
	: speller(spellerArg)
{
}


SpellChecker::Manager::~Manager()
{
	delete_aspell_speller(speller);
}
#endif

SpellCheckerBase::Manager *SpellChecker::getManager(string &errmsg)
{
	if (c_errmsg.size() > 0)
	{
		errmsg=c_errmsg;
		return NULL;
	}
#if HAVE_ASPELL_CONFIG_HH

	SpellChecker::Manager *manager;

	try {
		manager=new SpellChecker::Manager(*this);

	} catch ( std::exception &e)
	{
		errmsg=e.what();
		manager=NULL;
	}

	return manager;

#else

	if (config == NULL)
	{
		errmsg=strerror(errno);
		return NULL;
	}

	AspellCanHaveError *manager=new_aspell_speller(config);

	if (!manager)
	{
		errmsg=strerror(errno);
		return NULL;
	}

	if (aspell_error_number(manager))
	{
		errmsg=aspell_error_message(manager);
		delete_aspell_can_have_error(manager);
		return NULL;
	}

	AspellSpeller *speller=to_aspell_speller(manager);

	Manager *m=new Manager(speller);

	if (!m)
	{
		delete_aspell_speller(speller);
		errmsg=strerror(errno);
		return NULL;
	}

	return m;
#endif

}

string SpellChecker::Manager::search(string word, bool &found)
{
	try {

#if HAVE_ASPELL_CONFIG_HH
		found=aspell::Manager::check(word);
#else
		found=aspell_speller_check(speller, word.c_str(), word.size())
			!= 0;
#endif
	} catch (std::exception &e)
	{
		return e.what();
	}

	return "";
}

bool SpellChecker::Manager::suggestions(string word,
					vector<string>
					&suggestionList,
					string &errmsg)
{
#if HAVE_ASPELL_CONFIG_HH

	aspell::SuggestionList &s=aspell::Manager::suggest(word);

	aspell::SuggestionList::VirEmul *elements=s.elements();

	if (!elements)
	{
		errmsg=strerror(errno);
		return false;
	}

	try {
		const char *word;

		while ((word=elements->next()) != NULL)
			suggestionList.push_back(string(word));

		delete elements;
	} catch (...) {
		delete elements;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
#else

	const AspellWordList *s=aspell_speller_suggest(speller, word.c_str(),
						       word.size());

	if (!s)
	{
		errmsg=strerror(errno);
		return false;
	}

	AspellStringEnumeration *elements=aspell_word_list_elements(s);

	try {
		const char *word;

		while ((word=aspell_string_enumeration_next(elements)) != NULL)
			suggestionList.push_back(string(word));

		delete_aspell_string_enumeration(elements);
	}
	catch (...) {
		delete_aspell_string_enumeration(elements);
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

#endif

	return true;
}

string SpellChecker::Manager::replace(std::string word,
				      std::string replacement)
{
#if HAVE_ASPELL_CONFIG_HH
	store_repl(word, replacement);
#else
	aspell_speller_store_replacement(speller, word.c_str(), word.size(),
					 replacement.c_str(),
					 replacement.size());
#endif

	return "";
}

string SpellChecker::Manager::addToPersonal(string word)
{
#if HAVE_ASPELL_CONFIG_HH
	add_to_personal(word);
#else
	aspell_speller_add_to_personal(speller, word.c_str(), word.size());
#endif
	return "";
}

string SpellChecker::Manager::addToSession(string word)
{
#if HAVE_ASPELL_CONFIG_HH
	add_to_session(word.c_str());
#else
	aspell_speller_add_to_session(speller, word.c_str(), word.size());
#endif
	return "";
}
