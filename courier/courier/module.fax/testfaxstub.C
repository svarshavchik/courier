#include "config.h"

#include "faxstub.C"
#include "../libs/comfax.C"
#include <sstream>
#include <string_view>
#include <array>
#include <fstream>
#include <iostream>

void clog_msg_prerrno()
{
}

char *config_localfilename(const char *)
{
	return strdup("testfaxrc");
}

std::string result;

static void err_func(int n, const char *errmsg, struct rw_info *rw)
{
	std::ostringstream o;

	o << "Error: " << n << " " << errmsg;

	result=o.str();
}

static void nextfunc(struct rw_info *)
{
	result="ignored";
}

static void delfunc(struct rw_info *rw, const rfc822::tokens &host,
		    const rfc822::tokens &addr)
{
	result="deliver: ";

	host.print(std::back_inserter(result));
	result += ", addr: ";
	addr.print(std::back_inserter(result));
}

void testdel()
{
	static const struct {
		int mode;
		const char *addr;
		const char *smodule;
		const char *result;
	} tests[] = {

		// Test 1
		{
			0,
			"nobody@example.com",
			"local",
			"ignored",
		},

		// Test 2
		{
			RW_SUBMIT,
			"2125551212@fax",
			"local",
			"deliver: fax, addr: 12125551212",
		},

		// Test 3
		{
			RW_SUBMIT,
			"12125551212@fax",
			"esmtp",
			"deliver: fax, addr: 12125551212",
		},

		// Test 4
		{
			RW_SUBMIT,
			"2125551212@fax",
			"uucp",
			"Error: 550 Invalid fax phone number",
		},

		// Test 5
		{
			RW_SUBMIT,
			"5551212@fax",
			"local",
			"deliver: fax, addr: 5551212",
		},

		// Test 6
		{
			RW_SUBMIT,
			"9995551212@fax-lowres",
			"local",
			"deliver: fax-lowres, addr: 5551212",
		},

		// Test 7
		{
			RW_SUBMIT,
			"0117123456@fax",
			"local",
			"deliver: fax, addr: 0117123456",
		},

		// Test 8
		{
			RW_SUBMIT,
			"0117123456@fax",
			"esmtp",
			"Error: 550 Invalid fax phone number",
		},

		// Test 9
		{
			0,
			"0117123456@fax",
			"local",
			"deliver: fax, addr: 0117123456",
		},
	};

	// #define UPDTESTDEL 1

	size_t testnum=0;

	for (const auto &test:tests)
	{
		++testnum;

		rw_info rw{test.mode, {}, {}};

		rw.addr=std::string_view{test.addr};
		rw.err_func=err_func;
		rw.smodule=test.smodule;

		result="";

		rw_del_fax(&rw, nextfunc, delfunc);

#if UPDTESTDEL
		std::cout << "\n\t\t// Test " << testnum
			  << "\n\t\t{\n\t\t\t"
			  << (test.mode ? "RW_SUBMIT":"0") << ",\n";

		for (auto str: std::array<std::string_view, 3>{
				test.addr,
				test.smodule,
				result
			})
		{
			do
			{
				std::cout << "\t\t\t\"";

				size_t i;

				for (i=0; i<str.size(); ++i)
				{
					if (str[i] == '\n')
					{
						std::cout << "\\n";
						++i;
						break;
					}
					if (str[i] == '\\' || str[i] == '"')
					{
						std::cout << "\\";
					}
					std::cout << str[i];
				}
				std::cout << "\"";

				str=str.substr(i);

				if (!str.empty())
				{
					std::cout << "\n";
				}
			} while (!str.empty());

			std::cout << ",\n";
		}

		std::cout << "\t\t},\n";
#else
		if (result != test.result)
		{
			std::cerr << "rw_del_fax test " << testnum
				  << " failed:\n"
				  << "expected: " << test.result
				  << "\n     got: " << result << "\n";
			exit(1);
		}
#endif
	}
}

int main()
{
	{
		std::ofstream o{"testfaxrc"};

		o <<
			"rw^*    1               $9\n"
			"rw^     011             1011$9\n"
			"rw^     n11             1\n"
			"rw^     1011            011$9\n"
			"rw      (nnnnnnn)      999$1\n"
			"accept   2nnnnnnnnn   local\n"
			"accept   2nnnnnnnnn   esmtp\n"
			"accept   3nnnnnnnnn   local\n"
			"accept   3nnnnnnnnn   esmtp\n"
			"accept   4nnnnnnnnn   local\n"
			"accept   4nnnnnnnnn   esmtp\n"
			"accept   5nnnnnnnnn   local\n"
			"accept   5nnnnnnnnn   esmtp\n"
			"accept   6nnnnnnnnn   local\n"
			"accept   6nnnnnnnnn   esmtp\n"
			"accept   7nnnnnnnnn   local\n"
			"accept   7nnnnnnnnn   esmtp\n"
			"accept   8nnnnnnnnn   local\n"
			"accept   8nnnnnnnnn   esmtp\n"
			"accept   9nnnnnnnnn   local\n"
			"accept   9nnnnnnnnn   esmtp\n"
			"accept^ 011    local\n"
			"rw^    (.)       1$1$9\n"
			"rw^    1011      011$9\n"
			"rw^    1999      $9\n";

		o.close();
	}

	testdel();
	unlink("testfaxrc");
	return 0;
}
