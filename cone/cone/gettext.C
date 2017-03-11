/*
** Copyright 2003-2010, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "gettext.H"
#include <stdlib.h>
#include <unistd.h>
#include <sstream>
#include <algorithm>
#include <courier-unicode.h>
#include "libmail/mail.H"
#include <errno.h>
#include <cstring>

#if HAVE_LANGINFO_H
#include <langinfo.h>
#endif

Gettext::Key key_ABORT(N_("ABORT_K:\x03"));
Gettext::Key key_ADDADDRESSBOOK(N_("ADDADDRESSBOOK:Aa"));
Gettext::Key key_ADDDIRECTORY(N_("ADDDIRECTORY:\\\\D"));
Gettext::Key key_ADDFOLDER(N_("ADDFOLDER:Aa"));
Gettext::Key key_ADDLDAP(N_("ADDLDAP_K:Aa"));
Gettext::Key key_ADDREMOVE(N_("ADDREMOVE:-"));
Gettext::Key key_ADDRESSBOOK(N_("ADDRESSBOOK:Aa"));
Gettext::Key key_ADDRIGHTS(N_("ADDRIGHTS:Aa"));
Gettext::Key key_ADDRIGHTSADMINS(N_("ADDRIGHTSADMINS:Aa"));
Gettext::Key key_ADDRIGHTSANON(N_("ADDRIGHTSANON:2"));
Gettext::Key key_ADDRIGHTSANYONE(N_("ADDRIGHTSANYONE:1"));
Gettext::Key key_ADDRIGHTSAUTHUSER(N_("ADDRIGHTSAUTHUSER:3"));
Gettext::Key key_ADDRIGHTSGROUP(N_("ADDRIGHTSGROUP:Gg"));
Gettext::Key key_ADDRIGHTSOWNER(N_("ADDRIGHTSOWNER:Oo"));
Gettext::Key key_ADDRIGHTSUSER(N_("ADDRIGHTSUSER:Uu"));
Gettext::Key key_ALL(N_("ALL:Aa"));
Gettext::Key key_ATTACHFILE(N_("ATTACHFILE:\x14"));
Gettext::Key key_ATTACHKEY(N_("ATTACHKEY:\x05"));
Gettext::Key key_BLOBS(N_("BLOBS:Ss"));
Gettext::Key key_BOUNCE(N_("BOUNCE:Bb"));
Gettext::Key key_CERTIFICATES(N_("CERTIFICATES:Cc"));
Gettext::Key key_CLEAR(N_("CLEAR:Rr"));
Gettext::Key key_CLREOL(N_("MORE:\x0B"));
Gettext::Key key_COPY(N_("COPY:Cc"));
Gettext::Key key_CUT(N_("CUT:\x17"));
Gettext::Key key_DATEACCEPT(N_("DATEACCEPT: "));
Gettext::Key key_DATE(N_("DATE:Dd"));
Gettext::Key key_DAYDEC1(N_("DAYDEC1:-_"));
Gettext::Key key_DAYINC1(N_("DAYINC1:+="));
Gettext::Key key_DELADDRESSBOOKENTRY(N_("DELADDRESSBOOKENTRY:Dd"));
Gettext::Key key_DELETEADDRESSBOOK(N_("DELETEADDRESSBOOK:Dd"));
Gettext::Key key_DELETECERTIFICATE(N_("DELETECERTIFICATE:Dd"));
Gettext::Key key_DELETED(N_("DELETED:Dd"));
Gettext::Key key_DELETE(N_("DELETE:Dd"));
Gettext::Key key_DELFOLDER(N_("DELFOLDER:Dd"));
Gettext::Key key_DELRIGHTS(N_("DELRIGHTS:Dd"));
Gettext::Key key_DICTSPELL(N_("DICTSPELL:\\\\D"));
Gettext::Key key_DOWN(N_("MOVEDOWN:\\\\D"));
Gettext::Key key_DRAFTS(N_("DRAFTS:Dd"));
Gettext::Key key_DSNDELAY(N_("DSNDELAY:Dd"));
Gettext::Key key_DSNFAIL(N_("DSNFAIL:Ff"));
Gettext::Key key_DSNNEVER(N_("DSNNEVER:Nn"));
Gettext::Key key_DSNSUCCESS(N_("DSNSUCCESS:Ss"));
Gettext::Key key_EDIT(N_("EDIT:Ee"));
Gettext::Key key_EDITREPLACE(N_("EDITREPLACE:\x12"));
Gettext::Key key_EDITSEARCH(N_("EDITSEARCH:\x13"));
Gettext::Key key_EXPORTADDRESSBOOKENTRY(N_("Export:Ee"));
Gettext::Key key_EXTEDITOR(N_("EXTEDITOR:\x15"));
Gettext::Key key_EXTFILTER(N_("EXTFILTER:Ee"));
Gettext::Key key_ENCRYPTIONMENU(N_("ENCRYPTIONMENU:Ee"));
Gettext::Key key_ENCRYPT(N_("ENCRYPT:Ee"));
Gettext::Key key_EXPUNGE(N_("EXPUNGE:Xx"));
Gettext::Key key_FILTER(N_("FILTER:Ff"));
Gettext::Key key_FOLDERINDEX(N_("FOLDERINDEX:Ii"));
Gettext::Key key_FULLHEADERS(N_("FULLHEADERS:\x06"));
Gettext::Key key_FWD(N_("FWD:Ff"));
Gettext::Key key_GETFILE(N_("GETFILE:\x07"));
Gettext::Key key_GPGCAREFULCHECK(N_("GPG_CAREFULCHECK:3"));
Gettext::Key key_GPGCASUALCHECK(N_("GPG_CASUALCHECK:2"));
Gettext::Key key_GPGDELKEY(N_("GPGDELKEY:Dd"));
Gettext::Key key_GPGEDITKEY(N_("GPGEDITKEY:Ee"));
Gettext::Key key_GPGNEWKEY(N_("GPGNEWKEY:Nn"));
Gettext::Key key_GPGNOANSWER(N_("GPG_NOANSWER:0"));
Gettext::Key key_GPGNOCHECKING(N_("GPG_NOCHECKING:1"));
Gettext::Key key_GPGSIGNKEY(N_("GPGSIGNKEY:Ss"));
Gettext::Key key_HEADERS(N_("HEADERS:Hh"));
Gettext::Key key_IGNOREALL_K(N_("IGNOREALL_K:Aa"));
Gettext::Key key_IGNORE_K(N_("IGNORE_K:Ii"));
Gettext::Key key_IMAPACCOUNT(N_("IMAPACCOUNT:Ii"));
Gettext::Key key_IMPORTADDRESSBOOKENTRY(N_("IMPORTADDRESSBOOK:Ii"));
Gettext::Key key_IMPORTCERTIFICATE(N_("IMPORTCERTIFICATE:Ii"));
Gettext::Key key_INBOXMBOX(N_("INBOXMBOX:Ss"));
Gettext::Key key_JUMP(N_("JUMP:Jj"));
Gettext::Key key_JUSTIFY(N_("JUSTIFY:\x0a"));
Gettext::Key key_LARGER(N_("LARGER:Ll"));
Gettext::Key key_LESSTHAN(N_("LESSTHAN:<"));
Gettext::Key key_LISTFOLDERS(N_("LISTFOLDERS:Ll"));
Gettext::Key key_MACRO(N_("MACRO:\x0e"));
Gettext::Key key_MAINMENU(N_("MAINMENU:Mm"));
Gettext::Key key_MARK(N_("MARK:Rr"));
Gettext::Key key_MARKUNMARK(N_("MARKUNMARK: "));
Gettext::Key key_MASTERPASSWORD(N_("MASTERPASSWORD:Pp"));
Gettext::Key key_MONDEC1(N_("MONDEC1:[{"));
Gettext::Key key_MONINC1(N_("MONINC1:]}"));
Gettext::Key key_MORE(N_("MORE:\x0F"));
Gettext::Key key_MOVE(N_("MOVE:Mm"));
Gettext::Key key_MOVEMESSAGE(N_("MOVEMESSAGE:Oo"));
Gettext::Key key_MSGSEARCH(N_("MSGSEARCH:/"));
Gettext::Key key_NEWACCOUNT(N_("NEWACCOUNT:Nn"));
Gettext::Key key_NEXTMESSAGE(N_("NEXTMESSAGE:Nn"));
Gettext::Key key_NEXTUNREAD(N_("NEXTUNREAD:]"));
Gettext::Key key_NNTPACCOUNT(N_("NNTPACCOUNT:Nn"));
Gettext::Key key_NOT1(N_("NOT1:!!"));
Gettext::Key key_OPENFILE(N_("OPENFILE:\x06"));
Gettext::Key key_OTHERMBOX(N_("OTHERMBOX:Oo"));
Gettext::Key key_PERMS(N_("PERMISSIONS:Pp"));
Gettext::Key key_PERMEDIT(N_("EDITPERMISSIONS:Ee"));
Gettext::Key key_PERMVIEW(N_("VIEWPERMISSIONS:Vv"));
Gettext::Key key_POP3ACCOUNT(N_("POP3ACCOUNT:Pp"));
Gettext::Key key_POP3MAILDROPACCOUNT(N_("POP3MAILDROPACCOUNT:Mm"));
Gettext::Key key_POSTPONE(N_("POSTPONE:\x10"));
Gettext::Key key_PRINT(N_("PRINT:|"));
Gettext::Key key_PREVMESSAGE(N_("PREVMESSAGE:Pp"));
Gettext::Key key_PREVUNREAD(N_("PREVUNREAD:["));
Gettext::Key key_PRIVATEKEY(N_("PRIVATEKEY:Pp"));
Gettext::Key key_QUIT(N_("QUIT:Qq"));
Gettext::Key key_RECVBEFORE1(N_("RECVBEFORE1:44"));
Gettext::Key key_RECVON1(N_("RECVON1:55"));
Gettext::Key key_RECVSINCE1(N_("RECVSINCE1:66"));
Gettext::Key key_REMOTE(N_("REMOTE:Rr"));
Gettext::Key key_RENAME(N_("RENAME:Rr"));
Gettext::Key key_RENAMEADDRESSBOOKENTRY(N_("RENAMEADDRESSBOOKENTRY:Rr"));
Gettext::Key key_RENAMECERTIFICATE(N_("RENAME:Rr"));
Gettext::Key key_REPLACE0(N_("REPLACE0:0"));
Gettext::Key key_REPLACE1(N_("REPLACE1:1"));
Gettext::Key key_REPLACE2(N_("REPLACE2:2"));
Gettext::Key key_REPLACE3(N_("REPLACE3:3"));
Gettext::Key key_REPLACE4(N_("REPLACE4:4"));
Gettext::Key key_REPLACE5(N_("REPLACE5:5"));
Gettext::Key key_REPLACE6(N_("REPLACE6:6"));
Gettext::Key key_REPLACE7(N_("REPLACE7:7"));
Gettext::Key key_REPLACE8(N_("REPLACE8:8"));
Gettext::Key key_REPLACE9(N_("REPLACE9:9"));
Gettext::Key key_REPLACE_K(N_("REPLACE_K:Rr"));
Gettext::Key key_ROT13(N_("ROT13:\x12"));
Gettext::Key key_REPLIED(N_("REPLIED:Rr"));
Gettext::Key key_REPLY(N_("REPLY:Rr"));
Gettext::Key key_RESET(N_("RESET:\x12"));
Gettext::Key key_SAVEAS(N_("SAVEAS:\x01"));
Gettext::Key key_SAVEFILE(N_("SAVE:\x18"));
Gettext::Key key_SAVE(N_("SAVE:Ss"));
Gettext::Key key_SEARCHANYWHERE(N_("ANYWHEREKEY:Aa"));
Gettext::Key key_SEARCHBCC(N_("SEARCHBCC:Bb"));
Gettext::Key key_SEARCHBROADEN(N_("SEARCHBROADEN:Bb"));
Gettext::Key key_SEARCHCC(N_("SEARCHCC:Cc"));
Gettext::Key key_SEARCHCONTENTS(N_("CONTENTSKEY:Oo"));
Gettext::Key key_SEARCHFROM(N_("SEARCHFROM:Ff"));
Gettext::Key key_SEARCHHEADER(N_("HEADERKEY:Hh"));
Gettext::Key key_SEARCHNARROW(N_("SEARCHNARROW:Nn"));
Gettext::Key key_SEARCH(N_("SEARCH:;"));
Gettext::Key key_SEARCHSUBJECT(N_("SEARCHSUBJECT:Ss"));
Gettext::Key key_SEARCHTO(N_("SEARCHTO:Tt"));
Gettext::Key key_SELECTDSN(N_("SELECTDSN:Dd"));
Gettext::Key key_SELECT(N_("SELECT:Ss"));
Gettext::Key key_SEND(N_("SEND:\x18"));
Gettext::Key key_SENTBEFORE1(N_("SENTBEFORE1:11"));
Gettext::Key key_SENT(N_("SENT:Ss"));
Gettext::Key key_SENTON1(N_("SENTON1:22"));
Gettext::Key key_SENTSINCE1(N_("SENTSINCE1:33"));
Gettext::Key key_SETUPSCREEN(N_("SETUPSCREEN:Ss"));
Gettext::Key key_SIGN(N_("SIGN:Ss"));
Gettext::Key key_SIZE(N_("SIZE:Zz"));
Gettext::Key key_SMALLER(N_("SMALLER:Ss"));
Gettext::Key key_SORTFOLDER_ARRIVAL(N_("SORTFOLDER_ARRIVAL:Aa"));
Gettext::Key key_SORTFOLDER_DATE(N_("SORTFOLDER_DATE:Dd"));
Gettext::Key key_SORTFOLDER_NAME(N_("SORTFOLDER_NAME:Nn"));
Gettext::Key key_SORTFOLDER(N_("SORTFOLDER:$"));
Gettext::Key key_SORTFOLDER_SUBJECT(N_("SORTFOLDER_SUBJECT:Ss"));
Gettext::Key key_SORTFOLDER_THREAD(N_("SORTFOLDER_THREAD:Tt"));
Gettext::Key key_SORTNOT(N_("SORTNOT:!"));
Gettext::Key key_SPECIAL(N_("SPECIAL:Uu"));
Gettext::Key key_STATUS(N_("STATUS:Ss"));
Gettext::Key key_TAG0(N_("TAG0:0"));
Gettext::Key key_TAG1(N_("TAG1:1"));
Gettext::Key key_TAG2(N_("TAG2:2"));
Gettext::Key key_TAG3(N_("TAG3:3"));
Gettext::Key key_TAG4(N_("TAG4:4"));
Gettext::Key key_TAG5(N_("TAG5:5"));
Gettext::Key key_TAG6(N_("TAG6:6"));
Gettext::Key key_TAG7(N_("TAG7:7"));
Gettext::Key key_TAG8(N_("TAG8:8"));
Gettext::Key key_TAG9(N_("TAG9:9"));
Gettext::Key key_TAG(N_("TAG:Tt"));
Gettext::Key key_TAKEADDR(N_("TAKEADDR:Tt"));
Gettext::Key key_TEXT(N_("TEXT:Tt"));
Gettext::Key key_TODIRECTORY(N_("TODIRECTORY:\x14"));
Gettext::Key key_TOGGLE(N_("TOGGLE:Gg"));
Gettext::Key key_TOGGLESPACE(N_("TOGGLE_K: "));
Gettext::Key key_TOGGLEFLOWED(N_("FLOWED_K:\x06"));
Gettext::Key key_TOP(N_("TOP:Tt"));
Gettext::Key key_UNDELETE(N_("UNDELETE:Uu"));
Gettext::Key key_UNREADBULK(N_("UNREAD:Ee"));
Gettext::Key key_UNENCRYPT(N_("UNENCRYPT:Yy"));
Gettext::Key key_UNMARK(N_("UNMARK:Kk"));
Gettext::Key key_UNREAD(N_("UNREAD:Uu"));
Gettext::Key key_UNWATCH(N_("UNWATCH:Aa"));
Gettext::Key key_UP(N_("MOVEUP:\x15"));
Gettext::Key key_VIEWATT(N_("VIEWATT:Vv"));
Gettext::Key key_WATCH2(N_("WATCH:Ww"));
Gettext::Key key_WATCHDAYS(N_("WATCHDAYS:Dd"));
Gettext::Key key_WATCHDEPTH(N_("WATCHDEPTH:Rr"));
Gettext::Key key_WATCH(N_("WATCH:Aa"));
Gettext::Key key_WRITE(N_("WRITE:Ww"));
Gettext::Key key_YANK(N_("YANK:\x19"));
Gettext::Key key_YEARDEC1(N_("YEARDEC1:<,"));
Gettext::Key key_YEARINC1(N_("YEARINC1:>."));

Gettext::Gettext(std::string param) : paramStr(param)
{
}

Gettext::~Gettext()
{
}

Gettext &Gettext::arg(std::string arg)
{
	args.push_back(arg);
	return *this;
}

Gettext &Gettext::arg(const char *arg)
{
	args.push_back(std::string(arg));
	return *this;
}

Gettext &Gettext::arg(unsigned long ul)
{
	std::string buffer;

	{
		std::ostringstream o;

		o << ul;
		buffer=o.str();
	}

	args.push_back(std::string(buffer));
	return *this;
}

Gettext &Gettext::arg(long l)
{
	std::string buffer;

	{
		std::ostringstream o;

		o << l;
		buffer=o.str();
	}


	args.push_back(std::string(buffer));
	return *this;
}

Gettext &Gettext::arg(unsigned u)
{
	return arg( (unsigned long)u);
}

Gettext &Gettext::arg(int i)
{
	return arg( (long)i );
}

Gettext &Gettext::arg(unsigned short us)
{
	return arg( (unsigned long)us );
}

Gettext &Gettext::arg(short s)
{
	return arg ( (long) s);
}

Gettext::operator std::string() const
{
	std::string fstr="";

	std::string::const_iterator b=paramStr.begin(), e=paramStr.end();

	// Substitute %n% with parameter #n

	while (b != e)
	{
		std::string::const_iterator c=b;

		while (c != e && *c != '%')
			c++;

		fstr.append(b, c);

		b=c;
		if (c == e)
			continue;


		int n=0;

		b++;

		if (b != e && *b == '%')
		{
			b++;
			fstr.append("%");
			continue;
		}

		while (b != e)
		{
			if (! isdigit(*b))
			{
				b++;
				break;
			}

			n = n * 10 + (*b++ - '0');
		}

		if (n > 0 && (size_t)n <= args.size())
			fstr.append(args[n-1]);
	}

	return fstr;
}

const char *Gettext::keyname(const char *k)
{
	const char *p=strchr(k, ':');

	if (p)
		k=p+1;

	return k;
}

std::string Gettext::toutf8(std::string str)
{
	return unicode::iconvert::convert(str, unicode_default_chset(),
				       "utf-8");
}

std::string Gettext::fromutf8(std::string str)
{
	return unicode::iconvert::convert(str, "utf-8",
				       unicode_default_chset());
}

/////////////////////////////////////////////////////////////////////////////

Gettext::Key::Key(const char *p) : keyname(p)
{
}

Gettext::Key::~Key()
{
}

void Gettext::Key::init()
{
	if (keys.size() > 0)
		return; // Already wuz here

	const char *p=gettext(keyname);

	while (p && *p)
	{
		if (*p++ == ':')
			break;
	}

	std::string n(p);

	// gettext has problems with a literal \x04, so provide an alternative

	std::string::iterator b, e, q;

	for (b=n.begin(), e=n.end(), q=b; b != e; ++b)
	{
		if (*b == '\\')
		{
			std::string::iterator c=b;

			if (++c != e && *c++ == '\\' && c != e)
			{
				b=c;

				*q++ = *b & 31;
				continue;
			}
		}

		*q++=*b;
	}

	n.erase(q, b);

	unicode::iconvert::convert(n, unicode_default_chset(), keys);
}

bool Gettext::Key::operator==(char32_t k)
{
	init();
	return (find(keys.begin(), keys.end(), k) != keys.end());
}
