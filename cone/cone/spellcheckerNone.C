/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "spellcheckerNone.H"
#include "gettext.H"

using namespace std;

static string NOTAVAILABLE()
{
	return _("Spell checking disabled");
}

SpellChecker::SpellChecker(string l, string e)
{
}

SpellChecker::~SpellChecker()
{
}

SpellCheckerBase::Manager *SpellChecker::getManager(std::string &errmsg)
{
	errmsg=NOTAVAILABLE();
	return NULL;
}

SpellChecker::Manager::Manager()
{
}

SpellChecker::Manager::~Manager()
{
}

string SpellChecker::Manager::search(string word, bool &found)
{
	return NOTAVAILABLE();
}

bool SpellChecker::Manager::suggestions(string word, vector<string> &list,
					string &errmsg)
{
	errmsg=NOTAVAILABLE();
	return false;
}

string SpellChecker::Manager::replace(string word, string repl)
{
	return NOTAVAILABLE();
}

string SpellChecker::Manager::addToPersonal(string word)
{
	return NOTAVAILABLE();
}


string SpellChecker::Manager::addToSession(string word)
{
	return NOTAVAILABLE();
}

void SpellChecker::setDictionary(std::string)
{
}
