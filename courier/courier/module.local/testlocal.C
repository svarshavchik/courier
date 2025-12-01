/*
** Copyright 2025 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif
#include	"courier.h"
#include	"dbobj.h"

#undef ALIASDIR
#define ALIASDIR	"testlocal.dir/aliases"

static void fake_exec_maildrop(const char *, char **);

#define EXEC_MAILDROP fake_exec_maildrop
#include "local.C"

#undef LOCAL_EXTENSIONS
#define LOCAL_EXTENSIONS 1

extern "C" {
#include "dotcourier.c"
}

#define ACE_DOMAIN "example.com"
#define I18N_DOMAIN "test.испытание"

#include "../libs/rwdefaulthost.C"

#include <sstream>
#include <iostream>
#include <fstream>
#include <string_view>
#include <filesystem>

void *courier_malloc(unsigned n)
{
	return malloc(n);
}

char *courier_strdup(const char *ptr)
{
	return strdup(ptr);
}

int gethostname_sys(char *buf, size_t l)
{
	strcpy(buf, "mail.example.com");
	return 0;
}

char	*config_localfilename(const char *str)
{
	char *p=reinterpret_cast<char *>(malloc(strlen(str)+20));

	strcat(strcpy(p, "testlocal.dir/"), str);
	return p;
}

const char *config_maildropfilter()
{
	return "/maildrop";
}

void clog_msg_errno()
{
        perror(strerror(errno));
        exit(1);
}


static std::string result;

int auth_getuserinfo(const char *service, const char *uid,
		     int (*callback)(struct authinfo *, void *),
		     void *arg)
{
	result += "lookup \"";
	result += uid;
	result += "\"\n";

	authinfo ret{};
	uid_t u=1000;

	ret.sysuserid=&u;
	ret.sysgroupid=1000;

	if (strcmp(uid, "home1") == 0)
	{
		ret.address="home1@example.com";
		ret.homedir="testlocal.dir/home1";
		ret.maildir="./Maildir";
		return callback(&ret, arg);
	}

	if (strcmp(uid, "home2-user@hosted.example.com") == 0)
	{
		ret.address="home2-user@hosted.example.com";
		ret.homedir="testlocal.dir/home2";
		ret.maildir="./Maildir";
		return callback(&ret, arg);
	}

	if (strcmp(uid, "alias@hosted.example.com") == 0)
	{
		ret.address="alias@hosted.example.com";
		ret.homedir="testlocal.dir/home2-alias";
		ret.maildir="./Maildir";
		return callback(&ret, arg);
	}

	return -1;
}

uid_t courier_mailuid()
{
	return geteuid();
}

gid_t courier_mailgid()
{
	return getegid();
}


static void err_func(int n, const char *errmsg, struct rw_info *rw)
{
	std::ostringstream o;

	o << "Error: " << n << " " << errmsg;

	result += o.str();
}

static void nextfunc(struct rw_info *rwi)
{
	result += "continue: ";

	rwi->addr.print(std::back_inserter(result));
}

static void delfunc(struct rw_info *rw, const rfc822::tokens &host,
		    const rfc822::tokens &addr)
{
	result += "deliver: ";

	host.print(std::back_inserter(result));
	result += ", addr: ";
	addr.print(std::back_inserter(result));
}

void testdel()
{
	static const struct {
		int mode;
		const char *smodule;
		const char *addr;
		const char *result;
	} tests[]={
		// Test 1
		{
			0,
			"local",
			"uucp!bang!path",
			"continue: uucp!bang!path"
		},

		// Test 2
		{
			0,
			"local",
			"remote@remote.example.com",
			"continue: remote@remote.example.com"
		},

		// Test 3
		{
			0,
			"local",
			"notfound@example.com",
			"lookup \"notfound\"\n"
			"Error: 550 User <notfound> unknown"
		},

		// Test 4
		{
			0,
			"local",
			"notfound@hosted.example.com",
			"lookup \"notfound@hosted.example.com\"\n"
			"lookup \"alias@hosted.example.com\"\n"
			"Error: 550 User <notfound@hosted.example.com> unknown"
		},

		// Test 5
		{
			0,
			"local",
			"home1@example.com",
			"lookup \"home1\"\n"
			"deliver: home1@example.com!!1000!1000!testlocal.dir/home1!./Maildir!, addr: home1"
		},

		// Test 6
		{
			0,
			"local",
			"home2-user@hosted.example.com",
			"lookup \"home2-user@hosted.example.com\"\n"
			"deliver: home2-user@hosted.example.com!!1000!1000!testlocal.dir/home2!./Maildir!, addr: home2-user@hosted.example.com"
		},

		// Test 7
		{
			0,
			"local",
			"home1-noext@example.com",
			"lookup \"home1-noext\"\n"
			"lookup \"home1\"\n"
			"Error: 550 User <home1-noext> unknown"
		},

		// Test 8
		{
			0,
			"local",
			"home1-mlm@example.com",
			"lookup \"home1-mlm\"\n"
			"lookup \"home1\"\n"
			"deliver: home1@example.com!mlm!1000!1000!testlocal.dir/home1!./Maildir!, addr: home1"
		},

		// Test 9
		{
			0,
			"local",
			"home1-mlm-unsubscribe@example.com",
			"lookup \"home1-mlm-unsubscribe\"\n"
			"lookup \"home1-mlm\"\n"
			"lookup \"home1\"\n"
			"deliver: home1@example.com!mlm-unsubscribe!1000!1000!testlocal.dir/home1!./Maildir!, addr: home1"
		},

		// Test 10
		{
			0,
			"local",
			"home2-user-noext@hosted.example.com",
			"lookup \"home2-user-noext@hosted.example.com\"\n"
			"lookup \"home2-user@hosted.example.com\"\n"
			"Error: 550 User <home2-user-noext@hosted.example.com> unknown"
		},

		// Test 11
		{
			0,
			"local",
			"home2-user-mlm@hosted.example.com",
			"lookup \"home2-user-mlm@hosted.example.com\"\n"
			"lookup \"home2-user@hosted.example.com\"\n"
			"deliver: home2-user@hosted.example.com!mlm!1000!1000!testlocal.dir/home2!./Maildir!, addr: home2-user@hosted.example.com"
		},

		// Test 12
		{
			0,
			"local",
			"home2-user-mlm-unsubscribe@hosted.example.com",
			"lookup \"home2-user-mlm-unsubscribe@hosted.example.com\"\n"
			"lookup \"home2-user-mlm@hosted.example.com\"\n"
			"lookup \"home2-user@hosted.example.com\"\n"
			"deliver: home2-user@hosted.example.com!mlm-unsubscribe!1000!1000!testlocal.dir/home2!./Maildir!, addr: home2-user@hosted.example.com"
		},

		// Test 13
		{
			0,
			"local",
			"address2@hosted.example.com",
			"lookup \"address2@hosted.example.com\"\n"
			"lookup \"alias@hosted.example.com\"\n"
			"deliver: alias@hosted.example.com!address2@hosted.example.com!1000!1000!testlocal.dir/home2-alias!./Maildir!, addr: alias@hosted.example.com"
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
		rw.smodule=test.smodule;
		result="";

		rw_del_local(&rw, nextfunc, delfunc);

#if UPDATE_DEL
		std::vector<const char *> flags;

		if (test.mode & RW_SUBMIT)
			flags.push_back("RW_SUBMIT");
		if (test.mode & RW_SUBMITALIAS)
			flags.push_back("RW_SUBMITALIAS");
		if (test.mode & RW_VERIFY)
			flags.push_back("RW_VERIFY");
		if (test.mode & RW_EXPN)
			flags.push_back("RW_EXPN");

		if (flags.empty())
			flags.push_back("0");

		std::cout << (testnum > 1 ? ",\n\n":"")
			  << "\t\t// Test " << testnum
			  << "\n\t\t{\n"
			"\t\t\t";

		const char *pfix="";
		for (auto &f:flags)
		{
			std::cout << pfix << f;
			pfix="|";
		}
		std::cout << ",\n"
			"\t\t\t";

		if (test.smodule)
			std::cout <<
				"\"" << test.smodule << "\"";
		else
			std::cout << "nullptr";

		std::cout << ",\n"
			"\t\t\t\"" << test.addr << "\",\n"
			"\t\t\t\"";

		for (char c:result)
		{
			if (c == '\n')
			{
				std::cout << "\\n\"\n\t\t\t\"";
			}
			else if (c == '"' || c == '\\')
				std::cout << "\\" << c;
			else
				std::cout << c;
		}
		std::cout << "\"\n"
			"\t\t}";
#else
		if (result != test.result)
		{
			std::cerr << "rw_del_local test " << testnum
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

static void fake_exec_maildrop(const char *ignore, char **argv)
{
	std::ofstream o{"testlocal.dir/maildropexec"};

	o << getenv("SENDER") << "\n"
	  << getenv("HOME") << "\n"
	  << getenv("DEFAULT") << "\n"
	  << getenv("MAILDIRQUOTA") << "\n"
	  << getenv("BOUNCE") << "\n"
	  << getenv("BOUNCE2") << "\n";

	while (*argv)
	{
		o << *argv << "\n";
		++argv;
	}
	o.close();
	exit(0);
}

void testfilter()
{
	char buf[256];

	setenv("BOUNCE", "bounce", 1);
	setenv("BOUNCE2", "bounce2", 1);
	setenv("TCPREMOTEHOST", "remotehost", 1);
	setenv("TCPREMOTEIP", "remoteip", 1);

	rw_local_filter("local", -1,
			"local!home1@example.com!1000!1000!testlocal.dir/home1!./Maildir!1000C",
			"recipient",
			"sender",
			buf,
			sizeof(buf));

	std::ifstream i{"testlocal.dir/maildropexec"};
	std::string result{std::istreambuf_iterator<char>{i.rdbuf()},
			   std::istreambuf_iterator<char>{}};

	if (result !=
	    "sender\n"
	    "testlocal.dir/home1\n"
	    "./Maildir\n"
	    "1000C\n"
	    "bounce\n"
	    "bounce2\n"
	    "maildrop\n"
	    "-D\n"
	    "1000/1000\n"
	    "-M\n"
	    "rcptfilter-home1@example.com\n"
	    "remotehost\n"
	    "remoteip\n"
	    "sender\n"
	    "bounce\n"
	    "bounce2\n")
	{
		std::cout << "testfilter failed:\n" << result;
		exit(1);
	}
}

void testrw()
{
	static const struct {
		int mode;
		const char *addr;
		const char *result;
	} tests[]={
		// Test 1
		{
			RW_SUBMIT|RW_ENVSENDER,
			"",
			"continue: "
		},

		// Test 2
		{
			RW_SUBMIT|RW_ENVSENDER,
			"uucp!bangpath",
			"continue: uucp!bangpath"
		},

		// Test 3
		{
			RW_SUBMIT|RW_ENVSENDER,
			"user@example.com",
			"continue: user@example.com"
		},

		// Test 4
		{
			RW_SUBMIT|RW_ENVSENDER,
			"user",
			"continue: user@test.испытание"
		},

		// Test 5
		{
			RW_SUBMIT|RW_HEADER|RW_ENVSENDER,
			"user",
			"continue: user@example.com"
		}
	};
	size_t testnum=0;

	// #define UPDATE_RW 1

	for (auto &test:tests)
	{
		++testnum;

		rw_info rw{test.mode, {}, {}};

		rw.addr=std::string_view{test.addr};
		rw.err_func=err_func;
		rw.smodule="local";
		result="";

		rw_local(&rw, nextfunc);

#if UPDATE_RW
		std::vector<const char *> flags;

		if (test.mode & RW_SUBMIT)
		{
			flags.push_back("RW_SUBMIT");
		}
		if (test.mode & RW_HEADER)
		{
			flags.push_back("RW_HEADER");
		}
		if (test.mode & RW_ENVSENDER)
		{
			flags.push_back("RW_ENVSENDER");
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
			"\t\t\t\"" << result << "\"\n"
			"\t\t}";
#else
		if (result != test.result)
		{
			std::cerr << "rw_local test " << testnum
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

	std::filesystem::remove_all("testlocal.dir", ec);
	mkdir("testlocal.dir", 0777);
	mkdir("testlocal.dir/aliases", 0777);

	for (auto &[name, value] : std::array<
		     std::tuple<std::string_view, std::string_view>, 1>{{
			     {"locals", "mail.example.com\nexample.com\n"},
		     }})
	{
		std::string filename{"testlocal.dir/"};

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
			   "testlocal.dir/hosteddomains.dat",
			   "n");
		dbobj_store(&hosteddomains,
			    record.data(),
			    record.size(),
			    "",
			    0,
			    "I");
		dbobj_close(&hosteddomains);
	}

	mkdir("testlocal.dir/home1", 0777);
	std::ofstream{"testlocal.dir/home1/.courier-mlm"};
	std::ofstream{"testlocal.dir/home1/.courier-mlm-default"};
	mkdir("testlocal.dir/home2", 0777);
	std::ofstream{"testlocal.dir/home2/.courier-mlm"};
	std::ofstream{"testlocal.dir/home2/.courier-mlm-default"};
	mkdir("testlocal.dir/home2-alias", 0777);
	std::ofstream{
		"testlocal.dir/home2-alias/.courier-address2@hosted:example:com"
	};
	testdel();
	testfilter();
	testrw();

	std::filesystem::remove_all("testlocal.dir", ec);
	return 0;
}
