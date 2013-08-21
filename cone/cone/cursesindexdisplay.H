/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef cursesindexdisplay_H
#define cursesindexdisplay_H

#include "config.h"
#include "myfolder.H"
#include "myservercallback.H"
#include "curses/mycurses.H"
#include "curses/curseskeyhandler.H"

#include <vector>
#include <string>

///////////////////////////////////////////////////////////////////
//
// The folder index, displayed.

class CursesIndexDisplay : public Curses, public myFolder::IndexDisplay,
			   public CursesKeyHandler {

	class CheckNewMail;


public:
	enum exit_action {
		no_action,
		copy_single,
		// Screen terminated as a result of selecting "Copy"
		copy_batch,
		// Screen terminated as a result of selecting copy batch.

		move_single,
		move_batch // Move instead of Copy
	} action;

	CursesIndexDisplay(CursesContainer *parentArg,
			   myFolder *folderArg);
	~CursesIndexDisplay();

	// Inherited from Curses:

	int getWidth() const;
	int getHeight() const;
	void draw();
	void draw(size_t n);
	bool isFocusable();
	bool processKeyInFocus(const Curses::Key &key);
	int getCursorPosition(int &row, int &col);

	// Inheried from CursesKeyHandler::

	bool listKeys( std::vector< std::pair<std::string, std::string> > &list);
	bool processKey(const Curses::Key &key);

	// Add function keys mapped to external filters to a help list
	static void listExternalFilterKeys(std::vector< std::pair<std::string,
								  std::string>
							> &list);

private:
	void drawLine(size_t n);
	bool searchPromptBroadenNarrow(mail::ptr<myFolder> &folder,
				       bool &broaden);

public:
	static bool getTag(std::string promptStr, size_t &tagNum);
private:

	bool watchPrompt(unsigned &, unsigned &);

public:


	class FilterMessageCallback : public mail::callback::message,
				      public myServer::Callback {

		std::string filtercmd;

		bool gettingMessage;
		size_t msgnum;

		pid_t childpid;
		int pipefd;

	public:
		std::string errmsg;
	private:
		void reportProgress(size_t bytesCompleted,
				    size_t bytesEstimatedTotal,
				    size_t messagesCompleted,
				    size_t messagesEstimatedTotal);

	public:
		static bool getFilterCmd(int n, std::string &cmd_locale);

		FilterMessageCallback(std::string filtercmdarg);
		~FilterMessageCallback();
		void success(std::string message);
		void fail(std::string message);
		void messageTextCallback(size_t n, std::string text);

		void finish();
	};
};

#endif
