#! perl
#
#
# Copyright 2001 Double Precision, Inc.  See COPYING for
# distribution information.
#
# Dynamically generate the main menu.

my $fh=new FileHandle;

my %mainmenu;

if (opendir($fh, $webadmindir))
{
    my $n;

    while (defined ($n=readdir($fh)))
    {
	next unless $n =~ /^admin-(.*)\.pl$/;

	my $name=$1;
	my $title="";

	my $ffh=new FileHandle "$webadmindir/$n";
	my $line;

	while (defined($line=<$ffh>))
	{
	    last unless $line =~ /^#/;
	    next unless $line =~ /^#\s+(.*)/;
	    next unless $1 =~ /^TITLE:\s+(.*)/;

	    $line=$1;

	    $mainmenu{$name}=$line;
	    last;
	}
	close($ffh);
    }
    close($fh);
}

my $menuhtml="<table border=\"0\" cellspacing=\"8\">";

foreach (sort keys %mainmenu)
{
    my $title=$mainmenu{$_};

    my $pfix;

    $title =~ /^(>*)/;

    $pfix=length($1);

    $title =~ s/^>*//;

    $menuhtml .= "<tr><td>" . ("&nbsp;" x $pfix) . "<a href=\""
	. $cgi->url(-full=>1) . "/$_\">" . $title . "</a></td></tr>\n";
}

$menuhtml .= "</table>\n";

my $update="\@NOUPDATE\@";
my $cancel="\@NOCANCEL\@";

if ( -f "$sysconfdir/webadmin/changed" )
{
    $update="\@DOUPDATE\@";
    $cancel="\@DOCANCEL\@";
}

display_form("admin-main.html",
	     {
		 "MENU" => $menuhtml,
		 "UPDATE" => $update,
		 "CANCEL" => $cancel
		 }
	     );

