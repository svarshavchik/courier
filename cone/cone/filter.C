/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "filter.H"
#include "gettext.H"
#include "tags.H"
#include "unicode/unicode.h"
#include <sstream>

using namespace std;

Filter::Step::Step() : type(filter_step_selectall)
{
}

Filter::Step::~Step()
{
}

Filter::Step::operator string() const
{
	switch (type) {
	case filter_step_selectall:
		break;
	case filter_step_selectsrch:
		return "SRCH " + string(searchType);
	case filter_step_delete:
		return "DEL";
	case filter_step_undelete:
		return "UNDEL";
	case filter_step_delexpunge:
		return "DELEXP";
	case filter_step_mark:
		return "MARK";
	case filter_step_unmark:
		return "UNMARK";
	case filter_step_copy:
		return "COPY " + mail::searchParams::encode(toserver)
			+ " " + mail::searchParams::encode(tofolder)
			+ " " + mail::searchParams::encode(name_utf8);
	case filter_step_tag:
		return "TAG " + name_utf8;
	case filter_step_move:
		return "MOVE " + mail::searchParams::encode(toserver)
			+ " " + mail::searchParams::encode(tofolder)
			+ " " + mail::searchParams::encode(name_utf8);
	case filter_step_watch:
		return "WATCH";
	case filter_step_unwatch:
		return "UNWATCH";
	}
	return "ALL";
}

Filter::Step::Step(string s) : type(filter_step_selectall)
{
	if (strncmp(s.c_str(), "SRCH ", 5) == 0)
	{
		type=filter_step_selectsrch;
		searchType=mail::searchParams(s.substr(5));
	}
	else if (s == "DEL")
		type=filter_step_delete;
	else if (s == "UNDEL")
		type=filter_step_undelete;
	else if (s == "DELEXP")
		type=filter_step_delexpunge;
	else if (s == "MARK")
		type=filter_step_mark;
	else if (s == "UNMARK")
		type=filter_step_unmark;
	else if (strncmp(s.c_str(), "COPY ", 5) == 0)
	{
		type=filter_step_copy;

		s=mail::searchParams::decode(s.substr(5), toserver);
		s=mail::searchParams::decode(s, tofolder);
		mail::searchParams::decode(s, name_utf8);
	}
	else if (strncmp(s.c_str(), "TAG ", 4) == 0)
	{
		type=filter_step_tag;
		name_utf8=s.substr(4);
	}
	else if (strncmp(s.c_str(), "MOVE ", 5) == 0)
	{
		type=filter_step_move;

		s=mail::searchParams::decode(s.substr(5), toserver);
		s=mail::searchParams::decode(s, tofolder);
		mail::searchParams::decode(s, name_utf8);
	}
	else if (s == "WATCH")
		type=filter_step_watch;
	else if (s == "UNWATCH")
		type=filter_step_unwatch;
}

string Filter::Step::getDescription() const
{
	string s;
	string p1;

	switch (type) {
	case filter_step_selectall:
		break;
	case filter_step_selectsrch:

		p1=searchType.param1;

		switch (searchType.criteria) {
		case mail::searchParams::replied:
			p1=_("REPLIED");
			break;
		case mail::searchParams::deleted:
			p1=_("DELETED");
			break;
		case mail::searchParams::draft:
			p1=_("REPLIED");
			break;
		case mail::searchParams::unread:
			p1=_("UNREAD");
			break;
		case mail::searchParams::from:
			p1=_("From:");
			break;
		case mail::searchParams::to:
			p1=_("To:");
			break;
		case mail::searchParams::cc:
			p1=_("Cc:");
			break;
		case mail::searchParams::bcc:
			p1=_("Bcc:");
			break;
		case mail::searchParams::subject:
			p1=_("Subject:");
			break;

		case mail::searchParams::header:
			p1=Gettext(_("Header \"%1%\"")) << p1;
			break;
		case mail::searchParams::body:
			p1=_("Body");
			break;
		case mail::searchParams::text:
			p1=_("Header and body");
			break;

		// Internal date:

		case mail::searchParams::before:
			p1=Gettext(_("Received before: %1% "))
				<< searchType.param2;
			break;
		case mail::searchParams::on:
			p1=Gettext(_("Received on: %1% "))
				<< searchType.param2;
			break;
		case mail::searchParams::since:
			p1=Gettext(_("Received since: %1% "))
				<< searchType.param2;
			break;

		// Sent date,

		case mail::searchParams::sentbefore:
			p1=Gettext(_("Sent before: %1% "))
				<< searchType.param2;
			break;
		case mail::searchParams::senton:
			p1=Gettext(_("Sent on: %1% ")) << searchType.param2;
			break;
		case mail::searchParams::sentsince:
			p1=Gettext(_("Sent since: %1% ")) << searchType.param2;
			break;

		case mail::searchParams::larger:

			p1=Gettext(_("Larger than %1% bytes"))
				<< searchType.param2;
			break;
		case mail::searchParams::smaller:
			p1=Gettext(_("Smaller than %1% bytes"))
				<< searchType.param2;
			break;
		}

		s= Gettext(searchType.searchNot ? _("Search NOT %1%")
			   : _("Search %1%")) << p1;

		switch (searchType.criteria) {
		case mail::searchParams::from:
		case mail::searchParams::to:
		case mail::searchParams::cc:
		case mail::searchParams::bcc:
		case mail::searchParams::subject:
		case mail::searchParams::header:
		case mail::searchParams::body:
		case mail::searchParams::text:

			{
				std::string chset=searchType.charset;

				if (chset.size() == 0)
					chset="iso-8859-1";

				std::string srch(mail::iconvert::convert
						 (searchType.param2,
						  chset,
						  unicode_default_chset()));

				s=Gettext(_("%1% contains %2%")
					  ) << s << srch;
			}
		break;
		default:
			break;
		}
		return s;
	case filter_step_delete:
		return _("Mark deleted");
	case filter_step_undelete:
		return _("Unmark deleted");
	case filter_step_delexpunge:
		return _("Delete and expunge");
	case filter_step_mark:
		return _("Mark");
	case filter_step_unmark:
		return _("Unmark");
	case filter_step_copy:

		return Gettext(_("Copy to %1%"))
			<< Gettext::fromutf8(name_utf8);
	case filter_step_tag:
		{
			size_t n=0;
			istringstream i(name_utf8);

			i >> n;

			return Gettext(_("Tag as %1%"))
				<< (n < Tags::tags.names.size() ?
				    Tags::tags.names[n] : _("(unknown)"));
		}
	case filter_step_move:
		return Gettext(_("Move to %1%"))
			<< Gettext::fromutf8(name_utf8);
	case filter_step_watch:
		return _("Watch");
	case filter_step_unwatch:
		return _("Unwatch");
	}
	return _("Select all");
}
