/*
**
** Copyright 2007-2008 Double Precision, Inc.
** See COPYING for distribution information.
*/
#include	"config.h"
#include	"webmlmd.H"
#include	"webmlmdhandlers.H"
#include	"webmlmdcmlm.H"

#include	"cmlm.h"
#include	"cmlmsublist.h"
#include	<iostream>
#include	<fstream>
#include	<iomanip>
#include	<sstream>
#include	<algorithm>

#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<signal.h>
#include	<errno.h>
#include	<fcntl.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/wait.h>

#include	"sort.h"

#include	"cgi/cgi.h"

#define SUBLIST	"webmlmd.sublist"

#define PAGESIZE 100

static bool initial_list_sort()
{
	ExclusiveLock create_subindex_lock("webmlmdsublist.lock");
	int pipefd[2];

	if (pipe(pipefd) < 0)
	{
		std::cout << "fork: " << strerror(errno) << std::endl;
		return false;
	}

	signal(SIGCHLD, SIG_DFL);
	pid_t p=fork();

	if (p < 0)
	{
		std::cout << "fork: " << strerror(errno) << std::endl;
		close(pipefd[0]);
		close(pipefd[1]);
		return false;
	}

	if (p == 0)
	{
		close(pipefd[1]);
		if (dup2(pipefd[0], 0) != 0)
		{
			exit(1);
		}

		close(pipefd[0]);

		close(1);
		if (open(SUBLIST, O_WRONLY | O_CREAT | O_TRUNC, 0666)
		    != 1)
		{
			exit(1);
		}
		execl(SORT, SORT, (char *)NULL);
		exit(1);
	}
	close(pipefd[0]);
	FILE *fp=fdopen(pipefd[1], "w");
	if (!fp)
	{
		std::cout << "fdopen: " << strerror(errno) << std::endl;
		close(pipefd[1]);
		return false;
	}

	signal(SIGPIPE, SIG_IGN);

	SubscriberList sub_list;
	std::string addr;

	while (sub_list.Next(addr))
	{
		fprintf(fp, "%s\n", addr.c_str());
	}
	fclose(fp);
	close(pipefd[1]);

	pid_t p2;
	int waitstat;

	while ((p2=wait(&waitstat)) != p)
	{
		if (p2 == -1)
		{
			std::cout << "wait: " << strerror(errno) << std::endl;
			return false;
		}
	}

	if (!WIFEXITED(waitstat) || WEXITSTATUS(waitstat))
	{
		std::cout << "Subscription list sort failed" << std::endl;
		return false;
	}

	return true;
}

static bool build_list_index()
{
	size_t index_entry_size;

	{
		struct stat stat_buf;

		if (stat(SUBLIST, &stat_buf) < 0)
		{
			std::cout << SUBLIST ": " << strerror(errno)
				  << std::endl;
			return false;
		}

		std::ostringstream o;

		o << stat_buf.st_size;

		index_entry_size=o.str().size();
	}

	off_t index_pos=0;

	std::ofstream o(SUBLIST ".idx");

	std::ifstream i(SUBLIST);

	if (!o.is_open() || !i.is_open())
	{
		std::cout << SUBLIST ": " << strerror(errno)
			  << std::endl;
		return false;
	}

	std::string addr;

	while (std::getline(i, addr).good())
	{
		o << std::setw(index_entry_size)
		  << index_pos
		  << std::endl;

		index_pos += addr.size()+1;
	}

	o << std::flush;

	if (o.fail())
	{
		std::cout << SUBLIST ": " << strerror(errno)
			  << std::endl;
		return false;
	}

	return true;
}

class WSubList {
	std::fstream sublist;
	std::ifstream idxsublist;

	off_t idxEntryCount;
	off_t idxEntrySize;

public:
	WSubList();
	~WSubList();

	bool init();
	size_t size() const { return idxEntryCount; }

	std::string operator[](size_t n);
	void erase(size_t n);

	bool getLocation(size_t n, off_t &pos, std::string &addr);
	std::fstream &getSubListFp() { return sublist; }
};

WSubList::WSubList() : sublist(SUBLIST), idxsublist(SUBLIST ".idx"),
		       idxEntryCount(0)
{
}

WSubList::~WSubList()
{
}

bool WSubList::init()
{
	struct stat stat_buf;

	if (sublist.fail() || idxsublist.fail() ||
	    stat(SUBLIST ".idx", &stat_buf) < 0)
	{
		std::cout << SUBLIST ": " << strerror(errno)
			  << std::endl;
		return false;
	}

	std::string dummy;

	if (!std::getline(idxsublist, dummy).good())
		return true; // Empty list

	idxEntrySize=dummy.size()+1;
	idxEntryCount=stat_buf.st_size / idxEntrySize;

	return true;
}

bool WSubList::getLocation(size_t n, off_t &pos, std::string &addr)
{
	if ((off_t)n >= idxEntryCount)
		return false;

	std::string offsetStr;

	idxsublist.clear();
	sublist.clear();

	if (idxsublist.seekg(idxEntrySize * n).fail() ||
	    std::getline(idxsublist, offsetStr).eof())
		return false;

	std::istringstream i(offsetStr);

	i >> pos;

	if (i.fail())
		return false;

	if (sublist.seekg(pos).fail())
		return false;

	std::getline(sublist, addr);

	if (std::find_if(addr.begin(), addr.end(),
			 std::not1(std::ptr_fun(::isspace)))
	    != addr.end())
		return true;
	
	// All spaces -- deleted item?
	return false;
}

std::string WSubList::operator[](size_t n)
{
	off_t offsetNum;
	std::string addr;

	if (getLocation(n, offsetNum, addr))
		return addr;
				
	return "";
}

void WSubList::erase(size_t n)
{
	off_t offsetNum;
	std::string addr;

	if (!getLocation(n, offsetNum, addr))
		return;

	std::fill(addr.begin(), addr.end(), ' ');

	if (!sublist.seekp(offsetNum).fail())
		sublist << addr << std::flush;
}

static void dodel(WSubList &sub_list)
{
	std::istringstream i(cgi("delnum"));
	off_t delnum;

	i >> delnum;

	if (i.fail())
		return;

	std::string address=sub_list[delnum];

	if (address.size() == 0)
		return;

	webmlmd::cmlm unsubscribe;

	std::vector<std::string> args;

	args.push_back(address);

	if (!unsubscribe.start("", "", "unsub", args))
	{
		std::cout << "Error: "
			  << strerror(errno)
			  << std::endl;
		return;
	}

	unsubscribe.mk_received_header();

	FILE *fp=unsubscribe.stdinptr();

	fprintf(fp,
		"Subject: admin unsubscribe\n"
		"\n"
		"This address was unsubscribed by the administrator using"
		" WebMLM\n");

	if (unsubscribe.wait())
	{
		sub_list.erase(delnum);
		return;
	}
	std::cout << "Error: unsubscribe failed"
		  << std::endl;
}

static void show_list_address(size_t n, std::string orig_address,
			      std::string info_text)
{
	std::string infolink;

	if (std::find_if(orig_address.begin(), orig_address.end(),
			 std::not1(std::ptr_fun(::isspace)))
	    == orig_address.end())
	{
		orig_address="***";   // Show in place of an erased address
	}
	else
	{
		std::ostringstream o;

		o << "&nbsp;<a href=\"" << cgirelscriptptr()
		  << "/" << webmlmd::list_name()
		  << "/admin/subinfo?idx=" << n
		  << "\" target=\"_new\"><small>(" << info_text << ")</small</a>";
		infolink=o.str();
	}
	std::cout << "<span class='email'>"
		  << webmlmd::html_escape(orig_address)
		  << "</span>" << infolink;

}

HANDLER("SUBLIST", emit_sublist)
{
	off_t pageNum=0;
	const char *p=cgi("page");

	if (*p)
	{
		std::istringstream i(p);

		i >> pageNum;
	}
	else
	{
		if (!initial_list_sort() ||
		    !build_list_index())
			return;
	}

	std::string info_text="view info";

	if (!args.empty())
		info_text=args.front();

	WSubList sub_list;

	if (!sub_list.init())
		return;

	if (*cgi("dodel"))
		dodel(sub_list);

	std::cout << "<input type='hidden' name='page' value='"
		  << pageNum << "' /><table class='configtbl'><tbody>";
	size_t pageIdx;

	bool doSearch= *cgi("dosearch") && *cgi("searchaddr");

	for (pageIdx=0; pageIdx < (sub_list.size()+PAGESIZE-1)/PAGESIZE; pageIdx++)
	{
		std::cout << "<tr class='oddrow'><td>";

		if ((off_t)pageIdx != pageNum || doSearch)
		{
			std::cout << "<a href='";
			webmlmd::emit_list_url();
			std::cout << "/admin/sublist?page=" << pageIdx
				  << "'>";
		}

		off_t lastAddrIdx=pageIdx * PAGESIZE + PAGESIZE-1;

		if (lastAddrIdx >= (off_t)sub_list.size())
			lastAddrIdx=sub_list.size()-1;

		std::string firstAddr, lastAddr;

		off_t p;

		for (p=pageIdx*PAGESIZE; p <= lastAddrIdx; ++p)
		{
			firstAddr=sub_list[p];
			if (firstAddr != "")
				break;
		}

		p=lastAddrIdx;

		do
		{
			lastAddr=sub_list[p];

			if (lastAddr != "")
				break;

		} while (p-- > (off_t)(pageIdx*PAGESIZE));

		std::cout << "&nbsp;<span class='email'>"
			  << webmlmd::html_escape(firstAddr)
			  << "</span>&nbsp;&hellip;&nbsp;<span class='email'>"
			  << webmlmd::html_escape(lastAddr)
			  << "</span>";


		if ((off_t)pageIdx != pageNum || doSearch)
			std::cout << "</a>";
		std::cout << "</tr></td>\n";
	}
	std::cout << "<tr class='evenrow'><td><hr width='100%' /></td></tr>\n";

	const char *rowcl[2];

	rowcl[0]="oddrow";
	rowcl[1]="evenrow";

	int rowclNext=1;

	if (doSearch)
	{
		std::fstream &i=sub_list.getSubListFp();

		i.clear();
		i.seekg(0);

		size_t n=0;

		std::string search=cgi("searchaddr");

		std::transform(search.begin(), search.end(),
			       search.begin(), std::ptr_fun(tolower));

		std::string address;

		for ( ; std::getline(i, address).good(); ++n)
		{
			std::string orig_address=address;

			std::transform(address.begin(), address.end(),
				       address.begin(), std::ptr_fun(tolower));

			if (std::search(address.begin(), address.end(),
					search.begin(), search.end())
			    == address.end())
				continue;


			rowclNext=1-rowclNext;

			std::cout << "<tr class='" << rowcl[rowclNext]
				  << "'><td><input type='radio' name='delnum' "
				"value='"
				  << n << "' />";


			show_list_address(n, orig_address, info_text);

			std::cout << "</td></tr>\n";
		}

	}
	else
	{
		size_t i;

		for (i=0; i<PAGESIZE; i++)
		{
			off_t n=pageNum*PAGESIZE+i;

			if (n >= (off_t)sub_list.size())
				break;

			rowclNext=1-rowclNext;

			std::string addr=sub_list[n];

			if (addr == "")
			{
				addr="***";
			}
			else
			{
				addr=webmlmd::html_escape(addr);
			}

			std::cout << "<tr class='" << rowcl[rowclNext]
				  << "'><td><input type='radio' name='delnum' "
				"value='"
				  << n << "' />";


			show_list_address(n, sub_list[n], info_text);
			std::cout << "</td></tr>\n";
		}
	}
	std::cout << "</tbody></table>\n";
}

HANDLER("SUBINFO", emit_subinfo)
{
	std::istringstream i(cgi("idx"));

	size_t pos=0;

	i >> pos;

	if (i.fail())
		return;

	std::string address;

	{
		WSubList sub_list;

		if (!sub_list.init())
			return;

		address=sub_list[pos];
	}

	if (std::find_if(address.begin(), address.end(),
			 std::not1(std::ptr_fun(::isspace)))
	    == address.end())
		return;


	webmlmd::cmlm subinfo;

	std::vector<std::string> argv;

	argv.push_back(address);

	if (!subinfo.start("", "", "info", argv) || !subinfo.wait())
		return;

	char linebuf[256];

	while (fgets(linebuf, sizeof(linebuf), subinfo.stdoutptr()))
	{
		std::cout << webmlmd::html_escape(linebuf);
	}
}

