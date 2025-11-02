#include <unordered_map>
#include <string>
#include <pwd.h>
#include <unistd.h>
#include <locale.h>
#include <string.h>
#include <array>
#include <string_view>
#include <iostream>
#include <fstream>
#include <tuple>
#include <iterator>
#include "courier.h"

void *courier_malloc(unsigned n)
{
	return malloc(n);
}

char *courier_strdup(const char *ptr)
{
	return strdup(ptr);
}

char	*config_localfilename(const char *n)
{
	char *p=(char *)malloc(strlen(n)+30);
	return strcat(strcpy(p, "testsendmailrw.dir/"), n);
}

std::unordered_map<std::string, std::string> fake_env;

const char *get_fake_env(const char *name)
{
	auto iter=fake_env.find(name);

	if (iter == fake_env.end())
		return nullptr;

	return iter->second.c_str();
}

struct passwd *mypwd()
{
	static char pw_name[]="systemuser";
	static char pw_passwd[]="";
	static char pw_gecos[]="John Q Public";
	static char pw_dir[]="/";
	static char pw_shell[]="/bin/sh";

	static struct passwd p{
		pw_name,
		pw_passwd,
		geteuid(),
		getegid(),
		pw_gecos,
		pw_dir,
		pw_shell
	};

	return &p;
}
#define MYPWD 1

#define ENVIRON_SOURCE get_fake_env
#include "sendmail2.C"

void testrewrite_env_sender()
{
	const struct {
		const char *from;
		std::array<std::tuple<const char *, const char *>, 2> env;
		const char *result;
	} tests[]={
		// Test 1
		{
			"sender@example.com",
			{},
			"sender@example.com"
		},
		// Test 2
		{
			nullptr,
			{
				{
					{
						"MAILUSER",
						"list"
					},
					{
						"MAILHOST",
						"list.example.com"
					}
				}
			},
			"list@list.example.com"
		},
		// Test 3
		{
			nullptr,
			{
				{
					{
						"MAILHOST",
						"list.example.com"
					}
				}
			},
			"systemuser@list.example.com"
		},
		// Test 4
		{
			nullptr,
			{
				{
					{
						"MAILHOST",
						"test.xn--80akhbyknj4f"
					}
				}
			},
			"systemuser@test.испытание"
		},
	};

	// #define UPDATE_ENV_SENDER 1

	size_t testnum=0;

	for (const auto &test:tests)
	{
		++testnum;

		fake_env.clear();
		for (auto &[name,value]:test.env)
		{
			if (name && value) fake_env.emplace(name, value);
		}

		auto result=rewrite_env_sender(test.from);

#if UPDATE_ENV_SENDER
		std::cout << "\t\t// Test " << testnum << "\n"
			"\t\t{\n\t\t\t";

		if (test.from)
			std::cout << "\"" << test.from << "\"";
		else
			std::cout << "nullptr";

		std::cout << ",\n\t\t\t{";

		const char *pfix="\n\t\t\t\t{\n", *sfix="";

		for (auto &[name,value]:test.env)
		{
			if (!name || !value)
				continue;
			std::cout << pfix << "\t\t\t\t\t{\n"
				"\t\t\t\t\t\t\""
				  << name << "\",\n"
				"\t\t\t\t\t\t\"" << value << "\"\n\t\t\t\t\t}";
			pfix=",\n";
			sfix="\n\t\t\t\t}\n\t\t\t";
		}
		std::cout << sfix << "},\n\t\t\t\""
			  << result << "\"\n\t\t},\n";
#else
		if (result != test.result)
		{
			std::cout << "testrewrite_env_sender test "
				  << testnum
				  << " failed:\nExpected: "
				  << test.result
				  << "\n     Got: " << result << "\n";
			exit (1);
		}
#endif
	}
}

void testrewrite_headers()
{
	const struct {
		const char *from;
		std::string_view headers;
		std::array<std::tuple<const char *, const char *>, 4> env;
		std::string_view result;
	} tests[]={
		// Test 1
		{
			nullptr,
			"From: user@original\n",
			{},
			"From: user@original\n"
			"\n"
		},
		// Test 2
		{
			nullptr,
			"From: John Smith <user@original>\n",
			{},
			"From: John Smith <user@original>\n"
			"\n"
		},
		// Test 3
		{
			nullptr,
			"From: user@original\n",
			{
				{
					{
						"MAILUSER",
						"replacement_user"
					}
				}
			},
			"From: replacement_user@original\n"
			"\n"
		},
		// Test 4
		{
			nullptr,
			"From: user@original\n",
			{
				{
					{
						"MAILHOST",
						"replacement.example.com"
					}
				}
			},
			"From: user@replacement.example.com\n"
			"\n"
		},
		// Test 5
		{
			nullptr,
			"From: user@original\n",
			{
				{
					{
						"MAILUSER",
						"replacement_user"
					},
					{
						"MAILHOST",
						"replacement.example.com"
					}
				}
			},
			"From: replacement_user@replacement.example.com\n"
			"\n"
		},
		// Test 6
		{
			nullptr,
			"From: user@example.com\n",
			{
				{
					{
						"MAILNAME",
						"John Smith"
					}
				}
			},
			"From: John Smith <user@example.com>\n"
			"\n"
		},
		// Test 7
		{
			nullptr,
			"From: user@example.com\n",
			{
				{
					{
						"MAILNAME",
						"John Q. Smith"
					},
					{
						"MAILUSER",
						"john"
					}
				}
			},
			"From: \"John Q. Smith\" <john@example.com>\n"
			"\n"
		},
		// Test 8
		{
			nullptr,
			"Subject: nofromheader\n",
			{},
			"Subject: nofromheader\n"
			"From: John Q Public <systemuser>\n"
			"\n"
		},
		// Test 9
		{
			nullptr,
			"Subject: nofromheader\n",
			{
				{
					{
						"MAILUSER",
						"replacement_user"
					}
				}
			},
			"Subject: nofromheader\n"
			"From: John Q Public <replacement_user>\n"
			"\n"
		},
		// Test 10
		{
			nullptr,
			"Subject: nofromheader\n",
			{
				{
					{
						"MAILHOST",
						"replacement.example.com"
					}
				}
			},
			"Subject: nofromheader\n"
			"From: John Q Public <systemuser@replacement.example.com>\n"
			"\n"
		},
		// Test 11
		{
			nullptr,
			"Subject: nofromheader\n",
			{
				{
					{
						"MAILUSER",
						"replacement_user"
					},
					{
						"MAILHOST",
						"replacement.example.com"
					}
				}
			},
			"Subject: nofromheader\n"
			"From: John Q Public <replacement_user@replacement.example.com>\n"
			"\n"
		},
		// Test 12
		{
			nullptr,
			"Subject: nofromheader\n",
			{
				{
					{
						"MAILNAME",
						"John Smith"
					}
				}
			},
			"Subject: nofromheader\n"
			"From: John Smith <systemuser>\n"
			"\n"
		},
		// Test 13
		{
			nullptr,
			"Subject: nofromheader\n",
			{
				{
					{
						"MAILNAME",
						"John Q. Smith"
					},
					{
						"MAILUSER",
						"john"
					}
				}
			},
			"Subject: nofromheader\n"
			"From: \"John Q. Smith\" <john>\n"
			"\n"
		},
		// Test 14
		{
			"From User",
			"From: user@original\n",
			{},
			"From: From User <user@original>\n"
			"\n"
		},
		// Test 15
		{
			"From User",
			"From: John Smith <user@original>\n",
			{},
			"From: From User <user@original>\n"
			"\n"
		},
		// Test 16
		{
			"From User",
			"From: user@original\n",
			{
				{
					{
						"MAILUSER",
						"replacement_user"
					}
				}
			},
			"From: From User <replacement_user@original>\n"
			"\n"
		},
		// Test 17
		{
			"From User",
			"From: user@original\n",
			{
				{
					{
						"MAILHOST",
						"replacement.example.com"
					}
				}
			},
			"From: From User <user@replacement.example.com>\n"
			"\n"
		},
		// Test 18
		{
			"From User",
			"From: user@original\n",
			{
				{
					{
						"MAILUSER",
						"replacement_user"
					},
					{
						"MAILHOST",
						"replacement.example.com"
					}
				}
			},
			"From: From User <replacement_user@replacement.example.com>\n"
			"\n"
		},
		// Test 19
		{
			"From User",
			"From: user@example.com\n",
			{
				{
					{
						"MAILNAME",
						"John Smith"
					}
				}
			},
			"From: John Smith <user@example.com>\n"
			"\n"
		},
		// Test 20
		{
			"From User",
			"From: user@example.com\n",
			{
				{
					{
						"MAILNAME",
						"John Q. Smith"
					},
					{
						"MAILUSER",
						"john"
					}
				}
			},
			"From: \"John Q. Smith\" <john@example.com>\n"
			"\n"
		},
		// Test 21
		{
			"From User",
			"Subject: nofromheader\n",
			{},
			"Subject: nofromheader\n"
			"From: From User <systemuser>\n"
			"\n"
		},
		// Test 22
		{
			"From User",
			"Subject: nofromheader\n",
			{
				{
					{
						"MAILUSER",
						"replacement_user"
					}
				}
			},
			"Subject: nofromheader\n"
			"From: From User <replacement_user>\n"
			"\n"
		},
		// Test 23
		{
			"From User",
			"Subject: nofromheader\n",
			{
				{
					{
						"MAILHOST",
						"replacement.example.com"
					}
				}
			},
			"Subject: nofromheader\n"
			"From: From User <systemuser@replacement.example.com>\n"
			"\n"
		},
		// Test 24
		{
			"From User",
			"Subject: nofromheader\n",
			{
				{
					{
						"MAILUSER",
						"replacement_user"
					},
					{
						"MAILHOST",
						"replacement.example.com"
					}
				}
			},
			"Subject: nofromheader\n"
			"From: From User <replacement_user@replacement.example.com>\n"
			"\n"
		},
		// Test 25
		{
			"From User",
			"Subject: nofromheader\n",
			{
				{
					{
						"MAILNAME",
						"John Smith"
					}
				}
			},
			"Subject: nofromheader\n"
			"From: From User <systemuser>\n"
			"\n"
		},
		// Test 26
		{
			"From User",
			"Subject: nofromheader\n",
			{
				{
					{
						"MAILNAME",
						"John Q. Smith"
					},
					{
						"MAILUSER",
						"john"
					}
				}
			},
			"Subject: nofromheader\n"
			"From: From User <john>\n"
			"\n"
		},
		// Test 27
		{
			"испытание",
			"Subject: nofromheader\n",
			{
				{
					{
						"MAILNAME",
						"John Q. Smith"
					},
					{
						"MAILUSER",
						"john"
					}
				}
			},
			"Subject: nofromheader\n"
			"From: =?UTF-8?B?0LjRgdC/0YvRgtCw0L3QuNC1?= <john>\n"
			"\n"
		},
		// Test 28
		{
			"\"John Q Public\"",
			"Subject: nofromheader\n",
			{
				{
					{
						"MAILNAME",
						"John Q. Smith"
					},
					{
						"MAILUSER",
						"john"
					}
				}
			},
			"Subject: nofromheader\n"
			"From: =?UTF-8?B?IkpvaG4=?= Q =?UTF-8?Q?Public=22?= <john>\n"
			"\n"
		},
		// Test 29
		{
			nullptr,
			"Subject: nofromheader\n",
			{
				{
					{
						"MAILNAME",
						"испытание"
					},
					{
						"MAILUSER",
						"john"
					}
				}
			},
			"Subject: nofromheader\n"
			"From: =?UTF-8?B?0LjRgdC/0YvRgtCw0L3QuNC1?= <john>\n"
			"\n"
		},
		// Test 30
		{
			nullptr,
			"Subject: nofromheader\n",
			{
				{
					{
						"MAILUSER",
						"john"
					},
					{
						"MAILHOST",
						"test.испытание"
					}
				}
			},
			"Subject: nofromheader\n"
			"From: John Q Public <john@test.xn--80akhbyknj4f>\n"
			"\n"
		},
	};

	// #define UPDATE_HEADERS 1

	size_t testnum=0;

	for (const auto &test:tests)
	{
		++testnum;

		fake_env.clear();
		for (auto &[name,value]:test.env)
		{
			if (name && value) fake_env.emplace(name, value);
		}

		FILE *fp=fopen("testsendmail.in", "w");
		fwrite(test.headers.data(), test.headers.size(), 1, fp);
		fclose(fp);

		fp=fopen("testsendmail.in", "r");
		FILE *fpout=fopen("testsendmail.out", "w");
		rewrite_headers(test.from, fp, fpout);
		fclose(fp);
		fclose(fpout);

		std::string result;

		{
			std::ifstream ofs{"testsendmail.out"};

			result.insert(result.end(),
				      std::istreambuf_iterator<char>{
					      ofs.rdbuf()
				      },
				      std::istreambuf_iterator<char>{});
		}

#if UPDATE_HEADERS
		std::cout << "\t\t// Test " << testnum << "\n"
			"\t\t{\n\t\t\t";

		if (test.from)
		{
			std::cout << "\"";

			for (char c:std::string_view{test.from})
			{
				if (c == '\\' || c == '"')
				{
					std::cout << "\\";
				}
				std::cout << c;
			}
			std::cout << "\"";
		}
		else
			std::cout << "nullptr";

		std::cout << ",\n\t\t\t\"";
		const char *pfix="";
		const char *sfix="\",\n";

		for (char c:test.headers)
		{
			std::cout << pfix;
			if (c == '\n')
			{
				std::cout << "\\n\"";
				pfix="\n\t\t\t\"";
				sfix=",\n";
				continue;
			}

			if (c == '"' || c == '\\')
				std::cout << "\\";
			std::cout << c;
			pfix="";
			sfix="\",\n";
		}
		std::cout << sfix << "\t\t\t{";

		pfix="\n\t\t\t\t{\n";
		sfix="";

		for (auto &[name,value]:test.env)
		{
			if (!name || !value)
				continue;
			std::cout << pfix << "\t\t\t\t\t{\n"
				"\t\t\t\t\t\t\""
				  << name << "\",\n"
				"\t\t\t\t\t\t\"" << value << "\"\n\t\t\t\t\t}";
			pfix=",\n";
			sfix="\n\t\t\t\t}\n\t\t\t";
		}
		std::cout << sfix << "},\n\t\t\t\"";

		pfix="";
		sfix="\"\n";

		for (char c:result)
		{
			std::cout << pfix;
			if (c == '\n')
			{
				std::cout << "\\n\"";
				pfix="\n\t\t\t\"";
				sfix="\n";
				continue;
			}

			if (c == '"' || c == '\\')
				std::cout << "\\";
			std::cout << c;
			pfix="";
			sfix="\"\n";
		}

		std::cout << sfix << "\t\t},\n";
#else
		if (result != test.result)
		{
			std::cout << "testrewrite_env_sender test "
				  << testnum
				  << " failed:\nExpected: "
				  << test.result
				  << "\n     Got: " << result << "\n";
			exit (1);
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
		std::cout << "Unable to set the system locale: "
			  << e.what() << "\n";
		exit(1);
	}

	testrewrite_env_sender();
	testrewrite_headers();
	unlink("testsendmail.in");
	unlink("testsendmail.out");
	return 0;
}
