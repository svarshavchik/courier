/*
** Copyright 2003-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "outputdialog.H"
#include "gettext.H"
#include "myserver.H"

OutputDialog::OutputDialog(CursesMainScreen *parent)
	: CursesContainer(parent),
	  okButton(NULL, _("Press ENTER to continue")), partialLine(false)
{
	requestFocus();

	okButton=this;
	okButton= &OutputDialog::close;
}

OutputDialog::~OutputDialog()
{
}

int OutputDialog::getWidth() const
{
	return getParent()->getWidth();
}

int OutputDialog::getHeight() const
{
	return getParent()->getHeight();
}

bool OutputDialog::isDialog() const
{
	return true;
}

bool OutputDialog::processKeyInFocus(const Key &key)
{
	return true; // Eat all keyboard input.
}

// Display progressive output, as it comes in.

void OutputDialog::output(std::string s)
{
	// Previous output ended in a partial line?
	// Prepend the partial line to s, then erase the last line, and
	// resume course.

	if (partialLine && buf.size() > 0)
	{
		s=buf.end()[-1]+s;

		std::vector<unicode_char> blankLine;

		blankLine.insert(blankLine.end(), getWidth(), ' ');

		CursesAttr attr;
		writeText(blankLine, buf.size()-1, 0, attr);
		buf.erase(buf.end()-1, buf.end());
	}
	partialLine=false;

	size_t n;

	while ((n=s.find('\n')) != std::string::npos)
	{
		std::string l=s.substr(0, n);
		s=s.substr(n+1);
		outputLine(l);
	}

	if (s.size() > 0)
	{
		partialLine=true;
		outputLine(s);
	}
}

void OutputDialog::outputLine(std::string s)
{
	CursesAttr attr;

	std::string::iterator b=s.begin(), p=b, e=s.end();

	while (b != e)
	{
		if ((int)(unsigned char)*b < ' ')
		{
			++b;
			continue;
		}

		*p++ = *b++;
	}

	s.erase(p, e);


	std::vector<std::string> addStrings;

	{
		std::vector< std::vector<unicode_char> > ulines;
		std::back_insert_iterator
			<std::vector< std::vector<unicode_char>
				      > > insert_iter(ulines);

		std::vector<unicode_char> uline;

		mail::iconvert::convert(s,
					unicode_default_chset(),
					uline);

		unicodewordwrap(uline.begin(), uline.end(),
				unicoderewrapnone(),
				insert_iter, getWidth(), true);

		for (std::vector<std::vector<unicode_char> >::iterator
			     b=ulines.begin(),
			     e=ulines.end(); b != e; ++b)
		{
			addStrings.push_back(mail::iconvert::convert(*b,
								     unicode_default_chset()));
		}

	}

	size_t h=getHeight();

	if (h > 3)
		h -= 2;
	else
		h=1;

	if (addStrings.size() > h)
		addStrings.erase(addStrings.begin(),
				 addStrings.end() - h);

	if (addStrings.size() + buf.size() > h)
	{
		buf.insert(buf.end(), addStrings.begin(), addStrings.end());
		buf.erase(buf.begin(), buf.end()-h);
		erase();
		draw();
		return;
	}

	buf.insert(buf.end(), addStrings.begin(), addStrings.end());

	size_t i;

	for (i=0; i<addStrings.size(); i++)
		writeText((buf.end()-addStrings.size()+i)->c_str(),
			  buf.size()-addStrings.size()+i, 0, attr);
}

void OutputDialog::draw()
{
	size_t i;
	CursesAttr attr;

	for (i=0; i<buf.size(); i++)
		writeText(buf[i].c_str(), i, 0, attr);
	CursesContainer::draw();
}

void OutputDialog::outputDone()
{
	output("\n\n");
	okButton.setRow(buf.size() + 1);

	addChild(&okButton);
	okButton.requestFocus();
}

void OutputDialog::close()
{
	myServer::nextScreen=NULL;
	Curses::keepgoing=false;
}
