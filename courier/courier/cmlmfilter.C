/*
** Copyright 2018 S. Varshavchik.
** See COPYING for distribution information.
*/

#include	"config.h"
#include	"cmlm.h"
#include	"cmlmfilter.h"
#include	<string>
#include	<string.h>
#include	<sysexits.h>

int rcptfilter_msg()
{
	std::cerr << "200 Ok" << std::endl;
	return 99;
}

struct savemsg_ignore : public savemsg_sink {

	std::string last_error;
	bool have_error;

	savemsg_ignore() : have_error(false)
	{
	}

	void saveline(const std::string &) override
	{
	}

	void error(const std::string &errmsg) override
	{
		if (have_error)
			std::cerr << "550-" << last_error << std::endl;

		have_error=true;
		last_error=errmsg;
	}
};

int smtpfilter_msg()
{
	savemsg_ignore smi;

	int rc=savemsg(std::cin, smi);

	if (smi.have_error)
	{
		std::cerr << "550 " << smi.last_error << std::endl
			  << std::flush;
	}

	return rc;
}

static std::string getcmd()
{
	const char *mailfilter_env=getenv("MAILFILTER");

	if (!mailfilter_env)
		mailfilter_env="";

	const char *prefix_env=getenv("FILENAMEPREFIX");

	if (!prefix_env)
		prefix_env="";

	std::string mailfilter_s(mailfilter_env);
	std::string prefix_s(prefix_env);

	prefix_s += "-";

	if (mailfilter_s.substr(0, prefix_s.size()) == prefix_s)
	{
		mailfilter_s=mailfilter_s.substr(prefix_s.size());
	}
	else
	{
		mailfilter_s="";
	}

	return mailfilter_s;
}

int rcptfilter_ctlmsg()
{
	std::string cmd=getcmd();

	std::cerr << "200 Ok" << std::endl;

	if (is_cmd(cmd.c_str(), "subscribe") ||
	    is_cmd(cmd.c_str(), "alias-subscribe"))
		return 99;

	return 0;
}

int smtpfilter_ctlmsg()
{
	std::string cmd=getcmd();
	std::string msg(readmsg());

	std::string addr;

	{
		const char *address_cstr;

		if ((address_cstr=is_cmd(cmd.c_str(), "subscribe")) != 0)
		{
			addr=returnaddr(msg, address_cstr);
		}
		else if ((address_cstr=is_cmd(cmd.c_str(), "alias-subscribe"))
			 != 0)
		{
			addr=extract_alias_to_subscribe(msg);
		}
		else
		{
			std::cerr << "200 Ok" << std::endl;
			return 0;
		}
	}

	uaddrlower(addr);

	if (cmdget_s("UNICODE") != "1")
	{
		std::string::const_iterator b=addr.begin(), e=addr.end();

		while (b != e)
		{
			if (*b & 0x80)
			{
				std::cerr << "550 This mailing list does not "
					"accept Unicode E-mail addresses, yet."
					  << std::endl;
				return EX_SOFTWARE;
			}
			++b;
		}
	}

	std::cerr << "200 Ok" << std::endl;
	return 0;
}
