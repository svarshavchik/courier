#! perl
#
# TITLE: Aliases, role accounts, forwarding, and virtual domain accounts
#
#
# Copyright 2001 Double Precision, Inc.  See COPYING for
# distribution information.

use webadmin;

my $aliases=readaliases();

my $errstr="";
my $changeflag=0;

if ($cgi->param("AddAlias"))
{
    my $name=$cgi->param("alias");
    my @l=addrlist($cgi->param("addr"));

    if ($#l < 0 || (! defined ($name=validaddress($name))) || $name eq "")
    {
	$errstr="\@BADCHARS\@";
    }
    else
    {
	$$aliases{$name}=[] unless defined $$aliases{$name};
	my $aa=$$aliases{$name};
	push @$aa, @l;

	my $oldcnt=$#{$aa};

	my %sortit;

	grep { $sortit{$_}=1; } @$aa;

	my @aa=sort keys %sortit;

	$$aliases{$name}=\@aa;

	$changeflag=1;

	$errstr="\@DUPEALIAS\@" unless $#aa == $oldcnt;

    }
}

my $delalias=$cgi->param("DelAlias");

if ($delalias)
{
    my $deladdr=$cgi->param("DelAddr");

    if (defined $$aliases{$delalias})
    {
	my $list=$$aliases{$delalias};
	my $cnt=0;

	my @ll=grep ($_ ne $deladdr, @$list);

	if ($#ll < 0)
	{
	    delete $$aliases{$delalias};
	}
	else
	{
	    $$aliases{$delalias}= \@ll;
	}
	$changeflag=1;
    }
}

if ($cgi->param("vadd"))
{
    my $vdomain=validhostname($cgi->param("vdomain"));
    my $vaccount=validaddress($cgi->param("vaccount"));

    $vaccount=undef if $vaccount =~ /[\.\/]/;

    if ( $vdomain =~ /./ && $vaccount =~ /./)
    {
	grep {
	    $errstr="\@DUPEALIAS\@" if lc($_) eq "\@$vdomain";
	} keys %$aliases;

	foreach (ReadMultiLineConfigFile("locals"))
	{
	    $errstr="\@VLOCALERR\@" if lc($_) eq lc($vdomain);
	}

	foreach (ReadMultiLineConfigFile("hosteddomains/webadmin"))
	{
	    $errstr="\@VLOCALERR\@" if lc($_) eq lc($vdomain);
	}

	if ($errstr eq "")
	{
	    $$aliases{lc("\@$vdomain")}=[ $vaccount ];
	    $changeflag=1;
	    addacceptmailfor($vdomain);
	}
    }
}

if ($cgi->param("vmodify"))
{
    my $vdomain=$cgi->param("vdomain");
    my $wildcard=$cgi->param("wildcard");

    $vdomain=validhostname($vdomain);

    foreach (keys %$aliases)
    {
	next unless (defined $vdomain) && $_ eq "\@$vdomain";

	my $a=$$aliases{$_};

	my $filename=$vdomain;
	$filename =~ s/\./:/g;

	if ($$a[0] eq ".webadmin/$vdomain")
	{
	    my $fh=OpenConfigFile("aliasdir/.courier-:webadmin/$filename-default");

	    if (defined $fh)
	    {
		my $aa=<$fh>;
		close ($fh);
		chomp $aa;
		$aa =~ s/^\&//;
		next unless defined ($aa=validaddress($aa));
		$a= [ $aa ];

	    }
	    else
	    {
		$a=undef;
	    }
	}

	$a=validaddress($$a[0]);

	next unless defined $a;

	if ($wildcard)
	{
	    $$aliases{lc("\@$vdomain")}=[ ".webadmin/$vdomain" ];

	    NewConfigSubdir("aliasdir/.courier-:webadmin");
	    SaveOneLineConfigFile("aliasdir/.courier-:webadmin/$filename-default",
				  "\&$a");
	}
	else
	{
	    $$aliases{lc("\@$vdomain")}=[ $a ] ;
	    DelConfigSubdir("aliasdir/.courier-:webadmin");
	    DeleteConfigFile("aliasdir/.courier-:webadmin/$filename-default");
	}
	$changeflag=1;
    }
}

if ($cgi->param("VDelAlias"))
{
    my $vdomain=validhostname($cgi->param("VDelAlias"));

    my $v=(defined $vdomain) ? $$aliases{"\@$vdomain"}:undef;

    if (defined $v)
    {
	my $filename=$vdomain;
	$filename =~ s/\./:/g;

	DelConfigSubdir("aliasdir/.courier-:webadmin");
	DeleteConfigFile("aliasdir/.courier-:webadmin/$filename-default");

	delacceptmailfor($vdomain);
    }

    delete $$aliases{"\@$vdomain"};
    $changeflag=1;
}

if ($changeflag)
{
    savealiases($aliases);
    changed("makealiases");
}

my $aliaslist="";
my $valiaslist="";

foreach (sort keys %$aliases)
{
    my $k=$_;
    my $addrlist=$$aliases{$k};

    if (substr($k, 0, 1) eq "@")
    {
	$valiaslist="<table border=\"0\" cellspacing=\"4\"><tr><th align=\"left\">\@VDOMAIN\@</th><th align=\"left\" colspan=\"2\">\@VACCOUNT\@</th></tr><tr><td colspan=\"3\"><hr /></td></tr>\n" if $valiaslist eq "";

	$valiaslist .=
	    "<tr><td rowspan=\"2\" valign=\"top\"><tt>" . substr($k, 1)
		. "&nbsp;&nbsp;&nbsp;&nbsp;</tt></td><td><tt>";

	my $filename=substr($k, 1);

	my @list= @$addrlist;

	if ($#list == 0 && $list[0] eq ".webadmin/$filename")
	{
	    my $ff = $list[0];

	    $ff =~ s/\./:/g;

	    my $fh=OpenConfigFile("aliasdir/.courier-$ff-default");

	    if (defined $fh)
	    {
		my $l=<$fh>;
		chomp $l;
		close($fh);

		$l =~ s/^\&//;
		$list[0]=$l;
	    }
	}

	my $s=join("<br />", @list);
	$s =~ s:\@:\&\#64\;:;

	{
	    my $qs=new CGI ( {
		"VDelAlias" => substr($k, 1)
		} );

	    $valiaslist .= "$s</tt></td><td><a href=\"20aliases?"
		. $qs->query_string() . "\">\@VREMOVE\@</a></td></tr>\n";
	}

	$valiaslist .=
	    "<tr><td colspan=\"2\"><form method=\"get\" action=\"20aliases\">"
		. "<input type=\"hidden\" name=\"vdomain\" value=\""
		    . substr($k, 1) . "\" />";

	my $wildcard=0;

	my $a=$$aliases{$k};

	$a=$$a[0];

	$wildcard=1 if substr($a, 0, 10) eq ".webadmin/";

	$valiaslist .=
	    "<input type=\"radio\" name=\"wildcard\" value=\"1\""
		. ($wildcard ? " checked=\"checked\"":"") . " />\@VCATCHALL\@<br />";
	$valiaslist .=
	    "<input type=\"radio\" name=\"wildcard\" value=\"0\""
		. ($wildcard ? "":" checked=\"checked\"") . " />\@VNOCATCHALL\@<br />";
	$valiaslist .=
	    "<input type=\"submit\" name=\"vmodify\" value=\"\@VSAVE\@\" />";
	$valiaslist .= "</form></td></tr><tr><td colspan=\"3\"><hr /></td></tr>\n";
    }
    else
    {
	$aliaslist="<table border=\"0\" cellspacing=\"4\'><tr><th align=\"left\">\@ALIASNAME\@</th><th align=\"left\" colspan=\"3\">\@RECIP\@</th></tr><tr><td colspan=\"4\"><hr /></td></tr>\n" if $aliaslist eq "";


	my $korig=$k;

	$k =~ s:\@:\&\#64\;:;

	my $cell= "<td valign=\"top\" rowspan=\""
	    . ($#{$addrlist}+2) . "\"><tt>$k&nbsp;&nbsp;&nbsp;&nbsp;</tt></td>";

	foreach (@$addrlist)
	{
	    my $a=$_;

	    my $qs=new CGI( {
		"DelAlias" => $korig,
		"DelAddr" => $a
		} );

	    $a =~ s:\@:\&\#64\;:;

	    $aliaslist .= "<tr>$cell<td colspan=\"2\"><tt>$a</tt></td>"
		. "<td><a href=\"20aliases?"
		    . $qs->query_string() . "\">\@REMOVERECIP\@</a></td></tr>\n";
	    $cell="";
	}

	$aliaslist .= "<tr>$cell<td align=\"right\">\@ADDRECIP\@</td><td colspan=\"2\">"
	    . "<form method=\"get\" action=\"20aliases\">"
		. "<input type=\"hidden\" name=\"alias\" value=\"$k\" />"
		    . "<tt><input type=\"text\" name=\"addr\" size=\"20\" /></tt>"
			. "<input type=\"submit\" value=\"\@SAVE\@\" name=\"AddAlias\" />"
			    . "</form></td></tr><tr><td colspan=\"4\"><hr /></td></tr>\n";
    }
}
$aliaslist .= "</table>\n"
    unless $aliaslist eq "";

$valiaslist .= "</table>\n"
    unless $valiaslist eq "";

display_form("admin-20aliases.html",
	     {
		 "ERROR" => $errstr,
		 "ALIASLIST" => $aliaslist,
		 "VALIASLIST" => $valiaslist
	     }
	     );
