#
# Copyright 2002, Double Precision Inc.
#
# See COPYING for distribution information.
#

noinst_LIBRARIES=libcurses.a

libcurses_a_SOURCES=mycurses.H curses.C \
	cursesbutton.H cursesbutton.C \
	curseschoicebutton.H curseschoicebutton.C \
	cursescontainer.H cursescontainer.C \
	cursesdialog.H cursesdialog.C \
	cursesfield.H cursesfield.C \
	cursesfilereq.H cursesfilereq.C \
	cursesflowedline.H \
	curseskeyhandler.H curseskeyhandler.C \
	curseslabel.H curseslabel.C \
	cursesmainscreen.H cursesmainscreen.C \
	cursesmultilinelabel.H cursesmultilinelabel.C \
	cursesmoronize.H cursesmoronize.C \
	cursesobject.H cursesobject.C \
	cursesscreen.H cursesscreen.C \
	cursesstatusbar.H cursesstatusbar.C \
	cursestitlebar.H cursestitlebar.C \
	cursesvscroll.H cursesvscroll.C \
	timer.C timer.H \
	widechar.C widechar.H

noinst_DATA=curseslib

DISTCLEANFILES=curseslib

curseslib: config.status
	echo @CURSESLIB@ >curseslib
