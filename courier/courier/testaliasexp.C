/*
** Copyright 2025 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"comuidgid.h"
#include	<fstream>
#include	<sstream>
#include	<unordered_map>

#include	<errno.h>
#include	<string.h>
#include	<filesystem>
#include	<courierauth.h>

#define LOGFILE "testaliasexp.dir/testaliasexp.log"

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

template<typename closure>
void log(closure &&func)
{
	std::ofstream o{LOGFILE, std::ios::app};

	func(o);
	o.close();
}

char	*config_localfilename(const char *str)
{
	char *p=reinterpret_cast<char *>(malloc(strlen(str)+20));

	strcat(strcpy(p, "testaliasexp.dir/"), str);
	return p;
}

void clog_msg_start_info()
{
}

void clog_msg_start_err()
{
}

void clog_msg_str(const char *str)
{
	log([&]
	    (auto &o)
	{
		o << str;
	});
}

void clog_msg_int(int n)
{
	log([&]
	    (auto &o)
	{
		o << n;
	});
}

void clog_msg_uint(unsigned n)
{
	log([&]
	    (auto &o)
	{
		o << n;
	});
}

void clog_msg_send()
{
	log([&]
	    (auto &o)
	{
		o << "\n";
	});
}

void clog_msg_errno()
{
	log([&]
	    (auto &o)
	{
		o << "system error: " << strerror(errno) << "\n";
	});

}

#include	"aliasexp2.C"

extern struct rw_list *local_rw_install(const struct rw_install_info *p);

int auth_getuserinfo(const char *service, const char *uid,
		     int (*callback)(struct authinfo *, void *),
		     void *arg)
{
	authinfo ret{};
	uid_t u=getuid();

	ret.sysuserid=&u;
	ret.sysgroupid=getgid();

	if (strcmp(uid, "home1") == 0)
	{
		ret.address="home1@mail.example.com";
		ret.homedir="testaliasexp.dir/home1";
		ret.maildir="./Maildir";
		return callback(&ret, arg);
	}

	if (strcmp(uid, "home2") == 0)
	{
		ret.address="home2@mail.example.com";
		ret.homedir="testaliasexp.dir/home2";
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

int main(int argc, char **argv)
{
	std::error_code ec;

	std::filesystem::remove_all("testaliasexp.dir", ec);
	mkdir("testaliasexp.dir", 0777);
	mkdir("testaliasexp.dir/tmp", 0777);

	tmpdir="testaliasexp.dir/tmp";
	xpfix="aliastmp-";

	clog_msg_str("Log start:\n");

	std::string pwd=std::filesystem::current_path(ec);

	rw_transport local_transport{};

	local_transport.rw_ptr=local_rw_install(nullptr);

	{
		std::ofstream o{"testaliasexp.dir/aliases"};

		o << "postmaster: home1,\n"
			"  home2\n"
			"root: home1\n"
			"bin: home2\n"
			"adm: :include:" << pwd
		  << "/testaliasexp.dir/admalias\n"
		  << "sys: " << pwd << "/testaliasexp.dir/sys\n"
		  << "ext: | " << pwd << "/testaliasexp.dir/ext\n";
		o.close();
	}

	{
		std::ofstream o{"testaliasexp.dir/admalias"};

		o << "addr1, addr2\n" "addr3\n";
		o.close();
	}
	std::ifstream i{"testaliasexp.dir/aliases"};
	std::ostringstream o;

	if (aliasexp(i, o, &local_transport))
	{
		std::cerr << "aliasexp failed\n";
		exit(1);
	}

	if (o.str() !=
	    "<postmaster@mail.example.com>\n"
	    "<home1@mail.example.com>\n"
	    "<home2@mail.example.com>\n"
	    "\n"
	    "<root@mail.example.com>\n"
	    "<home1@mail.example.com>\n"
	    "\n"
	    "<bin@mail.example.com>\n"
	    "<home2@mail.example.com>\n"
	    "\n"
	    "<adm@mail.example.com>\n"
	    "<addr1@mail.example.com>\n"
	    "<addr2@mail.example.com>\n"
	    "<addr3@mail.example.com>\n"
	    "\n"
	    "<sys@mail.example.com>\n"
	    "<\"aliastmp-sys@mail+2eexample+2ecom\"@mail.example.com>\n"
	    "\n"
	    "<ext@mail.example.com>\n"
	    "<\"aliastmp-ext@mail+2eexample+2ecom\"@mail.example.com>\n"
	    "\n"
	    ".\n"
	)
	{
		std::cerr << "Unexpected alias expansion:\n" << o.str();
		exit(1);
	}
	std::unordered_map<std::string, std::string> tmpcontents, expected;

	for (auto &entry : std::filesystem::directory_iterator{
			"testaliasexp.dir/tmp", ec
		})
	{
		std::string path{entry.path()};

		std::ifstream i{path};

		tmpcontents.emplace(
			path,
			std::string{std::istreambuf_iterator<char>{i.rdbuf()},
				    std::istreambuf_iterator<char>{}}
		);
	}

	expected.emplace("testaliasexp.dir/tmp/ext@mail+2eexample+2ecom",
			 "| " + pwd + "/testaliasexp.dir/ext\n");
	expected.emplace("testaliasexp.dir/tmp/sys@mail+2eexample+2ecom",
			 pwd + "/testaliasexp.dir/sys\n");

	if (tmpcontents != expected)
	{
		std::cerr << "unexpected contents of testaliasexp.dir/tmp\n"
			  << "expected:\n";

		for (auto &[key,val]:expected)
		{
			std::cerr << key << ": " << val;
		}
		exit(1);
	}
	std::filesystem::remove_all("testaliasexp.dir", ec);
	return 0;
}
