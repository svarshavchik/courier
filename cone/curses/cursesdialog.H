/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef cursesdialog_H
#define cursesdialog_H

#include "../curses/curses_config.h"
#include "cursescontainer.H"

#include <vector>

////////////////////////////////////////////////////////////////////////////
//
// A helper container that formats dialog screens.
//
// The CursesDialog contains a list of fields, and their corresponding labels.
// The labels are right aligned.
//

class CursesLabel;
class CursesField;

class CursesDialog : public CursesContainer {

	std::vector< std::pair<CursesLabel *, Curses *> > prompts;

	int max_label_width;
	int max_field_width;

	// To prevent a lot of extra work, quietly eat all WriteText()s
	// until the dialog is complete.
	int draw_flag;

public:
	CursesDialog(CursesContainer *parent);
	~CursesDialog();

	// Calculate max size.

	int getWidth() const;
	int getHeight() const;

	void draw();

	// Add a new input field, and its corresponding prompt
	// (either one may be NULL, both can be NULL to append some useful
	// filler).

	virtual void addPrompt(CursesLabel *label,
			       Curses *field);
	virtual void addPrompt(CursesLabel *label,
			       Curses *field,
			       size_t atRow);
	void delPrompt(CursesLabel *label);
	void delPrompt(Curses *field);

private:
	void delPrompt(std::vector< std::pair<CursesLabel *, Curses *> >::iterator p,
		       int row);
public:

	void deleteChild(Curses *child);

	bool writeText(const char *text, int row, int col,
			       const CursesAttr &attr) const;
	bool writeText(const std::vector<unicode_char> &text, int row, int col,
		       const Curses::CursesAttr &attr) const;

};

#endif
