#ifndef init_H
#define init_H

#include "curses/cursesscreen.H"
#include "curses/cursestitlebar.H"
#include "curses/cursesstatusbar.H"
#include "curses/cursesmainscreen.H"

#include "spellcheckerbase.H"

extern CursesScreen *cursesScreen;
extern CursesMainScreen *mainScreen;
extern CursesStatusBar *statusBar;
extern CursesTitleBar *titleBar;
extern SpellCheckerBase *spellCheckerBase;

extern void init();

#endif
