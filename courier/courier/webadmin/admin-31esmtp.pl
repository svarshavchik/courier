#! perl
#
# TITLE: Outbound ESMTP
#
#
# Copyright 2001 Double Precision, Inc.  See COPYING for
# distribution information.

use webadmin;

my $errstr="";
local $changed=0;

sub readroutes {

    my @routes;

    my $fh=OpenConfigFile("esmtproutes");

    if (defined $fh)
    {
	foreach (<$fh>)
	{
	    my %route;
	    my $l=$_;

	    push @routes, \%route;

	    $route{'txt'}=$l;

	    next if $l =~ /^\s+\#/;
	    chomp $l;

	    next unless $l =~ /^([^:\#\s]+)?\s*:\s*([^,\/\s]+)?(,[^\/]*)?(\/[^\s]*)?\s*(\#.*)?$/;
	    my ($domain, $route, $port, $opts)=($1, $2, $3, $4);

	    $opts =~ s/^,//;

	    $route{'domain'}=$domain if $domain =~ /./;
	    $route{'route'}=$route if $route =~ /./;
	    $route{'port'}=$port if $port =~ /./;
	    $route{'flag'}=1;

	    $opts =~ s/^\///;

	    foreach( split(/\s,/, $opts))
	    {
		next unless $_ =~ /(.+)=(.+)/;
		$route{"OPT$1"}=$2;
	    }
	}
	close($fh);
    }

    return \@routes;
}

sub saveval {
    my $val=shift;
    my $file=shift;

    my $cur=ReadOneLineConfigFile($file);

    if ($cur =~ /./)
    {
	if ($val =~ /./)
	{
	    if ($val ne $cur)
	    {
		SaveOneLineConfigFile($file, $val);
		changed("");
		$changed=1;
	    }
	}
	else
	{
	    DeleteConfigFile($file);
	    changed("");
	    $changed=1;
	}
    }
    else
    {
	if ($val =~ /./)
	{
		SaveOneLineConfigFile($file, $val);
		changed("");
		$changed=1;
	}
    }
}

sub savetimeval {
    my $name=shift;
    my $file=shift;

    my $val=$cgi->param("${name}VAL");
    my $hms=$cgi->param("${name}HMS");

    return unless defined $hms;

    $val += 0;

    $val = $val ? "$val$hms":"";

    saveval($val, $file);
}

sub savestrval {
    my $name=shift;
    my $file=shift;

    my $val=$cgi->param($name);

    return unless defined $val;

    saveval($val, $file);
}

savetimeval("ESMTPDELAY", "esmtpdelay");
savetimeval("ESMTPTIMEOUTCONNECT", "esmtptimeoutconnect");
savetimeval("ESMTPTIMEOUTHELO", "esmtptimeouthelo");
savestrval("ESMTPHELO", "esmtphelo");
savetimeval("ESMTPTIMEOUTKEEPALIVE", "esmtptimeoutkeepalive");
savetimeval("ESMTPTIMEOUTKEEPALIVEPING", "esmtptimeoutkeepaliveping");
savetimeval("ESMTPTIMEOUTQUIT", "esmtptimeoutquit");

if ($cgi->param("AddRoute"))
{
    my $domain;
    my $route;

    if ($cgi->param("AddRoute") eq "DEFAULT")
    {
	$domain="";
    }
    else
    {
	$domain=validhostname(param("domain"));
	$errstr="\@BADDOMAIN\@" unless $domain =~ /./;
    }

    if (! $errstr)
    {
	$route=param("routedomain");
	if ($route =~ /./)
	{
	    $route=validhostname($route);
	    $errstr="\@BADDOMAIN\@" unless $route =~ /./;
	}
	else
	{
	    $route=param("routeip");

	    if ($route =~ /./)
	    {
		$errstr="\@BADDOMAIN\@" unless $route =~ /^[a-fA-f0-9\.\:]+$/;

		$route="[$route]";
	    }
	    else
	    {
		$route="";
	    }
	}
    }
    $route .= "," . param("port") if param("port");
    $route .= "/SECURITY=STARTTLS" if param("SECURITYSTARTTLS");
    $route .= "/SECURITY=NONE" if param("SECURITYNONE");

    unless ($errstr)
    {
	my @l=ReadMultiLineConfigFile("esmtproutes");
	push @l, "${domain}: $route";
	SaveMultiLineConfigFile("esmtproutes", \@l);
	changed("");
    }
}

if ($cgi->param("deleteroute"))
{
    my $key=$cgi->param("deleteroute");

    my $routes=readroutes();

    my $i;

    for ($i=0; $i <= $#{$routes}; $i++)
    {
	my $h=$$routes[$i];
	next unless $$h{'flag'};

	my $k="$$h{'domain'},$$h{'route'},$$h{'port'}";

	if ($k eq $key)
	{
	    splice @$routes, $i, 1;
	    last;
	}
    }

    my @l;

    foreach (@$routes)
    {
	my $s=$$_{'txt'};
	chomp $s;
	push @l, $s;
    }

    if ($#l >= 0)
    {
	SaveMultiLineConfigFile("esmtproutes", \@l);
    }
    else
    {
	DeleteConfigFile("esmtproutes");
    }
    changed("");
}

if ($cgi->param("MoveUp") || $cgi->param("MoveDown"))
{
    my $key=$cgi->param("key");

    my $routes=readroutes();

    my $i;

    for ($i=0; $i <= $#{$routes}; $i++)
    {
	my $h=$$routes[$i];
	next unless $$h{'flag'};

	my $k="$$h{'domain'},$$h{'route'},$$h{'port'}";

	if ($k eq $key)
	{
	    if ($cgi->param("MoveUp"))
	    {
		$j= $i > 0 ? $i-1:$i;
	    }
	    else
	    {
		$j= $i < $#{$routes} ? $i+1:$i;
	    }

	    my $save=$$routes[$i];
	    $$routes[$i]=$$routes[$j];
	    $$routes[$j]=$save;
	    last;
	}
    }

    my @l;

    foreach (@$routes)
    {
	my $s=$$_{'txt'};
	chomp $s;
	push @l, $s;
    }

    SaveMultiLineConfigFile("esmtproutes", \@l);
    changed("");
}

$errstr="\@SAVED\@" if $changed;

sub mktimevalinput {
    my $name=shift;
    my $time=shift;
    my $interval="s";

    ($time,$interval)=($1,lc($2))
	if $time =~ /([0-9]+)([hHmMsS]?)$/;

    $time += 0;

    $time="" unless $time;

    $interval="s" unless $time;

    my $str="<input type=\"text\" name=\"${name}VAL\" value=\"$time\" size=\"3\" />";

    $str .= "<select name=\"${name}HMS\"><option value=\"S\">\@SECONDS\@</option><option value=\"M\""
	. ($interval eq "m" ? " selected=\"selected\"":"")
	    . ">\@MINUTES\@</option><option value=\"H\""
		. ($interval eq "h" ? " selected=\"selected\"":"") . ">\@HOURS\@</option></select>";
    return $str;
}

my $helomsg=ReadOneLineConfigFile("esmtphelo");

$helomsg=htmlescape($helomsg);

my $routes=readroutes();
my $routeHTML="<table border=\"0\" cellspacing=\"16\">\n";

foreach (@$routes)
{
    my $h=$_;
    my $temp_cgi;

    unless ($$h{'flag'})
    {
	$routeHTML .= "<tr><td>&nbsp;</td><td colspan=\"3\"><em>"
	    . htmlescape($$h{'txt'}) . "</em></td></tr>\n";
	next;
    }


    $routeHTML .= "<tr><td valign=\"middle\"><form method=\"post\" action=\"31esmtp\"><input type=\"hidden\" name=\"key\" value=\"" . htmlescape( "$$h{'domain'},$$h{'route'},$$h{'port'}" )
	. "\" /><input type=\"submit\" name=\"MoveUp\" value=\"\@UP\@\" /><input type=\"submit\" name=\"MoveDown\" value=\"\@DOWN\@\" /></form></td>";
    $routeHTML .= "<td>"
	. ($$h{'domain'} =~ /./ ? "":"\@DEFAULTSMARTHOST\@")
	    . "<tt>$$h{'domain'}: $$h{'route'}$$h{'port'}</tt>"
		. ($$h{'route'} =~ /./ ? "":"\@DEFAULTMX\@");

    my %sec;

    grep ( $sec{$_}=1, split(/[,\s]+/, $$h{"OPTSECURITY"}));

    $routeHTML .= "<br />&nbsp;&nbsp;&nbsp;&nbsp;\@SECURE\@"
	if $sec{"STARTTLS"};
    $routeHTML .= "<br />&nbsp;&nbsp;&nbsp;&nbsp;\@UNSECURE\@"
	if $sec{"NONE"};

    $temp_cgi=new CGI({
	"deleteroute" => "$$h{'domain'},$$h{'route'},$$h{'port'}"
	});

    $routeHTML .= "</td><td valign=\"middle\"><a href=\"31esmtp?"
	. $temp_cgi->query_string() . "\">\@DELETE\@</a></td></tr>\n";
}

$routeHTML .= "</table>\n";

display_form("admin-31esmtp.html",
	     {
		 "ESMTPROUTES" => $routeHTML,

		 "ESMTPDELAY" => mktimevalinput("ESMTPDELAY", ReadOneLineConfigFile("esmtpdelay")),
		 "ESMTPTIMEOUTCONNECT" => mktimevalinput("ESMTPTIMEOUTCONNECT", ReadOneLineConfigFile("esmtptimeoutconnect")),
		 "ESMTPTIMEOUTHELO" => mktimevalinput("ESMTPTIMEOUTHELO", ReadOneLineConfigFile("esmtptimeouthelo")),
		 "ESMTPHELO" => $helomsg,
		 "ESMTPTIMEOUTKEEPALIVE" => mktimevalinput("ESMTPTIMEOUTKEEPALIVE", ReadOneLineConfigFile("esmtptimeoutkeepalive")),
		 "ESMTPTIMEOUTKEEPALIVEPING" => mktimevalinput("ESMTPTIMEOUTKEEPALIVEPING", ReadOneLineConfigFile("esmtptimeoutkeepaliveping")),
		 "ESMTPTIMEOUTQUIT" => mktimevalinput("ESMTPTIMEOUTQUIT", ReadOneLineConfigFile("esmtptimeoutquit")),

		 "ERROR" => $errstr,
	     }
	     );
