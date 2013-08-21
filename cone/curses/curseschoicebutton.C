/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "curses_config.h"
#include "curseschoicebutton.H"

using namespace std;

CursesChoiceButton::CursesChoiceButton(CursesContainer *parent)
	: CursesButton(parent, ""),
	  selectedOption(0)
{
}

void CursesChoiceButton::setOptions(const vector<string> &optionListArg,
				    size_t initialOption)
{
	optionList=optionListArg;
	selectedOption=initialOption;
	setText(optionListArg[selectedOption]);
}

CursesChoiceButton::~CursesChoiceButton()
{
}

void CursesChoiceButton::clicked()
{
	if (optionList.size() > 0)
	{
		selectedOption = (selectedOption+1) % optionList.size();
		setText(optionList[selectedOption]);
	}
}

