/*
** Copyright 2002-2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "imaplisthandler.H"
#include "imapfolders.H"
#include "smaplist.H"
#include "misc.H"
#include "unicode/unicode.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

using namespace std;

// Invoked from mail::imapFolder::readSubFolders()

void mail::imap::readSubFolders(string path,
				mail::callback::folderList &callback1,
				mail::callback &callback2)
{
	if (!ready(callback2))
		return;

	if (smap)
	{
		mail::smapLIST *s=new mail::smapLIST(path, callback1,
						     callback2);

		installForegroundTask(s);
	}
	else
	{
		mail::imapListHandler *h=
			new mail::imapListHandler(callback1, callback2, path,
						  false);
		installForegroundTask(h);
	}
}

void mail::imap::findFolder(string folder,
			    mail::callback::folderList &callback1,
			    mail::callback &callback2)
{
	if (!ready(callback2))
		return;

	if (folder.size() == 0) // Top level hierarchy
	{
		mail::imapFolder f(*this, "", "", "", -1);

		f.hasMessages(false);
		f.hasSubFolders(true);
		vector<const mail::folder *> list;
		list.push_back(&f);

		callback1.success(list);
		callback2.success("OK");
		return;
	}

	installForegroundTask( smap ?
			       (imapHandler *)
			       new mail::smapLISToneFolder(folder,
							   callback1,
							   callback2)
			       : new mail::imapListHandler(callback1,
							   callback2,
							   folder,
							   true));
}

string mail::imap::translatePath(string path)
{
	if (!smap)
	{
		char *p=libmail_u_convert_tobuf(path.c_str(),
						unicode_default_chset(),
						unicode_x_imap_modutf7, NULL);

		if (p)
		{
			try
			{
				path=p;
				free(p);
				return path;
			} catch (...) {
				free(p);
				LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
			}
		}

		return "";
	}

	vector<string> words;

	do
	{
		string component;

		size_t n=path.find('/');

		if (n == std::string::npos)
		{
			component=path;
			path="";
		}
		else
		{
			component=path.substr(0, n);
			path=path.substr(n+1);
		}

		vector<unicode_char> ucvec;

		unicode_char *uc;
		size_t ucsize;
		libmail_u_convert_handle_t h;

		if ((h=libmail_u_convert_tou_init(unicode_default_chset(),
						  &uc, &ucsize, 1)) == NULL)
		{
			uc=NULL;
		}
		else
		{
			libmail_u_convert(h, component.c_str(),
					  component.size());

			if (libmail_u_convert_deinit(h, NULL))
				uc=NULL;
		}

		if (!uc)
		{
			errno=EINVAL;
			return "";
		}

		try {
			size_t n;

			for (n=0; uc[n]; n++)
				;

			ucvec.insert(ucvec.end(), uc, uc+n);

			for (n=0; n<ucvec.size(); )
			{
				if (ucvec[n] == '\\' && n+1 < ucvec.size())
				{
					ucvec.erase(ucvec.begin()+n,
						    ucvec.begin()+n+1);
					n++;
					continue;
				}
				if (ucvec[n] == '%')
				{
					unicode_char ucnum=0;
					size_t o=n+1;

					while (o < ucvec.size())
					{
						if ((unsigned char)ucvec[o]
						    != ucvec[o])
							break;
						if (!isdigit(ucvec[o]))
							break;

						ucnum=ucnum * 10 +
							ucvec[o]-'0';
						++o;
					}

					if (o < ucvec.size() &&
					    ucvec[o] == ';')
						++o;

					ucvec[n++]=ucnum;
					ucvec.erase(ucvec.begin()+n,
						    ucvec.begin()+o);
					continue;
				}
				n++;
			}
			free(uc);
		} catch (...) {
			free(uc);
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}

		char *p;
		size_t psize;

		if ((h=libmail_u_convert_fromu_init("utf-8", &p, &psize, 1))
		    != NULL)
		{
			libmail_u_convert_uc(h, &ucvec[0], ucvec.size());
			if (libmail_u_convert_deinit(h, NULL))
				p=NULL;
		}

		if (!p)
		{
			errno=EINVAL;
			return "";
		}

		try {
			words.push_back(p);
			free(p);
		} catch (...) {
			free(p);
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}
	} while (path.size() > 0);

	vector<const char *> wordsp;

	vector<string>::iterator wb=words.begin(), we=words.end();

	while (wb != we)
	{
		wordsp.push_back( wb->c_str() );
		wb++;
	}

	return mail::smapHandler::words2path(wordsp);
}

mail::imapListHandler::
imapListHandler(mail::callback::folderList &myCallback,
		      mail::callback &myCallback2,
		      string myHier, bool oneFolderOnlyArg)
	: callback1(myCallback), callback2(myCallback2), hier(myHier),
	  oneFolderOnly(oneFolderOnlyArg), fallbackOneFolderOnly(false)
{
}

void mail::imapListHandler::installed(mail::imap &imapAccount)
{
	const char *sep="%";

	if (oneFolderOnly)
		sep="";

	imapAccount.imapcmd("LIST", "LIST \"\" " +
		     imapAccount.quoteSimple(hier + sep) + "\r\n");
}

mail::imapListHandler::~imapListHandler()
{
}

const char *mail::imapListHandler::getName()
{
	return "LIST";
}

void mail::imapListHandler::timedOut(const char *errmsg)
{
	callback2.fail(errmsg ? errmsg:"Server timed out.");
}

bool mail::imapListHandler::untaggedMessage(mail::imap &imapAccount, string name)
{
	if (name != "LIST")
		return false;
	imapAccount.installBackgroundTask( new mail::imapLIST(folders,
						       hier.length(),
						       oneFolderOnly));
	return true;
}

bool mail::imapListHandler::taggedMessage(mail::imap &imapAccount, string name,
					  string message,
					  bool okfail,
					  string errmsg)
{
	if (name != "LIST")
		return false;

	if (okfail && oneFolderOnly && !fallbackOneFolderOnly &&
	    folders.size() == 0)
	{
		fallbackOneFolderOnly=true;
		imapAccount.imapcmd("LIST",
				    "LIST " +
				    imapAccount.quoteSimple(hier)
				    + " \"\"\r\n");
		return true;
	}

	if (okfail)
	{
		vector<const mail::folder *> folderPtrs;

		vector<mail::imapFolder>::iterator b, e;

		b=folders.begin();
		e=folders.end();

		while (b != e)
		{
			if (fallbackOneFolderOnly)
			{
				b->path=hier;
			}

			folderPtrs.push_back( &*b);
			b++;
		}

		callback1.success(folderPtrs);
		callback2.success("OK");
	}
	else
		callback2.fail(errmsg);

	imapAccount.uninstallHandler(this);
	return true;
}

//////////////////////////////////////////////////////////////////////////
//
// Untagged LIST parser

mail::imapLIST::imapLIST(vector<mail::imapFolder> &mylist,
			       size_t pfixLengthArg,
			       bool oneNameOnlyArg)
	: folderList(mylist), pfixLength(pfixLengthArg),
	  oneNameOnly(oneNameOnlyArg),
	  next_func(&mail::imapLIST::start_attribute_list),
	  hasChildren(false), hasNoChildren(false), marked(false),
	  unmarked(false), noSelect(false)
{
}

mail::imapLIST::~imapLIST()
{
}

void mail::imapLIST::installed(mail::imap &imapAccount)
{
}

const char *mail::imapLIST::getName()
{
	return ("* LIST");
}

void mail::imapLIST::timedOut(const char *errmsg)
{
}

void mail::imapLIST::process(mail::imap &imapAccount, Token t)
{
	(this->*next_func)(imapAccount, t);
}

void mail::imapLIST::start_attribute_list(mail::imap &imapAccount, Token t)
{
	if (t == NIL)
		next_func= &mail::imapLIST::get_hiersep;
	else if (t == '(')
		next_func= &mail::imapLIST::get_attribute;
	else
		error(imapAccount);
}

void mail::imapLIST::get_attribute(mail::imap &imapAccount, Token t)
{
	if (t == ATOM)
	{
		string atom=t;

		upper(atom);

		if (atom == "\\HASCHILDREN")
			hasChildren=true;
		else if (atom == "\\HASNOCHILDREN")
			hasNoChildren=true;
		else if (atom == "\\NOSELECT")
			noSelect=true;
		else if (atom == "\\MARKED")
			marked=true;
		else if (atom == "\\UNMARKED")
			unmarked=true;
		else if (atom == "\\NOINFERIORS")
			hasNoChildren=true;
		return;
	}
	else if (t == ')')
		next_func= &mail::imapLIST::get_hiersep;
	else
		error(imapAccount);
}

void mail::imapLIST::get_hiersep(mail::imap &imapAccount, Token t)
{
	if (t == ATOM || t == STRING || t == NIL)
	{
		hiersep= t.text;
		next_func= &mail::imapLIST::get_name;
	}
	else error(imapAccount);
}

void mail::imapLIST::get_name(mail::imap &imapAccount, Token t)
{
	if (t == ATOM || t == STRING)
	{
		string nameVal;

		nameVal= t.text;

		if (oneNameOnly)
		{
			if (hiersep.size() > 0)
			{
				size_t p=nameVal.rfind(hiersep[0]);

				if (p != std::string::npos)
					nameVal=nameVal.substr(p);
			}
		}
		else
		{
			if (pfixLength <= nameVal.length())
				nameVal.erase(0, pfixLength);
		}
		char *p=libmail_u_convert_tobuf(nameVal.c_str(),
						unicode_x_imap_modutf7,
						unicode_default_chset(),
						NULL);

		try {
			nameVal=p;
			free(p);
		} catch (...) {
			free(p);
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}

		if (nameVal.size() == 0 && !oneNameOnly) // LIST % artifact
		{
			done();
			return;
		}

		mail::imapFolder f(imapAccount, t.text, hiersep, nameVal, -1);

		f.hasMessages(!noSelect);
		f.hasSubFolders(noSelect || hasChildren);
		folderList.push_back(f);
		next_func= &mail::imapLIST::get_xattr_start;
		xattributes.begin();
	}
	else
		error(imapAccount);
}

void mail::imapLIST::get_xattr_start(mail::imap &imapAccount, Token t)
{
	if (t == EOL)
	{
		done();
		return;
	}
	get_xattr_do(imapAccount, t);
}

void mail::imapLIST::get_xattr_do(mail::imap &imapAccount, Token t)
{
	bool err_flag;

	next_func= &mail::imapLIST::get_xattr_do;

	if (!xattributes.process(imapAccount, t, err_flag))
		return;

	if (err_flag)
	{
		error(imapAccount);
		return;
	}
	processExtendedAttributes(folderList.end()[-1]);
	done();
}

void mail::imapLIST::processExtendedAttributes(mail::imapFolder &folder)
{
}
