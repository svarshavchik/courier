/*
** Copyright 2002-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "misc.H"
#include "mail.H"
#include "structure.H"
#include "envelope.H"
#include "rfcaddr.H"
#include <cstring>

#include "rfc822/rfc2047.h"
#include "rfc2045/rfc2045.h"
#include "unicode/unicode.h"

#include <errno.h>

#include <map>
#include <string>

using namespace std;

mail::mimestruct::parameterList::parameterList()
{
}

mail::mimestruct::parameterList::~parameterList()
{
}

static int ins_param(const char *param,
		     const char *value,
		     void *voidp)
{
	string v=value;

	if (v.size() > 2 && v[0] == '"' && v[v.size()-1] == '"')
		v=v.substr(1, v.size()-2);

	((std::map<string, string> *)voidp)
		->insert(make_pair(string(param), v));
	return 0;
}

void mail::mimestruct::parameterList::set(string name, string value,
					  string charset,
					  string language)
{
	iterator p, b, e;

	mail::upper(name);
	p=begin();
	e=end();

	size_t s=name.size();

	while (p != e)
	{
		b=p++;
		if (b->first == name)
		{
			param_map.erase(b);
			continue;
		}

		if (b->first.size() > s &&
		    b->first.substr(0, s) == name &&
		    b->first[s] == '*') // RFC 2231
		{
			param_map.erase(b);
			continue;
		}
	}

	rfc2231_attrCreate(name.c_str(),
			   value.c_str(),
			   charset.c_str(),
			   language.c_str(),
			   ins_param,
			   &param_map);
}

bool mail::mimestruct::parameterList::exists(string name) const
{
	mail::upper(name);

	const_iterator b, e;

	b=begin();
	e=end();

	size_t s=name.size();

	while (b != e)
	{
		if (b->first == name)
			return true;

		if (b->first.size() > s &&
		    b->first.substr(0, s) == name &&
		    b->first[s] == '*') // RFC 2231
			return true;
		b++;
	}
	return false;
}

string mail::mimestruct::parameterList::get(string name,
					    string chset)
	const
{
	mail::upper(name);

	struct rfc2231param *paramList=NULL;

	const_iterator b, e;

	b=begin();
	e=end();

	string stringRet;

	try {
		while (b != e)
		{
			if (rfc2231_buildAttrList(&paramList,
						  name.c_str(),
						  b->first.c_str(),
						  b->second.c_str()) < 0)
				LIBMAIL_THROW("Out of memory.");

			b++;
		}

		int charsetLen;
		int langLen;
		int textLen;

		rfc2231_paramDecode(paramList, NULL, NULL, NULL,
				    &charsetLen,
				    &langLen,
				    &textLen);

		vector<char> charsetStr, langStr, textStr;

		charsetStr.insert(charsetStr.begin(), charsetLen, 0);
		langStr.insert(langStr.begin(), langLen, 0);
		textStr.insert(textStr.begin(), textLen, 0);

		rfc2231_paramDecode(paramList, &*charsetStr.begin(),
				    &*langStr.begin(),
				    &*textStr.begin(),
				    &charsetLen,
				    &langLen,
				    &textLen);

		stringRet=&textStr[0];

		if (chset.size() > 0 && charsetStr[0])
		{
			bool err;

			string s=mail::iconvert::convert(stringRet,
							 &charsetStr[0],
							 chset,
							 err);

			if (!err)
				stringRet=s;
		}

		rfc2231_paramDestroy(paramList);

	} catch (...) {
		rfc2231_paramDestroy(paramList);
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	return stringRet;
}

std::string mail::mimestruct::parameterList::toString(string s) const
{
	const_iterator b=begin(), e=end();

	while (b != e)
	{
		s += ";\n  ";

		s += b->first;
		if (b->second.size())
		{
			s += "=";

			string::const_iterator p=b->second.begin(),
				q=b->second.end();

			while (p != q)
			{
				if (!rfc2047_qp_allow_word(*p))
					break;
				++p;
			}

			if (p != q)
			{
				s += "\"";

				for (p=b->second.begin(); p != q; )
				{
					string::const_iterator r=p;

					while (p != q)
					{
						if (*p == '"' || *p == '\\')
							break;
						++p;
					}

					s += string(r, p);

					if (p != q)
					{
						s += "\\";
						s += string(p, p+1);
						++p;
					}
				}
				s += "\"";
			}
			else
				s += b->second;
		}
		++b;
	}

	return s;
}

mail::mimestruct::mimestruct()
	: content_size(0), content_lines(0),
	  message_rfc822_envelope(0), parent(0)
{
}

mail::mimestruct::~mimestruct()
{
	destroy();
}

mail::mimestruct::mimestruct(const mail::mimestruct &cpy)
	: content_size(0), content_lines(0),
	  message_rfc822_envelope(0), parent(0)
{
	(*this)=cpy;
}

mail::mimestruct &mail::mimestruct::operator=(const mail::mimestruct &cpy)
{
	destroy();

	vector<mail::mimestruct *>::const_iterator
		b=cpy.children.begin(),
		e=cpy.children.end();

	while (b != e)
	{
		(*addChild())= **b;
		b++;
	}

#define CPY(x) (x)=(cpy.x)

	CPY(mime_id);
	CPY(type);
	CPY(subtype);
	CPY(type_parameters);
	CPY(content_id);
	CPY(content_description);
	CPY(content_transfer_encoding);
	CPY(content_size);
	CPY(content_lines);
	CPY(content_md5);
	CPY(content_language);
	CPY(content_disposition);
	CPY(content_disposition_parameters);
#undef CPY

	if (cpy.message_rfc822_envelope)
	{
		message_rfc822_envelope=new mail::envelope;

		if (!message_rfc822_envelope)
			LIBMAIL_THROW("Out of memory.");

		*message_rfc822_envelope= *cpy.message_rfc822_envelope;
	}

	return *this;
}

mail::envelope &mail::mimestruct::getEnvelope()
{
	if (!message_rfc822_envelope)
		if ((message_rfc822_envelope=new mail::envelope) == NULL)
			LIBMAIL_THROW("Out of memory.");
	return *message_rfc822_envelope;
}

void mail::mimestruct::destroy()
{
	mail::envelope *p=message_rfc822_envelope;

	if (p)
	{
		message_rfc822_envelope=NULL;
		delete p;
	}

	vector<mail::mimestruct *>::iterator b=children.begin(),
		e=children.end();

	while (b != e)
	{
		delete *b;
		b++;
	}
	children.clear();
}

mail::mimestruct *mail::mimestruct::addChild()
{
	mail::mimestruct *p=new mail::mimestruct;

	if (!p)
		LIBMAIL_THROW("Out of memory.");

	try {
		p->parent=this;
		children.push_back(p);
	} catch (...)
	{
		delete p;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	return p;
}
