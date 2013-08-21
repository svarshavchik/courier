#ifndef spellchecker_H
#define spellchecker_H

#include "config.h"

#if USE_ASPELL
#include "spellcheckerAspell.H"
#else
#if USE_PSPELL
#include "spellcheckerPspell.H"
#else
#include "spellcheckerNone.H"
#endif
#endif

#endif
