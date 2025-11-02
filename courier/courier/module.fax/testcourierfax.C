#include "config.h"
#include "comctlfile.h"
#include <stdlib.h>
#include "numlib/numlib.h"
#include "../libs/comfax.C"
#include <fstream>
#include <errno.h>
#include <array>
#include <string_view>

#define FILTERBINDIR	"testfilterbin"
#define FAXTMPDIR	"testfaxtmp"

#define LOGFILE	"testcourierfax.log"

#define libmail_changeuidgid(a, b) do { } while(0)

#include "courierfax2.C"
#include "faxconvert.C"
#include "testfuncs.h"

#define TESTFILENAME "testcourierfax.file"

template<typename closure>
void log(closure &&func)
{
	std::ofstream o{LOGFILE, std::ios::app};

	func(o);
	o.close();
}

void invoke_sendfax(char **argvec)
{
	log([&]
	    (auto &o)
	{
		if (*argvec)
			++argvec;

		o << "sendfax";
		while (*argvec)
		{
			o << " " << *argvec;
			++argvec;
		}
		o << "\n";
	});
	exit(0);
}

void ctlfile_append_connectioninfo(struct ctlfile *,
				   unsigned, char, const char *msg)
{
	log([&]
	    (auto &o)
	{
		o << "connection: " << msg << "\n";
	});
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

int ctlfile_openi(ino_t, struct ctlfile *, int)
{
	return 0;
}

void ctlfile_close(struct ctlfile *)
{
}

void ctlfile_append_reply(struct ctlfile *, unsigned,
			  const char *msg, char, const char *)
{
	log([&]
	    (auto &o)
	{
		o << msg << "\n";
	});
}

const char *qmsgsdatname(ino_t)
{
	return TESTFILENAME;
}

void courierfax_failed(unsigned pages_sent,
		       struct ctlfile *ctf, unsigned nreceip,
		       int errcode, const char *errmsg)
{
	log([&]
	    (auto &o)
	{
		o << errmsg << "\n";
	});
}

void testcourierfax()
{
	const struct {
		const char *sender;
		const char *host;
		const char *recipient;

		const char *message;
		const char *result;
	} tests[]={
		{
			"sender",
			"fax",
			"2125551212@fax",
			"Mime-Version: 1.0\n"
			"Content-Type: text/plain; charset=utf-8\n"
			"\n"
			"Test message\n",
			"sendfax -v -r 2125551212@fax testfaxtmp/f0000-0001.g3\n"
			"1 pages - including 1 cover page(s) - sent by fax.\n",
		},
		{
			"sender",
			"fax-lowres-ignore",
			"2125551212@fax-lowres-ignore",
			"Mime-Version: 1.0\n"
			"Content-Type: multipart/mixed; boundary=aaa\n"
			"\n"
			"--aaa\n"
			"Content-Type: text/plain; charset=utf-8\n"
			"\n"
			"Test message\n"
			"\n"
			"--aaa\n"
			"Content-Type: text/plain; charset=utf-8\n"
			"\n"
			"First page\n"
			"\n"
			"--aaa\n"
			"Content-Type: application/octet-stream\n"
			"\n"
			"binary\n"
			"\n"
			"--aaa--\n",
			"sendfax -v -r -n 2125551212@fax-lowres-ignore testfaxtmp/f0000-0001.g3 testfaxtmp/f0001-0001.g3 testfaxtmp/f0001-0002.g3\n"
			"3 pages - including 1 cover page(s) - sent by fax.\n",
		},
	};

	// #define UPDTEST 1

	size_t testnum=0;

	for (auto &t:tests)
	{
		++testnum;

		unlink(LOGFILE);

		std::ofstream o{qmsgsdatname(0)};

		o << t.message;
		o.close();

		moduledel fakedel{};

		fakedel.msgid="test@example.com";
		fakedel.sender=t.sender;
		fakedel.delid="test@example.com";
		fakedel.host=t.host;
		fakedel.nreceipients=1;

		std::string recipient=t.recipient;

		char *recipient_str[2];
		char zero[]="0";
		recipient_str[0]=zero;
		recipient_str[1]=recipient.data();

		fakedel.receipients=recipient_str;
		courierfax(&fakedel);

		std::string result;

		{
			std::ifstream i{LOGFILE};

			if (i.is_open())
			{
				result=std::string{
					std::istreambuf_iterator<char>{
						i.rdbuf()
					},
					std::istreambuf_iterator<char>{}
				};
			}
		}

#if UPDTEST
		std::cout << "\t\t{\n";

		for (auto str: std::array<std::string_view, 5>{
				t.sender,
				t.host,
				t.recipient,
				t.message,
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
		if (t.result != result)
		{
			std::cerr << "Test " << testnum << "failed.\n"
				"Expected:\n"
				  << t.result << "\nGot:\n"
				  << result << "\n";
			exit(1);
		}
#endif
	}
}

int main()
{
	testfuncsinit();
	testcourierfax();
	unlink(qmsgsdatname(0));
	unlink(LOGFILE);
	return 0;
}
