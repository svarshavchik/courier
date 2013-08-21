/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef cursesfilereq_H
#define cursesfilereq_H

#include "../curses/curses_config.h"

#include "cursescontainer.H"
#include "curseskeyhandler.H"
#include "cursesfield.H"
#include "curseslabel.H"
#include "cursesvscroll.H"

#include <vector>

class CursesMainScreen;

////////////////////////////////////////////////////////////////////////////
//
// A file requester dialog.  It consists of an editable field, where file
// or a directory name may be manually entered, and scrollable contents of
// the current directory (and some miscellaneous labels, here and there)

class CursesFileReq : public CursesContainer,
		      public CursesKeyHandler {

	struct Direntry {
		std::string name;
		unsigned long size;
		bool isDir;
	};

	// Sorting collator - directories go first.

	class DirentryComparator {

		static int group(const Direntry &d);

	public:
		bool operator()(const Direntry &a, const Direntry &b);
	};


	class Dirlist : public CursesVScroll {

		CursesFileReq *parent;
		std::vector<Direntry> directory; // True - this is a dir entry
		int currentRow; // Current row where the cursor is

		void drawItem(size_t n);
	public:
		Dirlist(CursesFileReq *parentArg);
		~Dirlist();

		// This is a child of CursesFileReq, and its size is
		// automatically extended to the bottom of its parent.
		//  Its width is the same as the parent's width.

		int getHeight() const;
		int getWidth() const;

		bool isFocusable(); // Yes we are.

		void focusGained(); // Move to the first row
		void focusLost();   // Turn off cursor
		void draw();

		// Even though this is a CursesContainer subclass, its focus
		// behavior must be the same as Curses's focus behavior
		// (the default CursesContainer implementation doesn't work,
		// because this object does not have any children).

		Curses *getPrevFocus();
		Curses *getNextFocus();

		void operator=(std::vector<Direntry> &directoryArg);

		int getCursorPosition(int &row, int &col);
		bool processKeyInFocus(const Key &key);

	};

	CursesLabel directoryLabel;
	CursesLabel directoryName;
	CursesLabel filenameLabel;
	CursesFieldRedirect<CursesFileReq> filenameField;

	Dirlist dirlist; // Current directory's contents.

	void doresized(); // Reset directoryName's contents

	bool isDialog() const;	// Yes we are

	CursesMainScreen *parent; // My parent

	void opendir();	// Open a new directory

public:

	// The current directory (persistent).
	static std::string currentDir;

	//
	// An externally-provided function that converts a byte count to
	// a displayable size (such as "14Kb")
	//
	// A default implementation is provided, but it can be overriden,
	// if necessary.

	static std::string (*fmtSizeFunc)(unsigned long size, bool isDir);

	CursesFileReq(CursesMainScreen *mainScreen,
		      const char *filenameLabel="Filename: ",
		      const char *directoryLabel="Directory: ");
	~CursesFileReq();

	// Provide a default filename

	void setFilename(std::string n)
	{
		filenameField.setText(n);
	}
	void resized();
	void draw();
	void requestFocus();
	int getWidth() const;
	int getHeight() const;

	// Select the following entry.

	void select(Direntry &d);

	// Callback function - file(s) have been selected.
	// The default implementation saves the filenames in selectedFile,
	// and sets Curses::keepgoing to false.
	//
	// More than one file can be selected using globs.

	virtual void selected(std::vector<std::string> &);

	// Callback function - file selection aborted.
	// The default implementation sets selectedFiles to empty
	// and sets Curses::keepgoing to false.
	virtual void abort();

private:
	bool processKey(const Curses::Key &key);

	void filenameEnter();	// Enter pressed in the filename field

	std::vector<std::string> selectedFiles; // The selected files

	//
	// Standize the filename.  Remove all references to "/./" and "/../"
	//

public:
	static std::string washfname(std::string);

	static void expand(std::string pattern,
			   std::vector<std::string> &filenameList);

	bool listKeys( std::vector< std::pair<std::string, std::string> > &list);

	std::vector<std::string> &getFilenameList()
	{
		return selectedFiles;
	}
};

#endif
