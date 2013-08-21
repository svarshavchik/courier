#include "config.h"
#include "curses/cursesfield.H"
#include "curses/cursesstatusbar.H"
#include "cursesaddresslist.H"
#include "gettext.H"
#include "libmail/mail.H"
#include "libmail/rfcaddr.H"
#include "rfc822/rfc822.h"
#include <unistd.h>
#include <errno.h>

using namespace std;

extern CursesStatusBar *statusBar;

//////////////////////////////////////////////////////////////////////////


class CursesAddressList::AddressField : public CursesField {

public:
	CursesAddressList *parent;
	size_t rowNum;

	AddressField(CursesAddressList *parentArg,
		     size_t currentWidth,
		     size_t rowNumArg);
	~AddressField();

	void focusLost();

	bool processKeyInFocus(const Key &key);
};

CursesAddressList::AddressField::AddressField(CursesAddressList *parentArg,
					      size_t currentWidth,
					      size_t rowNumArg)
	: CursesField(parentArg, currentWidth),
	  parent(parentArg),
	  rowNum(rowNumArg)
{
	setUnderlined(false);
}

CursesAddressList::AddressField::~AddressField()
{
}

void CursesAddressList::AddressField::focusLost()
{
	parent->addressFocusLost(rowNum);
}

bool CursesAddressList::AddressField::processKeyInFocus(const Key &key)
{
	return CursesField::processKeyInFocus(key);
}

///////////////////////////////////////////////////////////////////////////

CursesAddressList::CursesAddressList(CursesContainer *parent)
	: CursesContainer(parent), width(parent->getWidth())
{
}

void CursesAddressList::setWidth(int w)
{
	if (w > 0)
	{
		width=w;

		// All AddressFields have the same width

		vector<AddressField *>::iterator b, e;

		b=fields.begin();
		e=fields.end();

		while (b != e)
		{
			AddressField *f= *b++;

			if (f)
				f->setWidth(w);
		}
	}
}

int CursesAddressList::getWidth() const
{
	return width;
}

CursesAddressList::~CursesAddressList()
{
	clear(false);
}

void CursesAddressList::clear(bool doErase)
{
	if (doErase) // Also erase visually.
	{
		vector<AddressField *>::iterator b=fields.begin(),
			e=fields.end();

		while (b != e)
			(*b++)->erase();
	}

	vector<AddressField *>::iterator b=fields.begin(), e=fields.end();

	while (b != e)
	{
		AddressField *p= *b;

		*b++=NULL;

		if (p)
			delete p;
	}
	fields.clear();
}

void CursesAddressList::addressFocusLost(size_t rowNum)
{
	validate(rowNum, fields[rowNum]->getText());
}

// Validate new contents of one of the AddressFields.

bool CursesAddressList::validate(size_t rowNum,  // Changed AddressField
				 string addresses) // What it has
{
	size_t errIndex;

	vector<mail::emailAddress> addrvecNative;

	{
		vector<mail::address> addrvec;

		if (!mail::address::fromString(addresses, addrvec, errIndex))
		{
			statusBar->clearstatus();
			statusBar->status(strerror(errno));
			statusBar->beepError();
			return false;
		}

		if (errIndex <= addresses.size())
		{
			// Figure out the cursor position

			string s=addresses.substr(0, errIndex);

			widecharbuf wc;

			wc.init_string(s);

			fields[rowNum]->requestFocus();
			fields[rowNum]->setCursorPos(wc.wcwidth(0));
			statusBar->clearstatus();
			statusBar->status(_("Syntax error"));
			statusBar->beepError();
			return false;
		}


		/*
		** We obtained the addresses in their native form, so convert
		** them properly.
		*/

		addrvecNative.reserve(addrvec.size());

		vector<mail::address>::iterator b, e;

		for (b=addrvec.begin(), e=addrvec.end(); b != e; ++b)
		{
			mail::emailAddress newAddress;

			std::string errmsg=newAddress
				.setDisplayName(b->getName(),
						unicode_default_chset());

			if (errmsg == "")
				errmsg=newAddress
					.setDisplayAddr(b->getAddr(),
							unicode_default_chset()
							);

			if (errmsg != "")
			{
				statusBar->clearstatus();
				statusBar->status(errmsg);
				statusBar->beepError();
				return false;
			}

			addrvecNative.push_back(newAddress);
		}
	}

	return validate(rowNum, addrvecNative);
}

bool CursesAddressList::validate(size_t rowNum,  // Changed AddressField
				 vector<mail::emailAddress> &addrvec)
	// What it has
{

	if (rowNum >= fields.size())
		return true;

	{
		vector<mail::emailAddress> newAddrVec;

		if (addressLookup(addrvec, newAddrVec))
			addrvec=newAddrVec;
	}

	if (addrvec.size() == 0) // This field is empty.
	{
		if (fields.size() <= 1) // Leave one field standing.
			return true;

		// Adjust the array.

		fields[rowNum]->erase();
		delete fields[rowNum];

		fields[rowNum]=NULL;

		fields.erase(fields.begin() + rowNum);

		// Repoint the remaining fields.

		size_t i;

		for (i=rowNum; i<fields.size(); i++)
			if (fields[i])
				fields[i]->rowNum=i;

		for (i=rowNum; i<fields.size(); i++)
			if (fields[i])
				fields[i]->setRow(i);

		// Make sure everything but the last field has a trailing
		// comma.

		i=fields.size()-1;

		string s=fields[i]->getText();

		if (s.size() > 0 && s[s.size()-1] == ',')
			fields[i]->setText(s.substr(0, s.size()-1));

		resized();
		return true;
	}

	if (addrvec.size() > 1)
	{
		// If there's more than one address, insert more fields.

		fields.insert(fields.begin() + rowNum + 1,
			      addrvec.size()-1, NULL);

		size_t i=1;

		try {
			while (i < addrvec.size())
			{
				if ((fields[i+rowNum]=
				     new AddressField(this,
						      getWidth(),
						      i + rowNum))  == NULL)
					outofmemory();
				fields[i+rowNum]->setRow(i+rowNum);
				i++;
			}
		} catch (...) {

			while (i)
			{
				--i;
				delete fields[i];
				fields[i]=NULL;
			}
			fields.erase(fields.begin() + rowNum+1,
				     fields.begin() + rowNum + addrvec.size());
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}

		for (i=fields.size(); i > rowNum + addrvec.size(); )
		{
			--i;
			if (fields[i])
				fields[i]->setRow(i);
		}
		resized();
	}

	// Common code to repopulate the changed AddressFields.

	vector<mail::emailAddress>::iterator b, e;
	b=addrvec.begin();
	e=addrvec.end();

	while (b != e)
	{
		// We want toString() to use the encoded form.

		string s= mail::address(b->getDisplayName(unicode_default_chset()),
					b->getDisplayAddr(unicode_default_chset())).toString();

		if (rowNum + 1 < fields.size())
			s += ","; // All but last addy has a trailing comma

		if (fields[rowNum])
			fields[rowNum++]->setText(s);


		++b;
	}
	return true;
}

void CursesAddressList::resized()
{
	CursesContainer::resized();

	int w=getWidth();

	vector<AddressField *>::iterator b=fields.begin(), e=fields.end();

	while (b != e)
	{
		if (*b)
			(*b)->setWidth(w);
		b++;
	}
}

void CursesAddressList::requestFocus()
{
	if (fields.size() > 0 && fields[0])
		fields[0]->requestFocus();
}

bool CursesAddressList::isFocusable()
{
	return false; // Child processes are focusable, really.

	//	return fields.size() > 0 && fields[0];
}

bool CursesAddressList::validateAll()
{
	size_t i;

	for (i= fields.size(); i > 0; )
	{
		--i;
		if (fields[i])
			if (!validate(i, fields[i]->getText()))
			{
				fields[i]->requestFocus();
				return false;
			}
	}
	return true;
}

bool CursesAddressList::getAddresses( vector<mail::emailAddress> &addressArray)
{
	size_t i;


	string s="";

	for (i=0; i<fields.size(); i++)
		if (fields[i])
			s += fields[i]->getText();

	size_t dummy;

	vector<mail::address> addrBuf;

	if (!mail::address::fromString(s, addrBuf, dummy))
		return false;

	/*
	** Addresses are shown in their native form.  Populate addressArray
	** using their encoded form.
	*/

	addressArray.reserve(addressArray.size() + addrBuf.size());

	vector<mail::address>::iterator b, e;

	for (b=addrBuf.begin(), e=addrBuf.end(); b != e; ++b)
	{
		mail::emailAddress addr;

		addr.setDisplayName(b->getName(), unicode_default_chset());
		addr.setDisplayAddr(b->getAddr(), unicode_default_chset());

		addressArray.push_back(addr);
	}
	return true;
}

void CursesAddressList::setAddresses( vector<mail::emailAddress> &addressArray)
{
	clear(true);

	AddressField *p=new AddressField(this, getWidth(), 0);

	if (!p)
		outofmemory();

	try {
		fields.push_back(p);
	} catch (...) {
		delete p;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
	resized();

	validate(0, addressArray);
}

void CursesAddressList::setRow(int r)
{
	erase();
	CursesContainer::setRow(r);
	draw();
}

void CursesAddressList::setCol(int c)
{
	erase();
	CursesContainer::setCol(c);
	draw();
}
