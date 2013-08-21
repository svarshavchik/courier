/*
** Copyright 2003-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "gpg.H"
#include "gpglib/gpglib.h"
#include "gpglib/gpg.h"
#include "curses/cursescontainer.H"
#include "curses/curseskeyhandler.H"
#include "curses/curseslabel.H"
#include "curses/cursesbutton.H"
#include "curses/cursesvscroll.H"
#include "curses/cursesmainscreen.H"
#include "init.H"
#include <unistd.h>
#include "gettext.H"
#include "myserver.H"
#include <vector>
#include <set>
#include <algorithm>
#include <errno.h>

extern char ucheck[];

const char *gpgprog()
{
	return GPG;
}
#undef GPG

extern Gettext::Key key_TOGGLESPACE;
extern Gettext::Key key_ABORT;

GPG GPG::gpg;

GPG::Key::Key(std::string f, std::string s, std::string l, bool i)
	: fingerprint(f), shortname(s), longname(l), invalid(i)
{
	size_t dummy;

	mail::address::fromString(s, address, dummy);
}

GPG::Key::~Key()
{
}

void GPG::Key::getDescription(std::vector<std::string> &descrArray,
			      size_t width)
{
	std::string keyDescr=longname;

	do
	{
		std::vector<unicode_char> unicode_buf;

		size_t p=keyDescr.find('\n');

		if (p == std::string::npos)
		{
			mail::iconvert::convert(keyDescr,
						unicode_default_chset(),
						unicode_buf);
			keyDescr="";
		}
		else
		{
			mail::iconvert::convert(keyDescr.substr(0, p),
						unicode_default_chset(),
						unicode_buf);
			keyDescr=keyDescr.substr(p+1);
		}

		std::vector< std::vector<unicode_char> > wrapped_text;

		std::back_insert_iterator<std::vector< std::vector
						       <unicode_char> > >
			insert_iter(wrapped_text);

		unicodewordwrap(unicode_buf.begin(),
				unicode_buf.end(),
				unicoderewrapnone(),
				insert_iter,
				width,
				true);

		for (std::vector< std::vector<unicode_char> >::const_iterator
			     b(wrapped_text.begin()),
			     e(wrapped_text.end()); b != e; ++b)
		{
			descrArray.push_back(mail::iconvert
					     ::convert(*b,
						       unicode_default_chset())
					     );
		}
	} while (keyDescr.size() > 0);
}

// Provide a description of columns in longname

void GPG::Key::getDescriptionColumns(std::vector<std::pair<std::string, int> > &columns)
{
	columns.clear();
	columns.reserve(4);
	columns.push_back(std::make_pair(std::string(_("Key Type        ")), 0));
	columns.push_back(std::make_pair(std::string(_("Created   ")), 18));
	columns.push_back(std::make_pair(std::string(_("Expires   ")), 29));
	columns.push_back(std::make_pair(std::string(_("Description")), 40));
}

GPG::GPG()
{
}

GPG::~GPG()
{
}

bool GPG::gpg_installed()
{
	if (access(gpgprog(), X_OK))
		return false;

	return true;
}

// Initialize the list of available keys.  First, obtain the list
// using libmail_gpg_listkeys, then sort it.

class GPG::initHelper {
public:

	initHelper();
	~initHelper();

	std::vector<GPG::Key> keys;

	// ... Then sort it by address.

	std::vector< std::pair<std::string, GPG::Key *> > sortedKeys;

	class sort {
	public:
		sort();
		~sort();
		bool operator()(const std::pair<std::string, GPG::Key *> &,
				const std::pair<std::string, GPG::Key *> &);
	};

	static int save_key(const char *fingerprint,
			    const char *shortname,
			    const char *longname,
			    int invalid_flag,
			    struct gpg_list_info *va);

	void operator>>( std::vector<Key> &);
};

GPG::initHelper::initHelper()
{
}

GPG::initHelper::~initHelper()
{
}

int GPG::initHelper::save_key(const char *fingerprint,
			      const char *shortname,
			      const char *longname,
			      int invalid_flag,
			      struct gpg_list_info *va)
{
	GPG::initHelper *h=(GPG::initHelper *)va->voidarg;

	h->keys.push_back(GPG::Key(fingerprint,
				   shortname,
				   longname,
				   invalid_flag != 0));
	return 0;
}


GPG::initHelper::sort::sort()
{
}

GPG::initHelper::sort::~sort()
{
}

bool GPG::initHelper::sort::operator()(const std::pair<std::string, GPG::Key *> &a,
				       const std::pair<std::string, GPG::Key *> &b)
{
	if (a.first == b.first)
		return a.second->longname < b.second->longname;

	return a.first < b.first;
}

void GPG::initHelper::operator>>( std::vector<Key> &keyList)
{
	sortedKeys.clear();
	sortedKeys.reserve(keys.size());

	std::vector<GPG::Key>::iterator kb=keys.begin(), ke=keys.end();

	while (kb != ke)
	{
		// Extract the address from longname

		std::vector<mail::address> addrVec;
		size_t dummy;
		mail::address::fromString(kb->longname, addrVec, dummy);

		std::string s;

		if (addrVec.size() > 0)
			s=addrVec[0].getAddr();

		sortedKeys.push_back(std::make_pair(s, &*kb));
		++kb;
	}

	keyList.clear();
	keyList.reserve(keys.size());

	std::sort(sortedKeys.begin(), sortedKeys.end(), sort());

	std::vector< std::pair<std::string, GPG::Key *> >::iterator
		b=sortedKeys.begin(),
		e=sortedKeys.end();

	while (b != e)
	{
		keyList.push_back( *b->second );
		b++;
	}
}

static int save_errmsg(const char *dummy, size_t dummy2, void *va)
{
	return 0;
}

void GPG::init()
{
	if (!gpg_installed())
		return;

	struct gpg_list_info gli;

	memset(&gli, 0, sizeof(gli));
	gli.charset=unicode_default_chset();
	gli.disabled_msg= _(" (disabled)");
	gli.revoked_msg= _(" (removed)");
	gli.expired_msg= _(" (disabled)");
	gli.group_msg= _("Group: @");

	{
		initHelper pub;
		gli.voidarg= &pub;

		libmail_gpg_listkeys(NULL, 0, &initHelper::save_key,
				     save_errmsg, &gli);
		libmail_gpg_listgroups(NULL, &initHelper::save_key,
				       &gli);
		pub >> public_keys;
	}

	{
		initHelper sec;
		gli.voidarg= &sec;

		libmail_gpg_listkeys(NULL, 1, &initHelper::save_key,
				     save_errmsg, &gli);

		sec >> secret_keys;
	}
}


GPG::Key_iterator GPG::get_secret_key(std::string fingerprint)
{
	return get_key(secret_keys, fingerprint);
}

GPG::Key_iterator GPG::get_public_key(std::string fingerprint)
{
	return get_key(public_keys, fingerprint);
}

void GPG::find_secret_keys(std::vector<mail::address> &addresses,
			  std::vector<Key_iterator> &keys)
{
	return find_keys(secret_keys, addresses, keys);
}

void GPG::find_public_keys(std::vector<mail::address> &addresses,
			   std::vector<Key_iterator> &keys)
{
	return find_keys(public_keys, addresses, keys);
}

GPG::Key_iterator GPG::get_key(std::vector<GPG::Key> &keys, std::string fingerprint)
{
	Key_iterator b=keys.begin(), e=keys.end();

	while (b != e)
	{
		if (b->fingerprint == fingerprint)
			break;
		b++;
	}

	return b;
}

void GPG::find_keys(std::vector<Key> &keys, std::vector<mail::address> &address_a,
		   std::vector<Key_iterator> &keys_out)
{
	Key_iterator b=keys.begin(), e=keys.end();

	while (b != e)
	{
		std::vector<mail::address>::iterator ba=address_a.begin(),
			ea=address_a.end();

		while (ba != ea)
		{
			if (ba->getAddr().size() > 0)
			{
				std::vector<mail::address>::iterator
					bk=b->address.begin(),
					ek=b->address.end();

				while (bk != ek)
				{
					if (*bk == *ba)
						break;
					bk++;
				}

				if (bk != ek)
				{
					keys_out.push_back(b);
					break;
				}
			}
			ba++;
		}
		b++;
	}
}

////////////////////////////////////////////////////////////////////////////
//
// The popup dialog.

class GPG::dialog : public CursesContainer, public CursesKeyHandler {

	std::vector<unicode_char> utitle;
	std::vector<GPG::Key> &keys;

	class Keylist : public CursesVScroll {
		GPG::dialog *parent;
		size_t currentRow;

		void drawKey(size_t);

	public:
		bool multiMode;
		std::set<size_t> selectedKeys;

		Keylist(GPG::dialog *parentArg,
			std::string fingerprintArg);
		Keylist(GPG::dialog *parentArg,
			std::vector<std::string> &selectedFingerprints);
		~Keylist();
		// This is a child of CursesFileReq, and its size is
		// automatically extended to the bottom of its parent.
		//  Its width is the same as the parent's width.

		int getHeight() const;
		int getWidth() const;

		bool isFocusable(); // Yes we are.

		void focusGained(); // Move to the first row
		void focusLost();   // Turn off cursor
		void draw();

		// Even though this is a CursesContainer subclass, its focus
		// behavior must be the same as Curses's focus behavior
		// (the default CursesContainer implementation doesn't work,
		// because this object does not have any children).

		Curses *getPrevFocus();
		Curses *getNextFocus();

		int getCursorPosition(int &row, int &col);
		bool processKeyInFocus(const Key &key);

		size_t getCurrentRow() const { return currentRow; }
	};

	Keylist keylist;
	bool closing;
	std::string fingerprint;
	std::string cancelDescr;
	std::string enterDescr;
	void init(std::string);

public:
	dialog(std::vector<GPG::Key> &keys, std::string fingerprintArg,
	       std::string title, std::string cancelDescrArg, std::string enterDescrArg);
	dialog(std::vector<GPG::Key> &keys, std::vector<std::string> &fingerprints,
	       std::string title, std::string cancelDescrArg, std::string enterDescrArg);
	~dialog();
	
	bool orderlyClose() const { return closing; }

	bool isDialog() const;	// Yes we are
	void resized();
	void draw();
	void requestFocus();
	int getWidth() const;
	int getHeight() const;

	void selected(size_t);

	operator std::string()
	{
		return fingerprint;
	}

	void operator>>(std::vector<std::string> &);

private:
	bool processKey(const Curses::Key &key);
	bool listKeys( std::vector< std::pair<std::string, std::string> > &list);
};

GPG::dialog::Keylist::Keylist(GPG::dialog *parentArg,
			      std::string fingerprint)
	: CursesVScroll(parentArg),
	  parent(parentArg), currentRow(0), multiMode(false)
{
	std::vector<GPG::Key>::iterator b=parentArg->keys.begin(),
		e=parentArg->keys.end();
	size_t r=0;

	while (b != e)
	{
		if (b->fingerprint == fingerprint)
			currentRow=r;

		r++;
		b++;
	}
}

GPG::dialog::Keylist::Keylist(GPG::dialog *parentArg,
			      std::vector<std::string> &selectedFingerprints)
	: CursesVScroll(parentArg),
	  parent(parentArg), currentRow(0), multiMode(true)
{
	std::set<std::string> fingerprintSet;

	fingerprintSet.insert(selectedFingerprints.begin(),
			      selectedFingerprints.end());

	size_t n=0;

	for (n=0; n<parentArg->keys.size(); n++)
		if (fingerprintSet.count(parent->keys[n].fingerprint) > 0)
			selectedKeys.insert(n);
}

GPG::dialog::Keylist::~Keylist()
{
}

void GPG::dialog::operator>> (std::vector<std::string> &array)
{
	array.clear();

	std::set<size_t>::iterator b=keylist.selectedKeys.begin(),
		e=keylist.selectedKeys.end();

	while (b != e)
		array.push_back( keys[*b++].fingerprint);

	// If user hit ENTER without specifying a single key, use the one
	// highlighted by the cursor.

	if (array.size() == 0 && fingerprint.size() > 0)
		array.push_back( fingerprint );
}

int GPG::dialog::Keylist::getHeight() const
{
	int h=parent->getHeight();
	int r=getRow();

	return (r < h ? h-r:1);
}

int GPG::dialog::Keylist::getWidth() const
{
	return parent->getWidth();
}

bool GPG::dialog::Keylist::isFocusable()
{
	return true;
}


void GPG::dialog::Keylist::focusGained()
{
	draw();
}

void GPG::dialog::Keylist::focusLost()
{
	draw();
}

void GPG::dialog::Keylist::draw()
{
	size_t n, wh;
	size_t i;

	getVerticalViewport(n, wh);

	for (i=0; i<wh; i++)
		drawKey(i+n);
}

void GPG::dialog::Keylist::drawKey(size_t n)
{
	std::vector<unicode_char> line;

	if (n < parent->keys.size())
	{
		mail::iconvert::convert(parent->keys[n].shortname,
					unicode_default_chset(),
					line);
		line.insert(line.begin(), ' ');
		line.insert(line.begin(), ' ');
	}

	CursesAttr attr;

	if (selectedKeys.count(n) > 0 && line.size() > 0)
	{
		std::vector<unicode_char> uc;

		mail::iconvert::convert(std::string(ucheck),
					unicode_default_chset(), uc);

		if (uc.size() > 0)
		    line[0]=uc[0];

		attr.setHighlight();
	}		    

	if (n == currentRow)
		attr.setReverse();

	{
		widecharbuf wc;

		wc.init_unicode(line.begin(), line.end());

		writeText(wc.get_unicode_fixedwidth(getWidth(), 0),
			  n, 0, attr);
	}
}
 
// Even though this is a CursesContainer subclass, its focus
// behavior must be the same as Curses's focus behavior
// (the default CursesContainer implementation doesn't work,
// because this object does not have any children).
 
Curses *GPG::dialog::Keylist::getPrevFocus()
{
	return Curses::getPrevFocus();
}

Curses *GPG::dialog::Keylist::getNextFocus()
{
	return Curses::getNextFocus();
}

int GPG::dialog::Keylist::getCursorPosition(int &row, int &col)
{
	if (currentRow >= parent->keys.size())
	{
		row=0;
		col=0;
		CursesVScroll::getCursorPosition(row, col);
		return 0;
	}


	row=currentRow;
	col=0;
	CursesVScroll::getCursorPosition(row, col);
	return 0;
}

bool GPG::dialog::Keylist::processKeyInFocus(const Key &key)
{
	if (key == key.ENTER)
	{
		if (currentRow < parent->keys.size())
		{
			parent->selected(currentRow);
			return true;
		}
	}


	if (key == key_TOGGLESPACE)
	{
		if (multiMode)
		{
			if (currentRow < parent->keys.size())
			{
				std::set<size_t>::iterator p=
					selectedKeys.find(currentRow);

				if (p == selectedKeys.end())
					selectedKeys.insert(currentRow);
				else
					selectedKeys.erase(p);
			}
			draw();
			return true;
		}
	}

	if (key == key.UP)
	{
		if (currentRow > 0)
		{
			--currentRow;
			parent->draw();
		}
		return true;
	}

	if (key == key.DOWN)
	{
		if (currentRow + 1 < parent->keys.size())
		{
			++currentRow;
			parent->draw();
		}
		return true;
	}

	if (key == key.PGUP)
	{
		size_t n, wh;

		getVerticalViewport(n, wh);

		if (currentRow > 0)
		{
			currentRow -= currentRow < wh ? currentRow:wh;
			parent->draw();
		}
		return true;
	}

	if (key == key.PGDN)
	{
		size_t n, wh;

		getVerticalViewport(n, wh);

		currentRow += wh;

		if (currentRow >= parent->keys.size())
		{
			currentRow=parent->keys.size();

			if (currentRow)
				--currentRow;
		}
		parent->draw();
		return true;
	}

	return false;
}

GPG::dialog::dialog(std::vector<GPG::Key> &keysArg, std::string fingerprintArg,
		    std::string title, std::string cancelDescrArg, std::string enterDescrArg)
	: CursesContainer(mainScreen), CursesKeyHandler(PRI_DIALOGHANDLER),
	  keys(keysArg),
	  keylist(this, fingerprintArg), closing(false),
	  cancelDescr(cancelDescrArg), enterDescr(enterDescrArg)
{
	init(title);
}

GPG::dialog::dialog(std::vector<GPG::Key> &keysArg, std::vector<std::string> &fingerprints,
		    std::string title, std::string cancelDescrArg, std::string enterDescrArg)
	: CursesContainer(mainScreen), CursesKeyHandler(PRI_DIALOGHANDLER),
	  keys(keysArg),
	  keylist(this, fingerprints), closing(false),
	  cancelDescr(cancelDescrArg), enterDescr(enterDescrArg)
{
	init(title);
}

void GPG::dialog::init(std::string titleArg)
{
	keylist.setRow(11);

	mail::iconvert::convert(titleArg, unicode_default_chset(), utitle);
}

GPG::dialog::~dialog()
{
	if (closing)
		Curses::keepgoing=true;
}

bool GPG::dialog::isDialog() const
{
	return true;
}

void GPG::dialog::resized()
{
	CursesContainer::resized();
}

void GPG::dialog::draw()
{
	CursesContainer::draw();

	size_t r=keylist.getCurrentRow();

	std::string keyDescr;

	std::vector<std::string> rows;

	if (r < keys.size())
		keys[r].getDescription(rows, getWidth());

	Curses::CursesAttr attr;

	std::vector<std::pair<std::string, int> > columns;

	GPG::Key::getDescriptionColumns(columns);

	CursesAttr columnAttr;

	columnAttr.setUnderline();

	std::vector<std::pair<std::string, int> >::iterator
		cb=columns.begin(), ce=columns.end();

	while (cb != ce)
	{
		writeText(cb->first.c_str(), 3, cb->second, columnAttr);
		++cb;
	}

	for (r=0; r<6; r++)
	{
		widecharbuf wc;

		if (r < rows.size())
			wc.init_string(rows[r]);

		writeText(wc.get_unicode_fixedwidth(getWidth(), 0),
			  r+4, 0, attr);
	}

	std::vector<unicode_char> w;

	w.insert(w.end(), getWidth(), '-');
	writeText(w, r+3, 0, attr);

	size_t ww=getWidth();

	{
		widecharbuf wc;

		wc.init_unicode(utitle.begin(), utitle.end());
		wc.expandtabs(0);

		size_t grapheme_width=wc.wcwidth(0);

		if (grapheme_width)
			--grapheme_width;

		ww= grapheme_width > ww ? 0: ww-grapheme_width;

		Curses::CursesAttr wAttr;

		std::vector<unicode_char> s;

		wc.tounicode(s);

		writeText(s, 1, ww/2, wAttr);
	}
}

void GPG::dialog::requestFocus()
{
	keylist.requestFocus();
}

int GPG::dialog::getWidth() const
{
	return mainScreen->getWidth();
}

int GPG::dialog::getHeight() const
{
	return mainScreen->getHeight();
}

bool GPG::dialog::listKeys( std::vector< std::pair<std::string, std::string> >
			    &list)
{
	list.push_back( std::make_pair(Gettext::keyname(_("ABORT:^C")),
				  cancelDescr));

	list.push_back( std::make_pair(Gettext::keyname(_("ENTER:Enter")),
				  enterDescr));

	if (keylist.multiMode)
		list.push_back( std::make_pair(Gettext::keyname(_("SPACE:Space")),
					  _("Select/Unselect")));

	return true;
}

bool GPG::dialog::processKey(const Curses::Key &key)
{

	if (key == key_ABORT)
	{
		closing=true;
		Curses::keepgoing=false;
		if (keylist.multiMode)
			keylist.selectedKeys.clear();
		return true;
	}
	return key.plain() && (unsigned char)key.ukey == key.ukey &&
		(unsigned char)key.ukey < ' ';
}

void GPG::dialog::selected(size_t r)
{
	fingerprint=keys[r].fingerprint;
	Curses::keepgoing=false;
	closing=true;
}

std::string GPG::select_private_key(std::string fingerprint,
			       std::string title,
			       std::string prompt,
			       std::string enterDescr,
			       std::string abortDescr)
{
	return select_key(secret_keys, fingerprint, title, prompt,
			  enterDescr, abortDescr);
}

std::string GPG::select_public_key(std::string fingerprint,
			      std::string title,
			      std::string prompt,
			      std::string enterDescr,
			      std::string abortDescr)
{
	return select_key(public_keys, fingerprint, title, prompt,
			  enterDescr, abortDescr);
}

std::string GPG::select_key( std::vector<Key> &keyVec, std::string fingerprint,
			std::string title, std::string prompt,
			std::string enterDescr,
			std::string abortDescr)
{
	if (keyVec.size() == 0)
	{
		statusBar->clearstatus();
		statusBar->status(_("No keys are installed."));
		return "";
	}

	myServer::nextScreen=NULL;

	std::string s;

	{
		GPG::dialog myKeylist(keyVec, fingerprint, title,
				      abortDescr, enterDescr);
		// _("SELECT PRIVATE KEY"));

		myKeylist.requestFocus();

		statusBar->clearstatus();
		statusBar->status(prompt);

		myServer::eventloop();
		mainScreen->erase();

		if (!myKeylist.orderlyClose())
			return ""; // Some kind of an interrupt

		s=myKeylist;
		Curses::keepgoing=true;
	}

	mainScreen->draw();
	return s;
}

void GPG::select_public_keys(std::vector<std::string> &fingerprints,
			     std::string title,
			     std::string prompt,
			     std::string enterDescr,
			     std::string abortDescr)
{
	if (public_keys.size() == 0)
	{
		statusBar->clearstatus();
		statusBar->status(_("No keys are installed."));
		return;
	}

	myServer::nextScreen=NULL;

	{
		GPG::dialog myKeylist(public_keys, fingerprints, title,
				      abortDescr, enterDescr);

		myKeylist.requestFocus();

		statusBar->clearstatus();
		statusBar->status(prompt);

		myServer::eventloop();
		mainScreen->erase();

		if (!myKeylist.orderlyClose())
			return; // Some kind of an interrupt

		Curses::keepgoing=true;

		myKeylist >> fingerprints;
	}

	mainScreen->draw();
}

/////////////////////////////////////////////////////////////////////////
//
// Common code creates libmail_gpg_info.argv.  Since this is C++, we
// try not to do dynamic alloc.  The caller supplies the std::vector<std::string>
// arguments, and we allocate memory using C++ vectors.  The caller supplies
// two vectors, as follows.  The final argv array is argv_cp

void GPG::create_argv(std::vector<std::string> &argv,
		      std::vector< std::vector<char> > &argv_ptr,
		      std::vector<char *> &argv_cp)
{
	std::vector<std::string>::iterator ekb, eke;

	ekb=argv.begin();
	eke=argv.end();

	while (ekb != eke)
	{
		std::vector<char> vc;

		vc.insert(vc.end(), ekb->begin(), ekb->end());
		vc.push_back(0);

		argv_ptr.push_back(vc);
		++ekb;
	}

	std::vector< std::vector<char> >::iterator ab=argv_ptr.begin(),
		ae=argv_ptr.end();

	while (ab != ae)
	{
		argv_cp.push_back( &(*ab)[0]);
		++ab;
	}
}

// Export a key

class GPG::exportKeyHelper {

	std::ostream &oStream;

	std::string errmsg;

public:
	exportKeyHelper(std::ostream &oStreamArg);
	~exportKeyHelper();

	static int out_func(const char *p, size_t n, void *);
	static int err_func(const char *p, size_t n, void *);

	int out_func(const char *p, size_t n);
	int err_func(const char *p, size_t n);

	operator std::string() const { return errmsg; }
};

GPG::exportKeyHelper::exportKeyHelper(std::ostream &oStreamArg)
	: oStream(oStreamArg)
{
}

GPG::exportKeyHelper::~exportKeyHelper()
{
}

int GPG::exportKeyHelper::out_func(const char *p, size_t n, void *vp)
{
	return ( ((GPG::exportKeyHelper *)vp)->out_func(p, n));
}

int GPG::exportKeyHelper::err_func(const char *p, size_t n, void *vp)
{
	return ( ((GPG::exportKeyHelper *)vp)->err_func(p, n));
}

int GPG::exportKeyHelper::out_func(const char *p, size_t n)
{
	return oStream.write(p, n).fail() ? -1:0;
}

int GPG::exportKeyHelper::err_func(const char *p, size_t n)
{
	errmsg += std::string(p, p+n);
	return 0;
}

std::string GPG::exportKey(std::string fingerprint,
		      bool secret,
		      std::ostream &oStream)
{
	exportKeyHelper h(oStream);

	if (libmail_gpg_exportkey("", secret ? 1:0, fingerprint.c_str(),
				  &GPG::exportKeyHelper::out_func,
				  &GPG::exportKeyHelper::err_func, &h) == 0)
		return "";

	return h;
}

//////////////////////////////////////////////////////////////////////////
//
// A dialog confirming a key selection.

class GPG::confirmHelper : public CursesContainer {

	CursesLabel prompt;

	class keyDisplay : public CursesContainer {

		std::vector<CursesLabel *> labels;
	public:
		keyDisplay(CursesContainer *parent, GPG::Key &key);
		~keyDisplay();

	private:
		CursesLabel *add(std::string, CursesAttr);
	};

	keyDisplay key1, key2;

	CursesButtonRedirect<GPG::confirmHelper> okButton, cancelButton;

	bool returnCode;

public:
	confirmHelper(CursesContainer *parent,
		      std::string promptStr,
		      GPG::Key &selectedKey,
		      GPG::Key *key2,
		      std::string okDescr,
		      std::string cancelDescr);

	bool isDialog() const;
	int getWidth() const;

	operator bool() const;
	~confirmHelper();

private:
	void doOk();
	void doCancel();

};

GPG::confirmHelper::keyDisplay::keyDisplay(CursesContainer *parent,
					   GPG::Key &key)
	: CursesContainer(parent)
{
	std::vector<std::pair<std::string, int> > columns;

	GPG::Key::getDescriptionColumns(columns);

	std::vector<std::pair<std::string, int> >::iterator cb=columns.begin(),
		ce=columns.end();

	{
		CursesAttr headingAttr;

		headingAttr.setUnderline();

		while (cb != ce)
		{
			CursesLabel *l=add(cb->first, headingAttr);

			l->setCol(cb->second);
			++cb;
		}
	}

	std::vector<std::string> description;

	key.getDescription(description, parent ? parent->getWidth():80);

	size_t i;
	CursesAttr norm_attr;

	for (i=0; i<description.size() && i < 10; i++)
		add(description[i], norm_attr)->setRow(i+1);
}

GPG::confirmHelper::keyDisplay::~keyDisplay()
{
	std::vector<CursesLabel *>::iterator b=labels.begin(), e=labels.end();

	while (b != e)
	{
		if (*b)
			delete *b;
		*b=NULL;
		++b;
	}
}

CursesLabel *GPG::confirmHelper::keyDisplay::add(std::string text, CursesAttr attr)
{
	CursesLabel *p=new CursesLabel(this, text, attr);

	if (!p)
		throw strerror(errno);

	try {
		labels.push_back(p);
	} catch (...) {
		delete p;
		throw;
	}
	return p;
}

GPG::confirmHelper::confirmHelper(CursesContainer *parent,
				  std::string promptStr,
				  GPG::Key &selectedKey,
				  GPG::Key *key2Arg,
				  std::string okDescr,
				  std::string cancelDescr)
	: CursesContainer(parent),
	  prompt(this, promptStr),

	  key1(this, selectedKey),
	  key2(key2Arg ? this:NULL, key2Arg ? *key2Arg:selectedKey),

	  okButton(this, okDescr),
	  cancelButton(this, cancelDescr),
	  returnCode(false)
{
	prompt.setRow(1);
	key1.setRow(3);

	size_t r=5+key1.getHeight();

	if (key2Arg)
	{
		key2.setRow(r);
		r += key2.getHeight() + 1;
	}
	okButton.setRow(r);
	cancelButton.setRow(okButton.getRow() + 2);

	okButton.setCol(4);
	cancelButton.setCol(4);

	okButton=this;
	cancelButton=this;

	okButton=&GPG::confirmHelper::doOk;
	cancelButton=&GPG::confirmHelper::doCancel;

	okButton.requestFocus();
}


bool GPG::confirmHelper::isDialog() const
{
	return true;
}

int GPG::confirmHelper::getWidth() const
{
	return getParent()->getWidth();
}

GPG::confirmHelper::operator bool() const
{
	return returnCode;
}

GPG::confirmHelper::~confirmHelper()
{
}

void GPG::confirmHelper::doOk()
{
	returnCode=true;
	Curses::keepgoing=false;
}

void GPG::confirmHelper::doCancel()
{
	Curses::keepgoing=false;
}

bool GPG::confirmKeySelection(std::string prompt,
			      Key &selectedKey,
			      Key *key2,

			      std::string okDescr,
			      std::string cancelDescr)
{
	bool rc;

	{
		confirmHelper helperDialog(mainScreen, prompt, selectedKey,
					   key2,
					   okDescr,
					   cancelDescr);

		myServer::eventloop();
		rc=helperDialog;
		if (myServer::nextScreen)
			return false; // Interrupted, for some reason.
		Curses::keepgoing=true;
	}

	mainScreen->erase();
	mainScreen->draw();
	return rc;
}

