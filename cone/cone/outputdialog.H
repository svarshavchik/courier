/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef outputdialog_h
#define outputdialog_h

#include "config.h"
#include "curses/cursescontainer.H"
#include "curses/cursesbutton.H"
#include "curses/cursesmainscreen.H"

//////////////////////////////////////////////////////////////////////////
//
// A dialog box that shows captured output of a subprocess.  Currently
// used to provide play-by-play of gpg --gen-key
//

class OutputDialog : public CursesContainer {

	std::vector<std::string> buf;

	CursesButtonRedirect<OutputDialog> okButton;

	void close();
	void draw();

public:
	OutputDialog(CursesMainScreen *parent);
	~OutputDialog();

	int getWidth() const;
	int getHeight() const;
	bool isDialog() const;
	bool processKeyInFocus(const Key &key);

	void output(std::string);
private:
	bool partialLine;
	
	void outputLine(std::string);
public:
	void outputDone();
};

#endif
