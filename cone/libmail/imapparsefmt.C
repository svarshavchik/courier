/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "imap.H"
#include "imapparsefmt.H"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

using namespace std;

/////////////////////////////////////////////////////////////////////////
//
// Parse a structured IMAP reply

mail::imapparsefmt::imapparsefmt()
	: current(NULL), nil(false), value(""), parent(NULL)
{
}

mail::imapparsefmt::imapparsefmt(mail::imapparsefmt *parentArg)
	: current(NULL), nil(false), value(""), parent(parentArg)
{
	parent->children.push_back(this);
}

mail::imapparsefmt::imapparsefmt(const mail::imapparsefmt &cpy)
	: current(NULL), nil(false), value(""), parent(NULL)
{
	try {
		(*this)=cpy;
	} catch (...)
	{
		destroy();
		children.clear();
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

mail::imapparsefmt::~imapparsefmt()
{
	destroy();
}

mail::imapparsefmt &mail::imapparsefmt
::operator=(const mail::imapparsefmt &cpy)
{
	destroy();
	children.clear();

	current=cpy.current;
	nil=cpy.nil;
	value=cpy.value;
	parent=cpy.parent;

	vector<mail::imapparsefmt *>::const_iterator b=cpy.children.begin(),
		e=cpy.children.end();

	while (b != e)
	{
		mail::imapparsefmt *ptr=new mail::imapparsefmt(**b);

		if (!ptr)
			LIBMAIL_THROW("Out of memory.");

		try {
			ptr->parent=this;
			children.push_back(ptr);
		} catch (...)
		{
			delete ptr;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}
		b++;
	}
	return *this;
}

void mail::imapparsefmt::destroy()
{
	vector<mail::imapparsefmt *>::iterator b=children.begin(),
		e=children.end();

	while (b != e)
	{
		delete *b;
		b++;
	}
}

bool mail::imapparsefmt::process(mail::imap &imapAccount,
				 mail::imapHandlerStructured::Token &t,
				 bool &parse_error)
{
	parse_error=false;
	if (current == NULL)	// Start of tree.
	{
		if (t == mail::imapHandlerStructured::NIL)
		{
			nil=1;
			return true;
		}

		if (t == mail::imapHandlerStructured::ATOM ||
		    t == mail::imapHandlerStructured::STRING)
		{
			value=t.text;
			return true;
		}
		if (t != '(')
		{
			parse_error=true;
			return true;
		}

		current=this;
		return false;
	}

	if (t == ')')
	{
		current=current->parent;
		return (current == NULL);
	}

	mail::imapparsefmt *child=new mail::imapparsefmt(current);

	if (t == mail::imapHandlerStructured::NIL)
	{
		child->nil=1;
		return false;
	}

	if (t == mail::imapHandlerStructured::ATOM ||
	    t == mail::imapHandlerStructured::STRING)
	{
		child->value=t.text;
		return false;
	}

	if (t != '(')
	{
		parse_error=1;
		return true;
	}

	current=child;
	return false;
}
