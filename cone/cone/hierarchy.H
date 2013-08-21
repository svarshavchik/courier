/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef hierarchy_H
#define hierarchy_H

#include "config.h"

#include "previousscreen.H"
#include "libmail/mail.H"

#include <vector>
#include <map>

/////////////////////////////////////////////////////////////////////////
//
// An abstraction of a folder hierarchy tree.
//

class myServer;

class Hierarchy : public PreviousScreen {

public:
	class Entry;

private:
	std::map<int, Entry *> rowmap;  // Which entry is on which row

public:

	friend class Entry;

	// A single entry in the hierarchy.

	class EntryIterator;

	class Folder;
	class Server;

	// A superclass for all objects that belong in a folder tree.

	class Entry {

		Hierarchy &hierarchy; // The hierarchy object.
		Entry *parent; // The parent object.

		std::vector<Entry *> children; // Children

		int rowNum; // Row number where this item is shown.

	protected:
		int nestingLevel; // Indentation level.
	public:
		Entry(Hierarchy &hierarchy, Entry *parent);

		virtual ~Entry();

		virtual Entry *clone() const=0;

		Entry(const Entry &);	// UNDEFINED
		Entry &operator=(const Entry *); // UNDEFINED

	protected:
		void clone(const Entry &);	// Copies children

	public:
		void clear(); // Remove children.

		size_t size() const;
		std::vector<Entry *>::iterator begin() { return children.begin(); }
		std::vector<Entry *>::iterator end() { return children.end(); }
		std::vector<Entry *>::const_iterator begin() const
		{
			return children.begin();
		}

		std::vector<Entry *>::const_iterator end() const
		{
			return children.end();
		}

		Hierarchy &getHierarchy() const
		{
			return hierarchy;
		}

		Entry *getParent() const
		{
			return parent;
		}

		int getRow() const { return rowNum; }
		int getNestingLevel() const { return nestingLevel; }

		void setRow(int r);

		void MoveUp(); // Move this entry up amongst its siblings.
		void MoveDown(); // Move this entry down

		// Prefix-iteration over the hierarchy

		bool prefixIterate(class EntryIterator &);

		// Partial prefix iteration - do a prefix iteration over the
		// entire hierarchy starting with THIS node.

		bool prefixIterateAfter(class EntryIterator &);

		// Visit entry as part of iteration.  Should return false
		// to stop iteration.

		virtual bool visit(class EntryIterator &)=0;

		// Safe casts:

		Folder *toFolder(); // Return !NULL if this is a folder
		Server *toServer(); // Return !NULL if this is a server

		Server *getServer(); // Return ptr to server, myself or parent
	};

	// An entry that contains a folder.

	class Folder : public Entry {

		class mail::folder *folder;

		size_t messageCount, unreadCount;

	public:
		bool countsInitialized;
		class HierarchyFolderUpdate *pendingUpdate;

		Folder (Hierarchy &hierarchy, Entry *parent,
			const class mail::folder *folderArg);

		virtual ~Folder();

		virtual Entry *clone() const;
		virtual bool visit(class EntryIterator &);

		void setMessageCount(size_t n) { messageCount=n; }
		void setUnreadCount(size_t n) { unreadCount=n; }

		size_t getMessageCount() const { return messageCount; }
		size_t getUnreadCount() const { return unreadCount; }
		const mail::folder *getFolder() const { return folder; }
		mail::folder *getFolder() { return folder; }
		void updateFolder(const mail::folder *newPtr);
		// Replace the folder object (change of status)

		void resortChildren();

		void updateInfo(myServer *, bool doCheaply);
		// Update message counts in folder.

	private:

		class sort : public mail::folder::sort {
		public:
			sort();
			~sort();

			bool operator()(Entry *a, Entry *b);
		};
	};

	// A top level server

	class Server : public Entry {
		class myServer *server;
	public:
		Server(Hierarchy &hierarchy, Entry *parent,
		       class myServer *serverArg);
		~Server();

		virtual Entry *clone() const;
		virtual bool visit(class EntryIterator &);

		virtual myServer *getServer() const { return server; }
	};

	// The root node, parent of all server nodes

	class Root : public Entry {
	public:
		Root(Hierarchy &hierarchy);
		~Root();
		virtual Entry *clone() const;
		virtual bool visit(class EntryIterator &);
	};

	// Object passed to Entry::visit.  The subclass invokes the appropriate
	// visit() method of EntryIterator.			
	class EntryIterator {
	public:
		EntryIterator();
		virtual ~EntryIterator();

		virtual bool visit(Folder *);
		virtual bool visit(Server *);
	};

	// Iterator used to assign rows.

	class EntryIteratorAssignRow : public EntryIterator {

		bool separate_servers;

		// true - extra blank row before each
		// server; used in the initial layout, but we can't do that
		// when we redo the layout.

	public:
		int row;

		EntryIteratorAssignRow(int r=0);
		~EntryIteratorAssignRow();

		bool visit(Folder *f);
		bool visit(Server *s);
	};

	// Safe casting from Entry to the desired object.

	class CastIterator : public EntryIterator {

	public:
		Folder *folder;
		Server *server;

		CastIterator();
		~CastIterator();

		bool visit(Hierarchy::Folder *f);
		bool visit(Hierarchy::Server *s);
	};

	Hierarchy();
	virtual ~Hierarchy();

	Root root; // The root node.

	class HierarchyDisplay *display;
	// Not NULL when this hierarchy is being displayed.

	Entry *getFirstRow();
	Entry *getLastRow();

	Entry *getEntry(int rowNum)
	{
		if (rowmap.count(rowNum) > 0)
			return rowmap.find(rowNum)->second;
		return NULL;
	}

	// Draw or erase the given object.
	void drawErase(Folder *folder, bool doErase=false);
	void drawErase(Server *server, bool doErase=false);
	void drawEraseBelow(Entry *entry, bool doErase=false);
};

//
// Something that displays this hierarchy.  Subclassed by CursesHierarchy

class HierarchyDisplay {
	Hierarchy *hierarchyPtr;
public:
	HierarchyDisplay(Hierarchy *h);
	virtual ~HierarchyDisplay();

	Hierarchy *getHierarchy() const { return hierarchyPtr; }
	virtual void drawEraseBelow(Hierarchy::Entry *e, bool doErase)=0;
	virtual bool drawErase(Hierarchy::Folder *folder, bool doErase)=0;
	virtual bool drawErase(Hierarchy::Server *server, bool doErase)=0;

	virtual void visible(Hierarchy::Entry *e)=0;
};
 

#endif
