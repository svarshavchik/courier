/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef cursesaddresslist_H
#define cursesaddresslist_H

#include "config.h"

/////////////////////////////////////////////////////////////////////////
//
// RFC 2822 address list entry
//
// This is an object derived from a CursesContainer object from our
// libcurses.  It contains a variable number of AddressField objects,
// which are derived from CursesField, and normally acts like any
// ordinary curses entry field.
//
// When curses input goes into an AddressField, everything acts as an
// ordinary text input field.  Outside of a text input field, each
// AddressField contains exactly one E-mail address.
// When AddressField loses input focus, its contents are parsed.
// If the AddressField is empty, or contains more than one address
// the list of AddressField objects is appropriately repopulated to
// preserve the one address per AddressField paradigm.

#include <vector>

#include "curses/mycurses.H"
#include "curses/cursescontainer.H"
#include "libmail/rfcaddr.H"

class CursesAddressList : public CursesContainer {

	class AddressField;

	std::vector<AddressField *> fields;

	void clear(bool);

	bool validate(size_t rowNum, std::string text);
	bool validate(size_t rowNum, std::vector<mail::emailAddress> &);
	int width;

public:

	void addressFocusLost(size_t rowNum); // Callback from AddressField

	void setRow(int); // Override from CursesContainer
	void setCol(int); // Override from CursesContainer

	CursesAddressList(CursesContainer *parent);
	~CursesAddressList();

	void resized();	// Resize addressfields accordingly.
	void setWidth(int w); // Resize addressfields accordingly.
	int getWidth() const;
	void requestFocus(); // Focus to first field
	bool isFocusable(); // Yup

	bool getAddresses( std::vector<mail::emailAddress> &addressArray );
	void setAddresses( std::vector<mail::emailAddress> &addressArray );

	bool validateAll();

	virtual bool addressLookup(std::vector<mail::emailAddress> &,
				   std::vector<mail::emailAddress> &)=0;

	// CursesEdit::AddressList subclasses this, and hits the address book.
};

#endif
