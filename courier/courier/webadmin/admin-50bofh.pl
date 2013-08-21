#! perl
#
# TITLE: BOFH mail filters
#
#
# Copyright 2001 Double Precision, Inc.  See COPYING for
# distribution information.


use webadmin;

my $badfrom1=$cgi->param("badfrom1");
my $errmsg="";

$badfrom1="\@" . $cgi->param("badfrom2") if $cgi->param("badfrom2") =~ /./;
$badfrom1="\@." . $cgi->param("badfrom3") if $cgi->param("badfrom3") =~ /./;

my $remove=$cgi->param("remove");

if (defined $remove)
{
	my @l =
	    grep( $_ ne $remove, ReadMultiLineConfigFile("bofh"));

	SaveMultiLineConfigFile("bofh", \@l);
	changed("");
}

if ($badfrom1 =~ /./)
{
    $badfrom1 =~ s/(@.*)/lc($1)/e;

    if ($badfrom1 =~ /[\n\s\"\']/)
    {
	$errmsg="\@BADCHARS\@";
    }
    else
    {
	my @new_badfrom1=
	    grep( !($_ =~ /^\s*badfrom\s+(.*)$/ && $1 eq $badfrom1),
		  ReadMultiLineConfigFile("bofh"));
	push @new_badfrom1, "badfrom $badfrom1";

	SaveMultiLineConfigFile("bofh", \@new_badfrom1);

	changed("");
    }
}

my $spamtrap=$cgi->param("spamtrap");

if ($spamtrap =~ /./)
{
    $spamtrap =~ s/(@.*)/lc($1)/e;

    if ($spamtrap =~ /[\n\s\"\']/)
    {
	$errmsg="\@BADCHARS\@";
    }
    else
    {
	my @new_spamtrap=
	    grep( !($_ =~ /^\s*spamtrap\s+(.*)$/ && $1 eq $spamtrap),
		  ReadMultiLineConfigFile("bofh"));
	push @new_spamtrap, "spamtrap $spamtrap";

	SaveMultiLineConfigFile("bofh", \@new_spamtrap);

	changed("");
    }
}

my $maxrcpts=$cgi->param("MAXRCPTS");
my $maxrcptshard=$cgi->param("MAXRCPTSHARD");

if ($cgi->param("addmaxrcpts"))
  {
    $maxrcpts += 0;

    my @new_maxrcpt=
      grep( !($_ =~ /^\s*maxrcpts\s+/),
	    ReadMultiLineConfigFile("bofh"));

    push @new_maxrcpt, ("maxrcpts $maxrcpts" .
			($maxrcptshard ? " hard":""))
      if $maxrcpts;

    SaveMultiLineConfigFile("bofh", \@new_maxrcpt);

    changed("");
  }

my $freemail=$cgi->param("freemail");

if ($freemail =~ /./)
{
    $freemail =~ s/\s//g;
    $freemail=lc($freemail);

    my $freemail2=lc($cgi->param("freemail2"));

    if (!($freemail =~ /^[a-z0-9\-\.]+$/) ||
       ($freemail2 =~ /./ && !($freemail2 =~ /^[a-z0-9\-\.\s]+$/)))
    {
	$errmsg="\@BADCHARS\@";
    }
    else
    {
	my @new_freemail=
	    grep( !($_ =~ /^\s*freemail\s+(.*)$/ && $1 eq $freemail),
		  ReadMultiLineConfigFile("bofh"));

	$freemail="$freemail $freemail2";

	$freemail=join(" ", grep(/./, split(/\s+/, $freemail)));

	push @new_freemail, "freemail $freemail";

	SaveMultiLineConfigFile("bofh", \@new_freemail);

	changed("");
    }
}

my $badmx=$cgi->param("badmx");

if ($badmx =~ /./)
{
    $badmx =~ s/\s//g;

    if ( ! ($badmx =~
	    /^([1-9][0-9]*)\.([1-9][0-9]*)\.([1-9][0-9]*)\.([1-9][0-9]*)$/)
	 || $1 > 255 || $2 > 255 || $3 > 255 || $4 > 255)
    {
	$errmsg="\@BADIP\@";
    }
    else
    {
	my @new_badmx=
	    grep( !($_ =~ /^\s*badmx\s+(.*)$/ && $1 eq $badmx),
		  ReadMultiLineConfigFile("bofh"));
	push @new_badmx, "badmx $badmx";

	SaveMultiLineConfigFile("bofh", \@new_badmx);

	changed("");
    }
}

if ($cgi->param("addbofhbadmime"))
  {
    my $bofhbadmime=$cgi->param("bofhbadmime");

    my @new_bofhbadmime=
      grep( !($_ =~ /^\s*opt\s+BOFHBADMIME=/),
	    ReadMultiLineConfigFile("bofh"));

    unshift @new_bofhbadmime, "opt BOFHBADMIME=$bofhbadmime"
      if $bofhbadmime eq "accept" || $bofhbadmime eq "reject";

    SaveMultiLineConfigFile("bofh", \@new_bofhbadmime);
    changed("");
  }

my $badfromhtml="<table border=\"0\" cellpadding=\"8\">";
my $badmxhtml="<table border=\"0\" cellpadding=\"8\">";
my $freemailhtml="<table border=\"0\" cellpadding=\"8\">";
my $spamtraphtml="<table border=\"0\" cellpadding=\"8\">";

my $maxrcptshtml="";
my $maxrcptshardhtml="";

my $bofhbadmimewrap="checked=\"checked\"";
my $bofhbadmimeaccept="";
my $bofhbadmimereject="";

my @a=ReadMultiLineConfigFile("bofh");

foreach (sort {
    my $c=$a;
    my $d=$b;
    $c =~ s/\s//g;
    $d =~ s/\s//g;
    return $c cmp $d; } @a)
{
    chomp;
    my $arg=$_;

    my $temp_cgi=new CGI ( { "remove" => $arg});

    if ($arg =~ /^\s*badfrom\s+(.*)/)
    {
	$domain=$1;

	$badfromhtml .= "<tr><td>" .
	    ($domain =~ /^\@\.(.*)/
	     ? ("<tt>@</tt>\@ANYTHING\@<tt>." . htmlescape($1))
	     : "<tt>" . htmlescape($domain))
	    . "</tt></td><td>- <a href=\"50bofh?"
	    . $temp_cgi->query_string() . "\">\@DELETE\@</a></td></tr>\n";
    }

    $badmxhtml .= "<tr><td><tt>" . htmlescape($1)
	. "</tt></td><td>- <a href=\"50bofh?"
	    . $temp_cgi->query_string() . "\">\@DELETE\@</a></td></tr>\n"
		if $arg =~ /^\s*badmx\s+(.*)/;

    $spamtraphtml .= "<tr><td><tt>" . htmlescape($1)
	. "</tt></td><td>- <a href=\"50bofh?"
	    . $temp_cgi->query_string() . "\">\@DELETE\@</a></td></tr>\n"
		if $arg =~ /^\s*spamtrap\s+(.*)/;

    if ($arg =~ /^\s*freemail\s+(.*)/)
      {
	my $freemail=$1;
	my @f=grep(/./, split(/\s+/, $freemail));

	$freemail=shift @f;

	$freemailhtml .= "<tr><td><tt>" . htmlescape($freemail)
	  . ($#f >= 0 ? "&nbsp;&nbsp;" .
	     htmlescape("(" . join(" ", @f) . ")"):"")
	    . "</tt></td><td>- <a href=\"50bofh?"
	      . $temp_cgi->query_string() . "\">\@DELETE\@</a></td></tr>\n";
      }

    if ($arg =~ /^\s*maxrcpts\s+(.*)/)
      {
	my @n=split(/\s+/, $1);

	$maxrcptshtml=shift @n;

	$maxrcptshardhtml="checked=\"checked\"" if lc(shift @n) eq "hard";
      }

    if ($arg =~ /^\s*opt\s+BOFHBADMIME=(.*)/)
      {
	my $val=$1;

	$bofhbadmimewrap="";
	$bofhbadmimeaccept="";
	$bofhbadmimereject="";

	if ($val eq "accept")
	  {
	    $bofhbadmimeaccept="checked=\"checked\"";
	  }
	elsif ($val eq "reject")
	  {
	    $bofhbadmimereject="checked=\"checked\"";
	  }
	else
	  {
	    $bofhbadmimewrap="checked=\"checked\"";
	  }
      }
}

$badfromhtml .= "</table>\n";
$badmxhtml .= "</table>\n";
$freemailhtml .= "</table>\n";
$spamtraphtml .= "</table>\n";

$maxrcptshardhtml="<input type=\"checkbox\" name=\"MAXRCPTSHARD\" $maxrcptshardhtml />";

my $me=ReadOneLineConfigFile("me");

$me=`hostname 2>/dev/null` unless $me =~ /./;

chomp $me;
$me=lc($me);

display_form("admin-50bofh.html",
	     {
	      "ERRMSG" => $errmsg,
	      "BADFROM" => $badfromhtml,
	      "BADMX" => $badmxhtml,
	      "FREEMAIL" => $freemailhtml,
	      "SPAMTRAP" => $spamtraphtml,

	      "BOFHBADMIMEWRAP" => "<input type=\"radio\" name=\"bofhbadmime\" value=\"wrap\" $bofhbadmimewrap />",

	      "BOFHBADMIMEACCEPT" => "<input type=\"radio\" name=\"bofhbadmime\" value=\"accept\" $bofhbadmimeaccept />",

	      "BOFHBADMIMEREJECT" => "<input type=\"radio\" name=\"bofhbadmime\" value=\"reject\" $bofhbadmimereject />",

	      "MAXRCPTS" => $maxrcptshtml,
	      "MAXRCPTSHARD" => $maxrcptshardhtml,
	      "ME" => htmlescape("\@$me")
	     }
	    );
