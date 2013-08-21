/*
** Copyright 2002-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "mimetypes.H"
#include "namespace.H"
#include "unicode/unicode.h"
#include <fstream>
#include <cctype>
#include <cstring>

using namespace std;

LIBMAIL_START

#include "mimetypefiles.h"

LIBMAIL_END

mail::mimetypes::mimetypes(string searchKey)
{
	size_t i;

	for (i=0; mimetypefiles[i]; i++)
	{
		ifstream f( mimetypefiles[i]);

		while (!f.bad() && !f.eof())
		{
			string line;

			getline(f, line);

			size_t p;

			p=line.find('#');

			if (p != std::string::npos)
				line=line.substr(0, p);

			string::iterator b, e, c;

			vector<string> words;

			b=line.begin();
			e=line.end();

			bool found=false;

			while (b != e)
			{
				if (unicode_isspace((unsigned char)*b))
				{
					b++;
					continue;
				}
				c=b;
				while (b != e && !unicode_isspace((unsigned char)
							  *b))
					b++;

				string w=string(c, b);

				if (strcasecmp(w.c_str(), searchKey.c_str())
				    == 0)
					found=true;
				words.push_back(w);
			}

			if (found)
			{
				type=words[0];
				extensions.insert(extensions.end(),
						  words.begin()+1,
						  words.end());
				return;
			}
		}
	}
}

mail::mimetypes::~mimetypes()
{
}
