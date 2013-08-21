#! perl
#
# TITLE: Webmail
#
#
# Copyright 2001-2005 Double Precision, Inc.  See COPYING for
# distribution information.


use webadmin;

my $errmsg="";

if ($cgi->param("submit"))
{
    if ($cgi->param("maildirfilterconfig"))
    {
	my $fh=OpenConfigFile("maildirfilterconfig");

	if ($fh)	# Already exists
	{
	    close($fh);
	}
	else
	{
	    SaveMultiLineConfigFile("maildirfilterconfig",
				    [ "MAILDIRFILTER=../.mailfilter",
				      "MAILDIR=./Maildir" ]);
	}
    }
    else
    {
	DeleteConfigFile("maildirfilterconfig");
    }

    my $qc=$cgi->param("AUTORESPONSEQUOTAC")+0;

    my $qs=$cgi->param("AUTORESPONSEQUOTAS")+0;

    $qc=0 if $qc < 0;
    $qs=0 if $qs < 0;

    if ($qc > 0 || $qs > 0)
    {
	$qc= $qc ? $qc . "C":"";
	$qs= $qs ? $qs . "S":"";
	SaveOneLineConfigFile("autoresponsequota", "$qc$qs");
    }
    else
    {
	DeleteConfigFile("autoresponsequota");
    }

    my $authdaemonvars=ReadEnvVarConfigFile("authdaemonrc");
 
    my %defaultoptions;

    foreach (split(/,/, $$authdaemonvars{'DEFAULTOPTIONS'}))
    {
	$defaultoptions{$1}=$2 if /(.*)=(.*)/;
    }

    if ($cgi->param("NOCHANGINGFROM"))
    {
	delete $defaultoptions{"wbnochangingfrom"};
    }
    else
    {
	$defaultoptions{"wbnochangingfrom"}=1;
    }

    if ($cgi->param("USEXSENDER"))
    {
	$defaultoptions{"wbusexsender"}=1;
    }
    else
    {
	delete $defaultoptions{"wbusexsender"};
    }

    if ($cgi->param("IMAGES"))
    {
	delete $defaultoptions{"wbnoimages"};
    }
    else
    {
	$defaultoptions{"wbnoimages"}=1;
    }

    my $v="";

    foreach (keys %defaultoptions)
    {
	$v .= "," if $v;
	$v .= "$_=$defaultoptions{$_}";
    }

    ReplaceEnvVarConfigFile("authdaemonrc", "DEFAULTOPTIONS", $v);
    changed("$authdaemond stop; $authdaemond start");

    my @logindomainlist=split(/\n/, $cgi->param("logindomainlist"));

    grep { s/\s+$//; s/^\s+//; } @logindomainlist;
    grep { $_=lc($_); } @logindomainlist;

    if ($#logindomainlist >= 0)
    {
	SaveMultiLineConfigFile("logindomainlist", \@logindomainlist);
    }
    else
    {
	DeleteConfigFile("logindomainlist", \@logindomainlist);
    }

    my $calendarmode=$cgi->param("calendarmode");

    if ($calendarmode eq "local" || $calendarmode eq "net")
    {
	SaveOneLineConfigFile("calendarmode", $calendarmode);
    }
    else
    {
	DeleteConfigFile("calendarmode");
    }
    changed("test -x ${libexecdir}/courier/pcpd || exit 0; ${sbindir}/webmaild stop; ${sbindir}/webmaild start");
    $errmsg="\@SAVED\@";
}

my $autoresponsequota=ReadOneLineConfigFile("autoresponsequota");

my $AUTORESPONSEQUOTAC="";
my $AUTORESPONSEQUOTAS="";

while ($autoresponsequota =~ /([0-9]+)(.)(.*)/)
{
    my ($n, $c, $rest)=($1,$2,$3);

    $autoresponsequota=$rest;

    $AUTORESPONSEQUOTAC=$n if $c eq "C";
    $AUTORESPONSEQUOTAS=$n if $c eq "S";
}

sub cexists {
    my $n=shift;

    my $fh=OpenConfigFile($n);

    if ($fh)
    {
	close($fh);
	return 1;
    }
    return undef;
}


my $authdaemonvars=ReadEnvVarConfigFile("authdaemonrc");
 
my %defaultoptions;

foreach (split(/,/, $$authdaemonvars{'DEFAULTOPTIONS'}))
{
    $defaultoptions{$1}=$2 if /(.*)=(.*)/;
}



my $maildirfilterconfig="<input type=\"checkbox\" name=\"maildirfilterconfig\""
    . ( cexists("maildirfilterconfig") ? " checked=\"checked\"":"") . " />";
my $NOCHANGINGFROM="<input type=\"checkbox\" name=\"NOCHANGINGFROM\""
    . ( $defaultoptions{"wbnochangingfrom"} ? "":" checked=\"checked\"") . " />";
my $USEXSENDER="<input type=\"checkbox\" name=\"USEXSENDER\""
    . ( $defaultoptions{"wbusexsender"} ? " checked=\"checked\"":"") . " />";
my $IMAGES="<input type=\"checkbox\" name=\"IMAGES\""
    . ( $defaultoptions{"wbnoimages"} ? "":" checked=\"checked\"") . " />";
my $logindomainlist="";

my $fh=OpenConfigFile("logindomainlist");

$logindomainlist="<textarea rows=\"12\" name=\"logindomainlist\" cols=\"60\">";

if ($fh)
{
    $logindomainlist .= htmlescape(join("", <$fh>));
    close($fh);
}
$logindomainlist .= "</textarea>";

my $calendarmode=ReadOneLineConfigFile("calendarmode");

my $calendarmodeHtml="<select name=\"calendarmode\">"
  . "<option value=\"\">\@CALDISABLED\@</option>"
  . "<option value=\"local\"" . ($calendarmode eq "local"
				  ? " selected='selected'":"")
  . ">\@CALLOCAL\@</option>"
  . "<option value=\"net\"" . ($calendarmode eq "net"
				? " selected='selected'":"")
  . ">\@CALNET\@</option></select>";

display_form("admin-47webmail.html",
	     {
		 "ERRMSG" => $errmsg,
		 "maildirfilterconfig" => $maildirfilterconfig,
		 "AUTORESPONSEQUOTAC" => $AUTORESPONSEQUOTAC,
		 "AUTORESPONSEQUOTAS" => $AUTORESPONSEQUOTAS,

		 "NOCHANGINGFROM" => $NOCHANGINGFROM,
		 "USEXSENDER" => $USEXSENDER,
		 "IMAGES" => $IMAGES,
		 "logindomainlist" => $logindomainlist,

		 "calendarmode" => $calendarmodeHtml,
	     }
	     );
