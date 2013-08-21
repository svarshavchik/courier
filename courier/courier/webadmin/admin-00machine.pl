#! perl
#
# TITLE: Mail server name and local domains
#
#
# Copyright 2001 Double Precision, Inc.  See COPYING for
# distribution information.

my $ME;
my $meerrmsg="";
my $msg="";
my $otherdomainerr="";

use webadmin;

my $aliases=readaliases();
my $defaultME=lc(`hostname 2>/dev/null`);

chomp $defaultME;

if (defined ($ME=$cgi->param("ME")))
{
    if ($ME)
    {
	if (($ME=validhostname($ME)))
	{
	    if (defined($$aliases{"\@" . lc($ME)}))
	    {
		$meerrmsg="\@VIRTUALEXISTS\@";
	    }
	    else
	    {
		SaveOneLineConfigFile("me", lc($ME));
		changed("makealiases", "courier restart");
	    }
	}
	else
	{
	    $meerrmsg="\@BADHOSTNAME\@";
	}
    }
    else
    {
	if (defined($$aliases{"\@" . lc($defaultME)}))
	{
	    $meerrmsg="\@ISVIRTUAL\@";
	}
	else
	{
	    DeleteConfigFile("me");
	    changed("makealiases", "courier restart");
	}
    }

    $msg="\@SAVED\@" unless $meerrmsg;
}

my $remlocal=$cgi->param("remlocal");

if (defined $remlocal)
{
    my @new_locals= grep( ($_ ne $remlocal),
			  ReadMultiLineConfigFile("locals"));

    if ($#new_locals >= 0)
    {
	SaveMultiLineConfigFile("locals", \@new_locals);
    }
    else
    {
	DeleteConfigFile("locals");
    }

    delacceptmailfor($remlocal);
    changed("makealiases", "courier restart");
}

my $remhosted=$cgi->param("remhosted");

if (defined $remhosted)
{
    my @new_hosted=
	grep( ($_ ne $remhosted),
	      ReadMultiLineConfigDirFile("hosteddomains/webadmin"));

    if ($#new_hosted >= 0)
    {
	SaveMultiLineConfigFile("hosteddomains/webadmin", \@new_hosted);
    }
    else
    {
	DeleteConfigFile("hosteddomains/webadmin");
    }

    delacceptmailfor($remhosted);
    changed("makehosteddomains", "courier restart");
}

my $newdomain=lc($cgi->param("newlocaldomain"));
my $localerr="";
my $hostederr="";

if ($newdomain)
{
    if (($newdomain=validhostname($newdomain)))
    {
	my $found=0;

	my @new_locals=ReadMultiLineConfigFile("locals");

	grep { $found=1 if lc($_) eq $newdomain } @new_locals;

	if ($found)
	{
	    $localerr="\@LOCALEXISTS\@";
	}
	else
	{
	    grep {
		$found=1 if lc($_) eq $newdomain
		} ReadMultiLineConfigDirFile("hosteddomains/webadmin");

	    if ($found)
	    {
		$localerr="\@LOCALHOSTED\@";
	    }
	    else
	    {
		if (defined($$aliases{"\@$newdomain"}))
		{
		    $localerr="\@VIRTUALEXISTS\@";
		    $found=1;
		}
		else
		{
		    push @new_locals, $newdomain;
		    @new_locals=sort @new_locals;
		    addacceptmailfor($newdomain);
		}
	    }
	}

	if (! $found)
	{
	    SaveMultiLineConfigFile("locals", \@new_locals);
	    changed("makealiases", "courier restart");
	}
    }
    else
    {
	$localerr="\@BADLOCAL\@";
    }
}

$newdomain=lc($cgi->param("newhosteddomain"));

if ($newdomain)
{
    if (($newdomain=validhostname($newdomain)))
    {
	my $found=0;

	my @new_hosted=ReadMultiLineConfigDirFile("hosteddomains/webadmin");

	grep { $found=1 if lc($_) eq $newdomain } @new_hosted;

	if ($found)
	{
	    $localerr="\@HOSTEDEXISTS\@";
	}
	else
	{
	    grep {
		$found=1 if lc($_) eq $newdomain
		} ReadMultiLineConfigFile("locals");

	    if ($found)
	    {
		$hostederr="\@LOCALHOSTED\@";
	    }
	    else
	    {
		if (defined($$aliases{"\@$newdomain"}))
		{
		    $hostederr="\@VIRTUALEXISTS\@";
		    $found=1;
		}
		else
		{
		    push @new_hosted, $newdomain;
		    @new_hosted=sort @new_hosted;
		    addacceptmailfor($newdomain);
		}
	    }
	}

	if (! $found)
	{
	    SaveMultiLineConfigFile("hosteddomains/webadmin", \@new_hosted);
	    changed("makehosteddomains", "courier restart");
	}
    }
    else
    {
	$hostederr="\@BADHOSTED\@";
    }
}

my $defaultdomain=$cgi->param("defaultdomain");
my $msgidhost=$cgi->param("msgidhost");

if (defined $defaultdomain)
{
    if ($defaultdomain)
    {
	if ( ($defaultdomain=validhostname($defaultdomain)))
	{
	    SaveOneLineConfigFile("defaultdomain", lc($defaultdomain));
	    changed("");
	}
	else
	{
	    $otherdomainerr="\@BADHOSTNAME\@";
	}
    }
    else
    {
	DeleteConfigFile("defaultdomain");
	changed("");
    }
}

if (defined $msgidhost)
{
    if ($msgidhost)
    {
	if ( ($msgidhost=validhostname($msgidhost)))
	{
	    SaveOneLineConfigFile("msgidhost", lc($msgidhost));
	    changed("");
	}
	else
	{
	    $otherdomainerr="\@BADHOSTNAME\@";
	}
    }
    else
    {
	DeleteConfigFile("msgidhost");
	changed("");
    }
}

$ME=lc(ReadOneLineConfigFile("me"));

my $localshtml="<table border=0 cellpadding=8>";

foreach (ReadMultiLineConfigFile("locals"))
{
    $localshtml .= "<tr><td><tt>$_</tt> -</td><td><a href=\"$SELF?remlocal=$_\">\@DELETE\@</a></td></tr>\n";
}

$localshtml .= "</table>\n";

my $hostedhtml="<table border=0' cellpadding=8>";

foreach (ReadMultiLineConfigDirFile("hosteddomains/webadmin"))
{
    $hostedhtml .= "<tr><td><tt>$_</tt> -</td><td><a href=\"$SELF?remhosted=$_\">\@DELETE\@</a></td></tr>\n";
}
$hostedhtml .= "</table>\n";

$defaultdomain=lc(ReadOneLineConfigFile("defaultdomain"));
$msgidhost=lc(ReadOneLineConfigFile("msgidhost"));

display_form("admin-00machine.html",
	     {
		 "ME" => htmlescape($ME),
		 "defaultME" => htmlescape($defaultME),
		 "LOCALS" => $localshtml,
		 "LOCALERR" => $localerr,

		 "HOSTED" => $hostedhtml,
		 "HOSTEDERR" => $hostederr,

		 "DEFAULTDOMAIN" => $defaultdomain,
		 "MSGIDHOST" => $msgidhost,
		 "OTHERDOMAINERR" => $otherdomainerr,
		 "MEERROR" => $meerrmsg,
		 "SAVEDMSG" => $msg,
		 }
	     );
