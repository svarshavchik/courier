/*
** Copyright 2025 S. Varshavchik.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	<stdlib.h>
#include	<string.h>
#include	"cme.C"
#include	"cread1l.c"
#include	"cgethostname.c"
#include	"addrlower.c"
#include	"islocal.c"
#include	"readfile.c"
#include	"removecomments.c"
#include	"cdomaincmp.c"
#include	"comesmtpidstring.c"
#include	<iostream>
#include	<fstream>
#include	<filesystem>
#include	<unistd.h>
#include	<sys/stat.h>

void *courier_malloc(unsigned n)
{
	return malloc(n);
}

char *courier_strdup(const char *ptr)
{
	return strdup(ptr);
}

char	*config_localfilename(const char *f)
{
	char *p=(char *)malloc(strlen(f)+20);

	return strcat(strcpy(p, "testme.dir/"), f);
}

void clog_msg_errno()
{
	perror(strerror(errno));
	exit(1);
}

int gethostname_sys(char *buf, size_t l)
{
	strncpy(buf, "JACK", l);
	return 0;
}

int main(int argc, char **argv)
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
	std::error_code ec;

	std::filesystem::remove_all("testme.dir", ec);

	mkdir("testme.dir", 0777);

	std::ofstream o{"testme.dir/me"};
	o << "*.испытание";
	o.close();

	if (strcmp(config_me_ace(), "jack.xn--80akhbyknj4f") ||
	    strcmp(config_me(), "jack.испытание") ||
	    strcmp(config_esmtpgreeting(), "jack.xn--80akhbyknj4f ESMTP"))
	{
		std::cerr << "Unexpected results:\n"
			  << config_me_ace() << "\n"
			  << config_me() << "\n"
			  << config_esmtpgreeting() << "\n";
		exit(1);
	}

	if (!config_is_indomain("jack.xn--80akhbyknj4f",
				"example.com\n"
				"jack.испытание\n"))
	{
		std::cerr << "config_is_indomain test failed\n";
		exit(1);
	}
	std::filesystem::remove_all("testme.dir", ec);

	return 0;
}
