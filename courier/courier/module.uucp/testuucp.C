#include "uucpstub.C"
#include "courier.h"
#include <sstream>
#include <iostream>
#include <vector>

#define ACE_DOMAIN	"xn--80akhbyknj4f"
#define I18N_DOMAIN	"испытание"
#include "../libs/rwdefaulthost.C"

int uucprewriteheaders()
{
	return 1;
}

const char *uucpme()
{
	return "example";
}

void uucpneighbors_init()
{
}

bool configt_islocal(rfc822::tokens::iterator b, rfc822::tokens::iterator e,
		     std::string &domain)
{
	std::string s;

	rfc822::tokens::print(b, e, std::back_inserter(s));

	if (s == "local.example.com")
		return true;

	if (s == "hosted.example.com")
		return true;

	if (s == "mail.hosted.example.com")
	{
		domain="hosted.example.com";
		return true;
	}

	return false;
}

char	*uucpneighbors_fetch(const char *key, size_t keylen,
			     size_t *retlen, const char *opts)
{
	std::string_view who{key, keylen};

	*retlen=0;

	if (who == "zilch")
		return strdup("");

	*retlen=1;

	if (who == "food")
		return strdup("F");

	if (who == "green")
		return strdup("G");

	if (who == "rose")
		return strdup("R");

	*retlen=0;
	return nullptr;
}

static std::string result;

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

static void rw_nextfunc(struct rw_info *rwi)
{
	result="result: ";

	rwi->addr.print(std::back_inserter(result));
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
		const char *addr;
		const char *result;
	} tests[]={
		// Test 1
		{
			"localaddr",
			"ignored"
		},

		// Test 2
		{
			"zilch",
			"ignored"
		},

		// Test 3
		{
			"zilch@local.example.com",
			"ignored"
		},

		// Test 4
		{
			"zilch!addr",
			"deliver: zilch, addr: addr"
		},

		// Test 5
		{
			"zilch!first.last",
			"deliver: zilch, addr: first.last"
		},

		// Test 6
		{
			"zilch!more!addr",
			"Error: 550 Unknown UUCP bang path."
		},

		// Test 7
		{
			"!zilch!addr",
			"Error: 550 Invalid UUCP bang path."
		},

		// Test 8
		{
			"zilch!addr!",
			"Error: 550 Invalid UUCP bang path."
		},

		// Test 9
		{
			"zilch!!addr",
			"Error: 550 Invalid UUCP bang path."
		},

		// Test 10
		{
			"food!banana@local.example.com",
			"deliver: food, addr: banana"
		},

		// Test 11
		{
			"food!fruits!banana@mail.hosted.example.com",
			"deliver: food!fruits, addr: banana"
		},

		// Test 12
		{
			"green!its!drink",
			"deliver: green!its, addr: drink"
		},

		// Test 13
		{
			"rose!byanyother!name",
			"deliver: rose, addr: byanyother!name"
		},

		// Test 14
		{
			"green!its|drink@anotherdomain.example.com",
			"ignored"
		}
	};

	size_t testnum=0;

	//#define UPDATE_DEL 1

	for (auto &test:tests)
	{
		++testnum;

		rw_info rw{0, {}, {}};

		rw.addr=std::string_view{test.addr};
		rw.err_func=err_func;
		rw.smodule="local";
		result="";

		rw_del_uucp(&rw, nextfunc, delfunc);

#if UPDATE_DEL
		std::cout << (testnum > 1 ? ",\n\n":"")
			  << "\t\t// Test " << testnum
			  << "\n\t\t{\n"
			"\t\t\t\"" << test.addr << "\",\n"
			"\t\t\t\"" << result << "\"\n"
			"\t\t}";
#else
		if (result != test.result)
		{
			std::cerr << "rw_del_uucp test " << testnum
				  << " failed:\n"
				  << "    Expected: " << test.result << "\n"
				  << "         Got: " << result << "\n";
			exit(1);
		}
#endif
	}

#if UPDATE_DEL
	std::cout << "\n";
#endif
}

void testrw()
{
	static const struct {
		int mode;
		const char *addr;
		const char *host;
		const char *result;
	} tests[]={
		// Test 1
		{
			0,
			"example!zilch!addr",
			"",
			"result: zilch!addr"
		},

		// Test 2
		{
			0,
			"zilch!addr",
			"",
			"result: zilch!addr"
		},

		// Test 3
		{
			0,
			"addr",
			"",
			"result: addr@испытание"
		},

		// Test 4
		{
			RW_HEADER,
			"addr",
			"",
			"result: addr@xn--80akhbyknj4f"
		},

		// Test 5
		{
			0,
			"green!its!drink",
			"",
			"result: green!its!drink"
		},

		// Test 6
		{
			0,
			"relay.example.com!user",
			"",
			"result: user@relay.example.com"
		},

		// Test 7
		{
			RW_OUTPUT,
			"user",
			"",
			"result: example!user"
		},

		// Test 8
		{
			RW_OUTPUT,
			"user@relay.example.com",
			"",
			"result: example!relay.example.com!user"
		},

		// Test 9
		{
			RW_OUTPUT,
			"user@relay.example.com",
			"",
			"result: example!relay.example.com!user"
		},

		// Test 10
		{
			RW_OUTPUT,
			"green!its!another!drink",
			"green!its!another",
			"result: drink"
		}
	};
	size_t testnum=0;

	// #define UPDATE_RW 1

	for (auto &test:tests)
	{
		++testnum;

		rw_info rw{test.mode, {}, std::string_view{test.host}};

		rw.addr=std::string_view{test.addr};
		rw.err_func=err_func;
		rw.smodule="local";
		result="";

		rw_uucp(&rw, rw_nextfunc);

#if UPDATE_RW
		std::vector<const char *> flags;

		if (test.mode & RW_OUTPUT)
		{
			flags.push_back("RW_OUTPUT");
		}
		if (test.mode & RW_HEADER)
		{
			flags.push_back("RW_HEADER");
		}

		if (flags.empty())
			flags.push_back("0");

		std::cout << (testnum > 1 ? ",\n\n":"")
			  << "\t\t// Test " << testnum
			  << "\n\t\t{\n"
			"\t\t\t";

		const char *sep="";

		for (auto f:flags)
		{
			std::cout << sep << f;
			sep="|";
		}

		std::cout << ",\n"
			"\t\t\t\"" << test.addr << "\",\n"
			"\t\t\t\"" << test.host << "\",\n"
			"\t\t\t\"" << result << "\"\n"
			"\t\t}";
#else
		if (result != test.result)
		{
			std::cerr << "rw_uucp test " << testnum
				  << " failed:\n"
				  << "    Expected: " << test.result << "\n"
				  << "         Got: " << result << "\n";
			exit(1);
		}
#endif
	}

#if UPDATE_RW
	std::cout << "\n";
#endif
}

int main()
{
	testdel();
	testrw();
	return 0;
}
