/*
** Copyright 1998 - 2002 S. Varshavchik.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif
#include	"courier.h"
#include	<sys/stat.h>
#include	<stdlib.h>
#include	<fcntl.h>

#include	<filesystem>
#include	<tuple>
#include	<array>
#include	<string_view>
#include	<fstream>
#include	<iostream>
#include	<map>
#include	<string>

#define GETENV get_fakeenv

std::unordered_map<std::string_view, std::string> fakeenv;

const char *get_fakeenv(const char *var)
{
	auto iter=fakeenv.find(var);

	if (iter == fakeenv.end())
		return NULL;

	return iter->second.c_str();
}

#include "esmtp.C"
#include "../libs/comfax.C"

void *courier_malloc(unsigned n)
{
	return malloc(n);
}

char *courier_strdup(const char *s)
{
	return strdup(s);
}

void clog_msg_errno()
{
	perror(strerror(errno));
	exit(1);
}

int gethostname_sys(char *buf, size_t l)
{
	strcpy(buf, "mail.example.com");
	return 0;
}

char	*config_localfilename(const char *str)
{
	char *p=reinterpret_cast<char *>(malloc(strlen(str)+20));

	strcat(strcpy(p, "testesmtp.dir/"), str);
	return p;
}

static std::string result;

static void err_func(int n, const char *errmsg, struct rw_info *rw)
{
	std::ostringstream o;

	o << "Error: " << n << " " << errmsg;

	result=o.str();
}

static void nextfunc(struct rw_info *rwi)
{
	result="continue: ";

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
		int mode;
		const char *addr;
		const char *result;
	} tests[]={
		// Test 1
		{
			0,
			"localaddr",
			"continue: localaddr"
		},

		// Test 2
		{
			0,
			"localaddr@example.com",
			"continue: localaddr@example.com"
		},

		// Test 3
		{
			0,
			"localaddr@mail.example.com",
			"continue: localaddr@mail.example.com"
		},

		// Test 4
		{
			RW_VERIFY,
			"uucp!bang!path@remote.example.com",
			"continue: uucp!bang!path@remote.example.com"
		},

		// Test 5
		{
			0,
			"remote@remote.example.com",
			"deliver: remote.example.com, addr: remote@remote.example.com"
		},

		// Test 6
		{
			RW_VERIFY,
			"localaddr@example.com",
			"continue: localaddr@example.com"
		},

		// Test 7
		{
			RW_VERIFY,
			"remote@remote.example.com",
			"Error: 550 Remote address."
		}
	};

	size_t testnum=0;

	// #define UPDATE_DEL 1

	for (auto &test:tests)
	{
		++testnum;

		rw_info rw{test.mode, {}, {}};
		rw.addr=std::string_view{test.addr};
		rw.err_func=err_func;
		rw.smodule="esmtp";
		result="";

		rw_del_esmtp(&rw, nextfunc, delfunc);

#if UPDATE_DEL
		std::cout << (testnum > 1 ? ",\n\n":"")
			  << "\t\t// Test " << testnum
			  << "\n\t\t{\n"
			"\t\t\t" << (test.mode ? "RW_VERIFY":"0") << ",\n"
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
		std::array<std::tuple<const char *, const char *>, 4> envars;
		const char *result;
	} tests[]={
		// Test 1
		{
			RW_SUBMIT|RW_ENVSENDER,
			"",
			{},
			"continue: "
		},

		// Test 2
		{
			RW_SUBMIT|RW_ENVSENDER,
			"nobody@example.com",
			{},
			"continue: nobody@example.com"
		},

		// Test 3
		{
			RW_SUBMIT|RW_ENVSENDER,
			"nobody@example..com",
			{},
			"Error: 517 Syntax error."
		},

		// Test 4
		{
			RW_SUBMIT|RW_ENVSENDER,
			"2125551212@fax",
			{},
			"Error: 517 Syntax error."
		},

		// Test 5
		{
			RW_SUBMIT|RW_ENVRECIPIENT,
			"2125551212@fax",
			{},
			"Error: 513 Syntax error."
		},

		// Test 6
		{
			RW_SUBMIT|RW_ENVRECIPIENT,
			"2125551212@fax",
			{
				{
					{"FAXRELAYCLIENT", ""}
				}
			},
			"Error: 513 Relaying denied."
		},

		// Test 7
		{
			RW_SUBMIT|RW_ENVRECIPIENT,
			"2125551212@fax",
			{
				{
					{"RELAYCLIENT", ""},
					{"FAXRELAYCLIENT", ""}
				}
			},
			"continue: 2125551212@fax"
		},

		// Test 8
		{
			RW_SUBMIT|RW_ENVRECIPIENT,
			"admin@example.com",
			{},
			"continue: admin@example.com"
		},

		// Test 9
		{
			RW_SUBMIT|RW_ENVRECIPIENT,
			"admin@mail.example.com",
			{},
			"Error: 513 Relaying denied."
		},

		// Test 10
		{
			RW_SUBMIT|RW_ENVRECIPIENT,
			"admin@mail.example.com",
			{
				{
					{"RELAYCLIENT", ""}
				}
			},
			"continue: admin@mail.example.com"
		},

		// Test 11
		{
			0,
			"local%gateway.example.com@mail.example.com",
			{},
			"continue: local@gateway.example.com"
		},

		// Test 12
		{
			0,
			"local%notgateway.example.com@mail.example.com",
			{},
			"continue: local%notgateway.example.com@mail.example.com"
		},

		// Test 13
		{
			RW_OUTPUT,
			"",
			{},
			"continue: "
		},

		// Test 14
		{
			RW_OUTPUT,
			"local",
			{},
			"continue: local@mail.example.com"
		},

		// Test 15
		{
			RW_OUTPUT,
			"local@gateway.example.com",
			{},
			"continue: local%gateway.example.com@mail.example.com"
		}
	};
	size_t testnum=0;

	// #define UPDATE_RW 1

	for (auto &test:tests)
	{
		++testnum;
		fakeenv.clear();
		for (auto &[name, value] : test.envars)
		{
			if (name && value)
				fakeenv.emplace(name, value);
		}

		rw_info rw{test.mode, {}, {}};
		rw.addr=std::string_view{test.addr};
		rw.err_func=err_func;
		rw.smodule="esmtp";
		result="";

		rw_esmtp(&rw, nextfunc);

#if UPDATE_RW
		std::vector<const char *> flags;

		if (test.mode & RW_SUBMIT)
		{
			flags.push_back("RW_SUBMIT");
		}
		if (test.mode & RW_OUTPUT)
		{
			flags.push_back("RW_OUTPUT");
		}
		if (test.mode & RW_ENVSENDER)
		{
			flags.push_back("RW_ENVSENDER");
		}
		if (test.mode & RW_ENVRECIPIENT)
		{
			flags.push_back("RW_ENVRECIPIENT");
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
			"\t\t\t{";

		const char *arr_pfix="\n\t\t\t\t{";
		const char *arr_sfix="";

		for (auto &[name, value] : test.envars)
		{
			if (name && value)
			{
				std::cout << arr_pfix << "\n\t\t\t\t\t{\""
					  << name << "\", \""
					  << value << "\"}";
				arr_pfix=",";
				arr_sfix="\n\t\t\t\t}\n\t\t\t";
			}
		}

		std::cout << arr_sfix << "},\n"
			"\t\t\t\"" << result << "\"\n"
			"\t\t}";
#else
		if (result != test.result)
		{
			std::cerr << "rw_esmtp test " << testnum
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

int main(int argc, char **argv)
{
	std::error_code ec;

	std::filesystem::remove_all("testesmtp.dir", ec);
	mkdir("testesmtp.dir", 0777);
	for (auto &[name, value] : std::array<
		     std::tuple<std::string_view, std::string_view>, 3>{{
			     {"esmtppercentrelay", "gateway.example.com\n"},
			     {"esmtpacceptmailfor", "example.com\n"},
			     {"locals", "mail.example.com\nexample.com\n"},
		     }})
	{
		std::string filename{"testesmtp.dir/"};

		filename += name;

		std::ofstream o{filename};
		o << value;
		o.close();
	}

	{
		dbobj hosteddomains;

		dbobj_init(&hosteddomains);

		std::string_view record{"hosted.example.com"};
		dbobj_open(&hosteddomains,
			   "testesmtp.dir/hosteddomains.dat",
			   "n");
		dbobj_store(&hosteddomains,
			    record.data(),
			    record.size(),
			    "",
			    0,
			    "I");
		dbobj_close(&hosteddomains);
	}

	testdel();
	testrw();
	std::filesystem::remove_all("testesmtp.dir", ec);
	return 0;
}
