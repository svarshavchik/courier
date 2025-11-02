/*
** Copyright 2025 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "config.h"
#include "courier.h"

#include <fstream>
#include <iostream>
#include <algorithm>
#include <string>
#include <string_view>
#include <array>
#include <locale>

#define FILTERBINDIR	"testfilterbin"
#define FAXTMPDIR	"testfaxtmp"

static std::string generated_fax;

static void testfaxconvert_capture(const char *filename)
{
	if (!generated_fax.empty())
		generated_fax += "---\n";

	std::ifstream i{filename};

	std::copy(std::istreambuf_iterator<char>{i},
		  std::istreambuf_iterator<char>{},
		  std::back_inserter(generated_fax)
	);
}

#define TESTFAXCONVERT(s) testfaxconvert_capture(s)

#include "faxconvert.C"
#include "testfuncs.h"

void testfaxconvert()
{
	static const struct {
		const char *message;
		const char *result;
	} tests[]={
		// Test 1
		{
			"Mime-Version: 1.0\n"
			"Content-Type: multipart/mixed; boundary=aaa\n"
			"From: sender@испытание\n"
			"To: receipient@xn--80akhbyknj4f\n"
			"Subject: Sample fax\n"
			"\n"
			"\n"
			"This is a multipart message\n"
			"--aaa\n"
			"Content-Type: multipart/alternative; boundary=bbb\n"
			"\n"
			"This is a multipart message\n"
			"--bbb\n"
			"Content-Type: text/html\n"
			"\n"
			"HTML\n"
			"\n"
			"--bbb\n"
			"Content-Type: text/plain; charset=utf-8\n"
			"Content-Transfer-Encoding: quoted-printable\n"
			"\n"
			"This is a s=61mple fax\n"
			"\n"
			"--bbb---\n"
			"--aaa\n"
			"Content-Type: text/plain; charset=utf-8\n"
			"\n"
			"First document\n"
			"\n"
			"--aaa--\n",

			"FAXRES=\n"
			"---\n"
			"First document\n"
			"---\n"
			"FAXRES=\n"
			"FROM=sender@испытание\n"
			"TO=receipient@испытание\n"
			"SUBJECT=Sample fax\n"
			"DATE=\n"
			"PAGES=2\n"
			"\n"
			"This is a sample fax\n",
		},

		// Test 2
		{
			"Mime-Version: 1.0\n"
			"Content-Type: multipart/signed; boundary=aaa\n"
			"From: =?UTF-8?B?0LTQsNC00LDQtNCw?= <sender@example.com>\n"
			"To: Recipient receipient@example.com\n"
			"Subject: Multipart/signed test\n"
			"\n"
			"\n"
			"MIME content follows\n"
			"--aaa\n"
			"Content-Type: text/plain; charset=utf-8\n"
			"\n"
			"First document\n"
			"\n"
			"--aaa\n"
			"Content-Type: application/pgp-signature\n"
			"\n"
			"Signature\n"
			"--aaa--\n",

			"FAXRES=\n"
			"FROM=дадада\n"
			"TO=Recipient, receipient@example.com\n"
			"SUBJECT=Multipart/signed test\n"
			"DATE=\n"
			"PAGES=0\n"
			"\n"
			"First document\n",
		},

		// Test 3
		{
			"Mime-Version: 1.0\n"
			"Content-Type: multipart/mixed; boundary=aaa;\n"
			"    start=\"<main@example.com>\"\n"
			"From: sender@example.com\n"
			"To: recipient@example.com\n"
			"Subject: Multipart/related test\n"
			"\n"
			"--aaa\n"
			"Content-Type: multipart/related; boundary=bbb;\n"
			"    start=\"<main@example.com>\"\n"
			"\n"
			"\n"
			"MIME content follows\n"
			"\n"
			"--bbb\n"
			"Content-Type: image/gif\n"
			"Content-Id: <image@example.com>\n"
			"\n"
			"Image\n"
			"--bbb\n"
			"Content-Type: text/plain; charset=utf-8\n"
			"Content-Id: <main@example.com>\n"
			"\n"
			"First document\n"
			"\n"
			"--bbb--\n"
			"--aaa\n"
			"Content-Type: text/plain; charset=utf-8\n"
			"\n"
			"Second document\n"
			"\n"
			"--aaa--\n",

			"FAXRES=\n"
			"---\n"
			"Second document\n"
			"---\n"
			"FAXRES=\n"
			"FROM=sender@example.com\n"
			"TO=recipient@example.com\n"
			"SUBJECT=Multipart/related test\n"
			"DATE=\n"
			"PAGES=2\n"
			"\n"
			"First document\n",
		},

		// Test 4
		{
			"Mime-Version: 1.0\n"
			"Content-Type: multipart/mixed; boundary=aaa;\n"
			"    start=\"<main@example.com>\"\n"
			"From: sender@example.com\n"
			"To: recipient@example.com\n"
			"Subject: Multipart/related test\n"
			"\n"
			"--aaa\n"
			"Content-Type: multipart/related; boundary=bbb;\n"
			"    start=\"<main@example.com>\"\n"
			"\n"
			"\n"
			"MIME content follows\n"
			"\n"
			"--bbb\n"
			"Content-Type: image/gif\n"
			"Content-Id: <image@example.com>\n"
			"\n"
			"Image\n"
			"--bbb\n"
			"Content-Type: text/html; charset=utf-8\n"
			"Content-Id: <main@example.com>\n"
			"\n"
			"First document\n"
			"\n"
			"--bbb--\n"
			"--aaa\n"
			"Content-Type: text/plain; charset=utf-8\n"
			"\n"
			"Second document\n"
			"\n"
			"--aaa--\n",

			"FAXRES=\n"
			"---\n"
			"Second document\n"
			"---\n"
			"FAXRES=\n"
			"FROM=sender@example.com\n"
			"TO=recipient@example.com\n"
			"SUBJECT=Multipart/related test\n"
			"DATE=\n"
			"PAGES=2\n"
			"\n"
			"Document type \"text/html\" cannot be faxed\n"
			"----------------------------------------------------------------\n"
			"\n"
			"First document\n",
		},

		// Test 5
		{
			"Mime-Version: 1.0\n"
			"Content-Type: multipart/mixed; boundary=aaa;\n"
			"    start=\"<main@example.com>\"\n"
			"From: sender@example.com\n"
			"To: recipient@example.com\n"
			"Subject: Multipart/related test\n"
			"\n"
			"--aaa\n"
			"Content-Type: multipart/related; boundary=bbb;\n"
			"    start=\"<main@example.com>\"\n"
			"\n"
			"\n"
			"MIME content follows\n"
			"\n"
			"--bbb\n"
			"Content-Type: image/gif\n"
			"Content-Id: <image@example.com>\n"
			"\n"
			"Image\n"
			"--bbb\n"
			"Content-Type: multipart/signed; boundary=ccc\n"
			"Content-Id: <main@example.com>\n"
			"\n"
			"--ccc\n"
			"Content-Type: text/plain; charset=utf-8\n"
			"\n"
			"First document\n"
			"\n"
			"--ccc\n"
			"Content-Type: application/pgp-signature\n"
			"\n"
			"Signature\n"
			"--ccc--\n"
			"--bbb--\n"
			"--aaa\n"
			"Content-Type: text/plain; charset=utf-8\n"
			"\n"
			"Second document\n"
			"\n"
			"--aaa--\n",

			"FAXRES=\n"
			"---\n"
			"Second document\n"
			"---\n"
			"FAXRES=\n"
			"FROM=sender@example.com\n"
			"TO=recipient@example.com\n"
			"SUBJECT=Multipart/related test\n"
			"DATE=\n"
			"PAGES=2\n"
			"\n"
			"First document\n",
		},
	};

	size_t testnum=0;

	// #define UPD_FAXCONVERT 1

	for (auto &t:tests)
	{
		++testnum;
		generated_fax.clear();
		{
			std::ofstream o{"testmessage"};

			o << t.message;
			o.close();
		}

		unsigned ncoverpages=0;

		if (faxconvert("testmessage", FAX_OKUNKNOWN, &ncoverpages))
		{
			std::cerr << "faxconvert test " << testnum
				  << ": returned error code.\n";
			exit(1);
		}

#if UPD_FAXCONVERT

		std::string_view str{t.message};

		std::cout << "\n\t\t// Test " << testnum << "\n"
			"\t\t{\n";

		const char *comma="";
		for (auto str : std::array<std::string_view, 2>{
				t.message,
				generated_fax})
		{
			std::cout << comma;
			comma=",\n\n";

			const char *eol="";

			do
			{
				std::cout << eol;
				eol="\n";
				auto nl=std::find(str.begin(), str.end(), '\n');

				if (nl != str.end()) ++nl;

				std::cout << "\t\t\t\"";

				for (char c:std::string_view{
						str.data(),
						static_cast<size_t>(
							nl-str.begin())})
				{
					switch (c) {
					case '\n':
						std::cout << "\\n";
						break;
					case '"':
					case '\\':
						std::cout << "\\" << c;
						break;
					default:
						std::cout << c;
					}
				}
				std::cout << "\"";
				eol="\n";

				str=str.substr(nl-str.begin(),
					       str.end()-nl);
			} while (str.size());
		}

		std::cout << ",\n"
			"\t\t},\n";
#else
		if (generated_fax != t.result)
		{
			std::cout << "testfaxconvert test " << testnum
				  << " failed\n"
				"Got:\n["
				  << generated_fax << "]\n\nExpected:\n["
				  << t.result
				  << "]\n";
			exit(1);
		}
#endif
	}
}

int main()
{
	try {
		setlocale(LC_ALL, "");
		std::locale::global( std::locale{""});
	} catch (std::exception &e)
	{
		std::cout << "Unable to set the default locale: "
			  << e.what() << "\n";
		exit(1);
	}

	testfuncsinit();
	testfaxconvert();
	return 0;
}
