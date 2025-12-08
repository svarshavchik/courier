/*
** Copyright 2023, S. Varshavchik.
**
** See COPYING for distribution information.
*/

#include "config.h"

#ifdef USE_HUNSPELL
#include "spellcheckerHunspell.H"
#include "libmail/mail.H"
#include <errno.h>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <algorithm>
#include <fcntl.h>
#include <sys/wait.h>

SpellChecker::SpellChecker(std::string languageArg,
			   std::string encodingArg)
{
}

SpellChecker::~SpellChecker()
{
}

void SpellChecker::setDictionary(std::string dic)
{
	dictionaryName=dic;
}

struct SpellChecker::pipe_guard {

	int pipefd[2];

	pipe_guard()
	{
		if (pipe(pipefd) < 0)
		{
			pipefd[0]=-1;
		}
	}

	int operator[](size_t n) const
	{
		return pipefd[n];
	}

	operator bool() const
	{
		return pipefd[0] >= 0;
	}

	void used()
	{
		pipefd[0]=-1;
	}

	~pipe_guard()
	{
		close();
	}

	void close()
	{
		if (pipefd[0] >= 0)
		{
			::close(pipefd[0]);
			::close(pipefd[1]);
			used();
		}
	}
};

SpellCheckerBase::Manager *SpellChecker::getManager(std::string &errmsg)
{
	// Pipes attached to hunspell's stdin and stdout, and a CLOEXEC pipe
	// that indicates a successful exec.

	pipe_guard stdin_pipe, stdout_pipe, fork_pipe;

	if (!stdin_pipe || !stdout_pipe || !fork_pipe)
	{
		errmsg="pipe() failed";
		return nullptr;
	}

	pid_t p=fork();

	if (p < 0)
	{
		errmsg="fork() failed";
		return nullptr;
	}

	// The first child forks off the 2nd child which runs hunspell,
	// the first child exits. The parent waits for the first child to
	// exit.

	if (p == 0)
	{
		if (fcntl(fork_pipe[1], F_SETFD, FD_CLOEXEC) < 0)
		{
			if (write(fork_pipe[1], "1", 1) < 0)
				;
			_exit(1);
		}

		p=fork();

		if (p < 0)
		{
			if (write(fork_pipe[1], "1", 1) < 0)
				;
			_exit(1);
		}

		if (p)
			_exit(0);

		// Close the read end of the fork pipe. Set up stdin and
		// stdout.
		close(fork_pipe[0]);

		close(0);
		close(1);
		if (dup(stdin_pipe[0]) != 0 ||
		    dup(stdout_pipe[1]) != 1)
		{
			if (write(fork_pipe[1], "1", 1) < 0)
				;
			_exit(1);
		}

		fork_pipe.used();
		stdin_pipe.close();
		stdout_pipe.close();
		execl(HUNSPELLPROG, HUNSPELLPROG, "-a",
		      (dictionaryName.empty() ? nullptr:"-d"),
		      dictionaryName.c_str(), nullptr);

		if (write(fork_pipe[1], "1", 1) < 0)
			;
		_exit(1);
	}

	close(fork_pipe[1]);

	while (wait(nullptr) != p)
		;

	char stat;

	if (read(fork_pipe[0], &stat, 1) != 0)
	{
		close(fork_pipe[0]);
		fork_pipe.used();
		errmsg="Could not start hunspell";
		return nullptr;
	}
	close(fork_pipe[0]);
	fork_pipe.used();

	FILE *stdin_fp=fdopen(stdin_pipe[1], "w");

	if (stdin_fp == NULL)
	{
		errmsg="Could not start hunspell";
		return nullptr;
	}

	FILE *stdout_fp=fdopen(stdout_pipe[0], "r");

	if (stdout_fp == NULL)
	{
		fclose(stdin_fp);
		errmsg="Could not start hunspell";
		return nullptr;
	}

	close(stdin_pipe[0]);
	close(stdout_pipe[1]);
	stdin_pipe.used();
	stdout_pipe.used();

	return  new SpellChecker::Manager{stdin_fp, stdout_fp};
}

SpellChecker::Manager::Manager(FILE *stdin_fp,
			       FILE *stdout_fp)
	: stdin_fp{stdin_fp},
	  stdout_fp{stdout_fp}
{
	int c;

	// Read the banner line emitted by hunspell.

	while ((c=getc(stdout_fp)) != EOF)
		if (c == '\n')
			break;
}

SpellChecker::Manager::~Manager()
{
	fclose(stdin_fp);
	fclose(stdout_fp);
}

std::string SpellChecker::Manager::search(std::string s, bool &flag)
{
	// suggestions() also calls this. Optimize: check if the word
	// is the same, no action. Otherwise, check the word.

	if (s != lastword)
	{
		lastword=s;
		lastword_good=true;
		hints.clear();

		fprintf(stdin_fp, " %s\n", s.c_str());
		fflush(stdin_fp);

		std::string line;

		// Read one line at a time

		int c;
		while ((c=getc(stdout_fp)) != EOF)
		{
			if (c != '\n')
			{
				line.push_back(c);
				continue;
			}

			// Empty line indicates end of spell checking.
			if (line.empty())
				break;

			switch (*line.c_str()) {
			case '-':
			case '#':
				lastword_good=false;
				break;
			case '&':
				lastword_good=false;
				auto e=line.end();
				auto b=std::find(line.begin(), e, ':');

				while (b != e)
				{
					if (*b == ':' || *b == ',' || *b == ' ')
					{
						++b;
						continue;
					}

					auto p=b;

					while (b != e)
					{
						if (*b == ',')
							break;
						++b;
					}

					hints.push_back(std::string{p, b});
				}
			}
			line.clear();
		}
	}

	flag=lastword_good;

	return "";
}

bool SpellChecker::Manager::suggestions(std::string word,
					std::vector<std::string> &suggestions,
					std::string &errmsg)
{
	bool flag;

	errmsg=search(word, flag);

	suggestions=hints;

	return true;
}

std::string SpellChecker::Manager::replace(std::string word,
					   std::string replacement)
{
	return "";
}

std::string SpellChecker::Manager::addToPersonal(std::string word)
{
	return "";
}

std::string SpellChecker::Manager::addToSession(std::string word)
{
	fprintf(stdin_fp, "@%s\n", word.c_str());
	fflush(stdin_fp);
	return "";
}

#endif
