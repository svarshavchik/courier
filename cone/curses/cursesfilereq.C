/*
** Copyright 2002-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "curses_config.h"
#include "cursesfilereq.H"
#include "cursesmainscreen.H"

#include <algorithm>

#include <sstream>
#include <string.h>
#include <pwd.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <sys/types.h>
#if HAVE_DIRENT_H
#include <dirent.h>
#define NAMLEN(dirent) strlen((dirent)->d_name)
#else
#define dirent direct
#define NAMLEN(dirent) (dirent)->d_namlen
#if HAVE_SYS_NDIR_H
#include <sys/ndir.h>
#endif
#if HAVE_SYS_DIR_H
#include <sys/dir.h>
#endif
#if HAVE_NDIR_H
#include <ndir.h>
#endif
#endif
#include <sys/stat.h>

#if	HAVE_GLOB_H
#include <glob.h>
#endif

CursesFileReq::Dirlist::Dirlist(CursesFileReq *parentArg)
	: CursesVScroll(parentArg), parent(parentArg), currentRow(-1)
{
}

CursesFileReq::Dirlist::~Dirlist()
{
}

int CursesFileReq::Dirlist::getHeight() const
{
	int h=parent->getHeight();
	int r=getRow();

	return (r < h ? h-r:1);
}

int CursesFileReq::Dirlist::getWidth() const
{
	return (parent->getWidth());
}

void CursesFileReq::Dirlist::draw()
{
	size_t n, wh;

	getVerticalViewport(n, wh);

	size_t h=directory.size() / 2;

	size_t i;

	for (i=0; i<wh; i++)
	{
		size_t p=n+i;

		if (p < h)
			drawItem(p);

		if (p + h < directory.size())
			drawItem(p+h);
	}
}

void CursesFileReq::Dirlist::drawItem(size_t n)
{
	if (n >= directory.size())
		return;

	size_t h=directory.size()/2;

	size_t w=getWidth()/2;

	if (w > 2)
		--w;

	if (w <= 0)
		w=1;

	size_t rowNum=n;
	size_t colNum=0;

	if (rowNum >= h)
	{
		rowNum -= h;
		colNum = w+2;
	}

	std::pair<std::vector<unicode_char>, size_t> nameW;

	{
		widecharbuf wc;

		wc.init_string(directory[n].name);
		wc.expandtabs(0);
		nameW=wc.get_unicode_truncated(w, 0);
	}

	std::string s= (*CursesFileReq::fmtSizeFunc)(directory[n].size,
						     directory[n].isDir);

	std::pair<std::vector<unicode_char>, size_t> sizeW;

	sizeW.second=0;
	if (nameW.second < w)
	{
		widecharbuf wc;

		wc.init_string(s);
		wc.expandtabs(0);
		sizeW=wc.get_unicode_truncated(w-nameW.second, 0);
	}

	nameW.first.insert(nameW.first.end(), w - nameW.second - sizeW.second,
			   ' ');

	nameW.first.insert(nameW.first.end(), sizeW.first.begin(),
			   sizeW.first.end());

	writeText(nameW.first, rowNum, colNum,
		  CursesAttr().setReverse(currentRow >= 0 &&
					  (size_t)currentRow == n));
}

void CursesFileReq::Dirlist::operator=(std::vector<Direntry> &directoryArg)
{
	directory=directoryArg;
	currentRow= -1;
	scrollTo(0);
	draw();
}

int CursesFileReq::Dirlist::getCursorPosition(int &row, int &col)
{
	if (currentRow < 0)
	{
		row=0;
		col=0;
		CursesVScroll::getCursorPosition(row, col);
		return 0;
	}

	size_t h=directory.size()/2;

	size_t w=getWidth()/2;

	if (w > 2)
		--w;

	if (w <= 0)
		w=1;

	row=currentRow;
	col=w-1;

	if ( (size_t)currentRow >= h)
	{
		row -= h;
		col += w+2;
	}
	CursesVScroll::getCursorPosition(row, col);
	return 0;
}

bool CursesFileReq::Dirlist::isFocusable()
{
	return true;
}

bool CursesFileReq::Dirlist::processKeyInFocus(const Key &key)
{
	if (key == key.UP)
	{
		if (currentRow == 0)
		{
			Curses::transferPrevFocus();
			return true;
		}

		--currentRow;
		drawItem(currentRow);
		drawItem(currentRow+1);
		return true;
	}

	if (key == key.DOWN)
	{
		if ((size_t)currentRow + 1 >= directory.size())
		{
			Curses::transferNextFocus();
			return true;
		}

		++currentRow;
		drawItem(currentRow);
		drawItem(currentRow-1);
		return true;
	}

	if (key == key.PGUP)
	{
		size_t h=getHeight();

		size_t oldRow=currentRow;

		currentRow=h < (size_t)currentRow ? currentRow-h:0;

		drawItem(oldRow);
		drawItem(currentRow);
		return true;
	}

	if (key == key.PGDN)
	{
		size_t h=getHeight();

		size_t oldRow=currentRow;

		currentRow=currentRow + h < directory.size() ? currentRow+h:
			directory.size() > 0 ? directory.size()-1:0;

		drawItem(oldRow);
		drawItem(currentRow);
		return true;
	}

	if (key == key.LEFT)
	{
		size_t h=directory.size() / 2;

		if ((size_t)currentRow >= h && (size_t)currentRow - h < h)
		{
			currentRow -= h;
			drawItem(currentRow);
			drawItem(currentRow+h);
		}
		return true;
	}

	if (key == key.RIGHT)
	{
		size_t h=directory.size() / 2;

		if ((size_t)currentRow < h &&
		    (size_t)currentRow + h < directory.size())
		{
			currentRow += h;
			drawItem(currentRow);
			drawItem(currentRow-h);
		}
		return true;
	}

	if (key == key.ENTER)
	{
		if (currentRow != -1 &&
		    (size_t)currentRow < directory.size())
			parent->select(directory[currentRow]);
		return true;
	}

	return false;
}

// Must override getPrevFocus/getNextFocus in CursesContainer, because we
// do not have any children and should behave like Curses.

Curses *CursesFileReq::Dirlist::getPrevFocus()
{
	return Curses::getPrevFocus();
}

Curses *CursesFileReq::Dirlist::getNextFocus()
{
	return Curses::getNextFocus();
}

void CursesFileReq::Dirlist::focusGained()
{
	currentRow=0;
	draw();
}

void CursesFileReq::Dirlist::focusLost()
{
	currentRow= -1;
	draw();
}

std::string CursesFileReq::currentDir;

static std::string defaultSizeFunc(unsigned long size, bool isDir)
{
	if (isDir)
		return " (Dir)";

	std::string buffer;

	{
		std::ostringstream o;

		o << " (" << size << ")";
		buffer=o.str();
	}

	return buffer;
}

std::string (*CursesFileReq::fmtSizeFunc)(unsigned long, bool)= &defaultSizeFunc;

CursesFileReq::CursesFileReq(CursesMainScreen *mainScreen,
			     const char *filenameLabelArg,
			     const char *directoryLabelArg)
	: CursesContainer(mainScreen), CursesKeyHandler(PRI_DIALOGHANDLER),
	  directoryLabel(this, directoryLabelArg),
	  directoryName(this, ""),
	  filenameLabel(this, filenameLabelArg),
	  filenameField(this),
	  dirlist(this),
	  parent(mainScreen)
{
	filenameField=this;
	filenameField= &CursesFileReq::filenameEnter;

	int w=directoryLabel.getWidth();
	int ww=filenameLabel.getWidth();

	if (ww > w)
		w=ww;

	directoryLabel.setRow(0);
	directoryLabel.setCol(w);
	directoryLabel.setAlignment(directoryLabel.RIGHT);

	filenameLabel.setRow(2);
	filenameLabel.setCol(w);
	filenameLabel.setAlignment(filenameLabel.RIGHT);

	directoryName.setRow(0);
	directoryName.setCol(w);

	filenameField.setRow(2);
	filenameField.setCol(w);

	dirlist.setRow(4);

	if (currentDir.size() == 0)
	{
		std::string homedir=getenv("HOME");

		if (homedir.size() == 0)
		{
			struct passwd *pw=getpwuid(getuid());

			if (pw)
				homedir=pw->pw_dir;
			else
				homedir="";
		}

		currentDir=homedir;
	}

	doresized();
	opendir();
}

int CursesFileReq::getWidth() const
{
	return parent->getWidth();
}

int CursesFileReq::getHeight() const
{
	return (parent->getHeight());
}

CursesFileReq::~CursesFileReq()
{
}

bool CursesFileReq::isDialog() const
{
	return true;
}

void CursesFileReq::resized()
{
	doresized();
	CursesContainer::resized();
}

void CursesFileReq::draw()
{
	CursesContainer::draw();
}

void CursesFileReq::doresized()
{
	int w=getWidth();

	int c=filenameField.getCol();

	filenameField.setWidth(c > w ? 1: w-c);

	// If too narrow fo the full directory name, truncate it.

	c=directoryName.getCol();

	size_t dls=c < w ? w-c:1;

	std::vector<unicode_char> directoryW;

	mail::iconvert::convert(currentDir, unicode_default_chset(),
				directoryW);

	size_t directoryW_size;

	{
		widecharbuf wc;

		wc.init_unicode(directoryW.begin(), directoryW.end());
		wc.expandtabs(0);

		wc.tounicode(directoryW);

		directoryW_size=wc.wcwidth(0);
	}

	if (directoryW_size > dls)
	{
		std::vector<unicode_char>::iterator b, e, c, lastSlash;

		b=directoryW.begin();
		e=directoryW.end();

		lastSlash=b;

		for (c=b; c != e; )
		{
			if (*c++ == '/')
				lastSlash=c;
		}

		std::vector<unicode_char> replVector;

		replVector.push_back('/');
		replVector.push_back('.');
		replVector.push_back('.');
		replVector.push_back('.');
		replVector.push_back('/');

		replVector.insert(replVector.end(), lastSlash, e);

		size_t suffixSize;

		{
			widecharbuf wc;

			wc.init_unicode(replVector.begin(), replVector.end());

			suffixSize=wc.wcwidth(0);
		}

		if (suffixSize < dls)
		{
			widecharbuf wc;

			wc.init_unicode(directoryW.begin(), lastSlash);

			directoryW=wc.get_unicode_truncated(dls-suffixSize, 0)
				.first;

			lastSlash=b=directoryW.begin();
			e=directoryW.end();

			for (c=b; c != e; ++c)
			{
				if (*c == '/')
					lastSlash=c;
			}

		}
		else
		{
			lastSlash=directoryW.begin();
		}

		directoryW.erase(lastSlash, directoryW.end());

		directoryW.insert(directoryW.end(),
				  replVector.begin(), replVector.end());
	}

	directoryName.setText(mail::iconvert::convert(directoryW,
						      unicode_default_chset()));
}

void CursesFileReq::requestFocus()
{
	filenameField.requestFocus();
}

void CursesFileReq::select(Direntry &d)
{
	if (d.isDir)	// Open this directory.
	{
		currentDir = currentDir + "/" + d.name;
		opendir();
		Curses::erase();
		doresized();
		draw();
		filenameField.requestFocus();
		return;
	}

	std::vector<std::string> filenameList;

	filenameList.push_back(currentDir + "/" + d.name);
	selected(filenameList);
}

void CursesFileReq::expand(std::string pattern,
			   std::vector<std::string> &filenameList)
{
	filenameList.clear();

#if	HAVE_GLOB

	glob_t pglob;

	if (glob(pattern.c_str(), GLOB_ERR|GLOB_NOCHECK

#ifdef GLOB_BRACE
		 | GLOB_BRACE
#endif

#ifdef GLOB_NOMAGIC
		 | GLOB_NOMAGIC
#endif

#ifdef GLOB_TILDE
		 | GLOB_TILDE
#endif
		 , NULL, &pglob) == 0)
	{
		if (pglob.gl_pathc > 0)
		{
			try {
				size_t i;

				for (i=0; i<pglob.gl_pathc; i++)
				{
					filenameList
						.push_back(washfname(pglob
								     .gl_pathv
								     [i]));
				}
			} catch (...) {
				globfree(&pglob);
				throw;
			}
		}
		globfree(&pglob);
	}

#endif
	if (filenameList.size() == 0)
	{
		filenameList.push_back(washfname(pattern));
	}
}

			   
void CursesFileReq::filenameEnter()
{
	std::string s=filenameField.getText();

	if (s.size() == 0)
		return;

	// Relative path?  Prepend current directory

	if (s[0] != '/')
		s=currentDir + "/" + s;

	std::vector<std::string> filenameList;

	expand(s, filenameList);

	struct stat stat_buf;

	if (filenameList.size() == 1 &&
	    stat(filenameList[0].c_str(), &stat_buf) == 0 &&
	    S_ISDIR(stat_buf.st_mode))
	{
		currentDir=filenameList[0];
		filenameField.setText("");
		opendir();
		Curses::erase();
		doresized();
		draw();
		filenameField.requestFocus();
		return;
	}


	selected(filenameList);
}

bool CursesFileReq::processKey(const Curses::Key &key)
{
	if (key.plain() && key.ukey == '\x03')
	{
		abort();
		return true;
	}
	return key.plain() && (unsigned char)key.ukey == key.ukey &&
		(unsigned char)key.ukey < ' ';
}

void CursesFileReq::selected(std::vector<std::string> &filenameList)
{
	selectedFiles=filenameList;
	keepgoing=false;
}

void CursesFileReq::abort()
{
	selectedFiles.clear();
	keepgoing=false;
}

void CursesFileReq::opendir()
{
	currentDir=washfname(currentDir); // cleanup filename.

	std::vector<Direntry> newDirList;

	{
		std::vector<std::string> filenames;

		DIR *dirp= ::opendir(currentDir.c_str());

		struct dirent *de;

		try {
			while (dirp && (de=readdir(dirp)) != NULL)
				filenames.push_back(de->d_name);

			if (dirp)
				closedir(dirp);
		} catch (...) {
			if (dirp)
				closedir(dirp);
			throw;
		}

		// stat everything, figure out if its a dir or not

		std::vector<std::string>::iterator b=filenames.begin(),
			e=filenames.end();

		while (b != e)
		{
			Direntry d;

			d.name= *b;

			struct stat statInfo;

			std::string n=currentDir + "/" + *b++;

			if (stat(n.c_str(), &statInfo) < 0)
			    continue;

			d.size=statInfo.st_size;

			d.isDir= S_ISDIR(statInfo.st_mode);

			newDirList.push_back(d);
		}
	}

	sort(newDirList.begin(), newDirList.end(), DirentryComparator());

	dirlist=newDirList;
}

std::string CursesFileReq::washfname(std::string filename)
{
	filename += "/"; // Temporary thing.

	std::string::iterator b, c, e;

	b=filename.begin();
	e=filename.end();
	c=b;

	// Combine multiple /s into one.

	while (b != e)
	{
		if (*b != '/')
		{
			*c++ = *b++;
			continue;
		}

		while (b != e && *b == '/')
			b++;

		*c++ = '/';
	}

	filename.erase(c, e);

	b=filename.begin();
	e=filename.end();
	c=b;

	while (b != e)
	{
		if (*b == '/')
		{
			if (e - b >= 3 &&
			    b[1] == '.' && b[2] == '/')
			{
				b += 2;
				continue;
			}
			else if (e - b >= 4 &&
				 b[1] == '.' && b[2] == '.' && b[3] == '/')
			{
				b += 3;

				while (c != filename.begin())
				{
					if (*--c == '/')
						break;
				}
				continue;
			}
		}

		*c++ = *b++;
	}

	if (c != filename.begin())
		--c; // Trailing /

	filename.erase(c, e);
	return filename;
}

// Sort filename list.  Directories first, then files.

int CursesFileReq::DirentryComparator::group(const Direntry &d)
{
	if (d.name == ".")
		return 0;

	if (d.name == "..")
		return 1;

	if (d.isDir)
		return 2;
	return 3;
}

bool CursesFileReq::DirentryComparator::operator()(const Direntry &a,
						   const Direntry &b)
{
	int d=group(a) - group(b);

	if (d != 0)
		return  (d < 0);

	return (strcoll(a.name.c_str(), b.name.c_str()) < 0);
}

bool CursesFileReq::listKeys( std::vector< std::pair<std::string, std::string> > &list)
{
	return true;
}
