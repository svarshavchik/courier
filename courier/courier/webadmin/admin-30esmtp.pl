#! perl
#
# TITLE: Inbound ESMTP
#
#
# Copyright 2001-2004 Double Precision, Inc.  See COPYING for
# distribution information.

use webadmin;

eval 'use Net::CIDR';

# Parse the BLACKLISTS string

sub parsebl {
    my $str=shift;

    my @bl;

    while ($str)
    {
	if ($str =~ /^\s+(.*)/)
	{
	    $str=$1;
	    next;
	}

	my $i=0;
	my $inq=0;

	for ($i=0; $i < length($str); $i++)
	{
	    $inq ^= 1 if substr($str, $i, 1) eq "\"";
	    last if substr($str, $i, 1) =~ /\s/ && !$inq;
	    $i++ if substr($str, $i, 1) eq "\\";
	}

	my $s=substr($str, 0, $i);
	$str=substr($str, $i);

	my $hash={ "STR" => $s };

	next unless $s =~ /^-block=(.*)/;

	$s=$1;

	my $zone;
	my $var="BLOCK";
	my $ip;
	my $msg;

	$zone=$s;

	if ($s =~ /^([^,]*),(.*)/)
	{
	    $zone=$1;
	    $var=$2;

	    if ($var =~ /^([^,]*),(.*)/)
	    {
		$var=$1;
		$msg=$2;
	    }

	    if ($var =~ /(.*)\/(.*)/)
	    {
		$var=$1;
		$ip=$2;
	    }
	}

	$$hash{'ZONE'}=$zone;
	$$hash{'VAR'}=$var;
	$$hash{'IP'}=$ip;
	$$hash{'MSG'}=$msg;

	push @bl, $hash;
    }
    return @bl;
}

my $errstr="";
my $changed=0;
my $changedsmtp=0;

if ($cgi->param("Save"))
{
    ReplaceEnvVarConfigFile("esmtpd", "ESMTPDSTART",
			    $cgi->param("ENABLED") ? "YES":"NO");
    ReplaceEnvVarConfigFile("esmtpd-ssl", "ESMTPDSSLSTART",
			    $cgi->param("ENABLED_SSL") ? "YES":"NO");
    ReplaceEnvVarConfigFile("esmtpd-msa", "ESMTPDSTART",
			    $cgi->param("ENABLED_MSA") ? "YES":"NO");

    ReplaceEnvVarConfigFile("esmtpd", "NOADDMSGID",
			    $cgi->param("MSGID") ? "0":"1");
    ReplaceEnvVarConfigFile("esmtpd-ssl", "NOADDMSGID",
			    $cgi->param("MSGID_SSL") ? "0":"1");
    ReplaceEnvVarConfigFile("esmtpd-msa", "NOADDMSGID",
			    $cgi->param("MSGID_MSA") ? "0":"1");

    ReplaceEnvVarConfigFile("esmtpd", "NOADDDATE",
			    $cgi->param("DATE") ? "0":"1");
    ReplaceEnvVarConfigFile("esmtpd-ssl", "NOADDDATE",
			    $cgi->param("DATE_SSL") ? "0":"1");
    ReplaceEnvVarConfigFile("esmtpd-msa", "NOADDDATE",
			    $cgi->param("DATE_MSA") ? "0":"1");

    ReplaceEnvVarConfigFile("esmtpd", "AUTH_REQUIRED",
			    $cgi->param("AUTH") ? "1":"0");
    ReplaceEnvVarConfigFile("esmtpd-ssl", "AUTH_REQUIRED",
			    $cgi->param("AUTH_SSL") ? "1":"0");
    ReplaceEnvVarConfigFile("esmtpd-msa", "AUTH_REQUIRED",
			    $cgi->param("AUTH_MSA") ? "1":"0");

    ReplaceEnvVarConfigFile("esmtpd", "BOFHCHECKDNS",
			    $cgi->param("CHECKDNS") ? "1":"0");
    ReplaceEnvVarConfigFile("esmtpd-ssl", "BOFHCHECKDNS",
			    $cgi->param("CHECKDNS_SSL") ? "1":"0");
    ReplaceEnvVarConfigFile("esmtpd-msa", "BOFHCHECKDNS",
			    $cgi->param("CHECKDNS_MSA") ? "1":"0");

    ReplaceEnvVarConfigFile("esmtpd", "MAXDAEMONS", $cgi->param("MAXDAEMONS"));
    ReplaceEnvVarConfigFile("esmtpd-ssl", "MAXDAEMONS",
			    $cgi->param("MAXDAEMONS_SSL"));
    ReplaceEnvVarConfigFile("esmtpd-msa", "MAXDAEMONS",
			    $cgi->param("MAXDAEMONS_MSA"));
    ReplaceEnvVarConfigFile("esmtpd", "MAXPERIP", $cgi->param("MAXPERIP"));
    ReplaceEnvVarConfigFile("esmtpd-ssl", "MAXPERIP",
			    $cgi->param("MAXPERIP_SSL"));
    ReplaceEnvVarConfigFile("esmtpd-msa", "MAXPERIP",
			    $cgi->param("MAXPERIP_MSA"));
    ReplaceEnvVarConfigFile("esmtpd", "MAXPERC", $cgi->param("MAXPERC"));
    ReplaceEnvVarConfigFile("esmtpd-ssl", "MAXPERC",
			    $cgi->param("MAXPERC_SSL"));
    ReplaceEnvVarConfigFile("esmtpd-msa", "MAXPERC",
			    $cgi->param("MAXPERC_MSA"));

    $dodrop=$cgi->param("DROP") || "";

    ReplaceEnvVarConfigFile("esmtpd", "DROP", $dodrop ? "-drop":"");

    my $esmtp=ReadEnvVarConfigFile("esmtpd");


    my @a;

    foreach (grep (/./,split(/\s+/, $$esmtp{'ESMTPAUTH_WEBADMIN'})))
    {
	push @a, $_ if $cgi->param("SASL_$_");
    }

    my $esmtpauth=join(" ", @a);

    ReplaceEnvVarConfigFile("esmtpd", "ESMTPAUTH", $esmtpauth);

    @a=();

    foreach (grep (/./,split(/\s+/, $$esmtp{'ESMTPAUTH_TLS_WEBADMIN'})))
    {
	push @a, $_ if $cgi->param("SASL_TLS_$_");
    }

    $esmtpauth=join(" ", @a);

    ReplaceEnvVarConfigFile("esmtpd", "ESMTPAUTH_TLS", $esmtpauth);

    $changed=1;

}

if ($cgi->param("Add"))
{
    my $esmtp=ReadEnvVarConfigFile("esmtpd");
    my $bl=$$esmtp{'BLACKLISTS'};

    my $radio=$cgi->param("AddBl");

    $bl .= " " if $bl;

    $radio =~ s/\s//g;

    if ($radio eq "")
      {
      }
    elsif ($radio ne "CUSTOM")
    {
	my $suffix="";

	($radio, $suffix)=($1, $2) if $radio =~ /(.*)(\/.*)/;

	$bl .= "-block=$radio,BLOCK$suffix";
	$changed=1;
	ReplaceEnvVarConfigFile("esmtpd", "BLACKLISTS", $bl);
    }
    else
    {
	my $zone=$cgi->param("zone");
	my $ip=$cgi->param("ip");
	my $msg=$cgi->param("msg");

	$ip =~ s/\s//g;

	$ip="" if ! defined $ip;
	$msg="" if ! defined $msg;

	$msg .= s/\"/\'/g;
	$msg .= s/[\\\*\?\~]/ /g;

	if (($zone=validhostname($zone)) &&
	    ($ip eq "" || $ip =~ /^[0-9\.]+$/))
	{
	    $ip="/$ip" if $ip ne "";
	    $bl .= "-block=$zone,BLOCK$ip";
	    $bl .= ",\"$msg\"" if $msg ne "";
	    $changed=1;
	    ReplaceEnvVarConfigFile("esmtpd", "BLACKLISTS", $bl);
	}
	else
	{
	    $errstr="\@INVALID\@";
	}
    }
}

if ($cgi->param("AddAccess"))
{
    my $ip=$cgi->param("netblock");
    my $v=$cgi->param("value");

    $ip =~ s/\s//g;

    $ip =~ s/\.$//g;

    if ($ip =~ /[\-\/]/)
    {
	eval { my @foo=Net::CIDR::range2cidr($ip); } ;
	$ip=undef if $@;
    }

    if ($ip =~ /^[a-fA-F0-9\.:\-\/]+$/ || $ip eq "*")
    {
	if ($v eq "BOUNCE")
	{
	    $v=$cgi->param("msg");
	    $v =~ s/\"//g;
	    $v =~ s/\'//g;
	    $v =~ s/\\/ /g;
	    $v="allow,BLOCK=\"$v\"";
	}

	my $bofh=$cgi->param("bofhbadmime");

	$v .= ",BOFHBADMIME=$bofh"
	  if $bofh eq "accept" || $bofh eq "reject";

	my $l=join("\n",
		   ReadMultiLineConfigFile("smtpaccess/webadmin"),
		   "$ip\t$v\n");

	my $fh=NewConfigFile("smtpaccess/webadmin");

	print $fh $l;
	close($fh);
	$changedsmtp=1;
    }
    else
    {
	$errstr="\@INVALID\@";
    }
}

my $delbl=$cgi->param("deleteblacklist");

if ($delbl)
{
    my $esmtp=ReadEnvVarConfigFile("esmtpd");

    my @newbl;

    foreach (parsebl($$esmtp{'BLACKLISTS'}))
    {
	my $zone=$$_{'ZONE'};
	my $ip=$$_{'IP'};
	my $str=$$_{'STR'};

	$ip="/$ip" if $ip;

	next if $delbl eq "$zone$ip";

	push @newbl, $str;
    }

    ReplaceEnvVarConfigFile("esmtpd", "BLACKLISTS", join(" ", @newbl));
    $changed=1;
}

if ($delbl=$cgi->param("deletesmtpaccess"))
{
    my @l;

    foreach (ReadMultiLineConfigFile("smtpaccess/webadmin"))
    {
	my $l=$_;

	next if $l =~ /^([^\s]+)\s/ && $1 eq $delbl;
	push @l, $l;
    }

    my $fh=NewConfigFile("smtpaccess/webadmin");

    print $fh join("\n", @l, "");
    close($fh);
    $changedsmtp=1;
}

if ($changedsmtp)
{
    changed("$sbindir/makesmtpaccess");
    changed("$sbindir/makesmtpaccess-msa");
    $errstr="\@SAVED\@";
}

if ($changed)
{
    changed("$sbindir/esmtpd stop ; . $sysconfdir/esmtpd ; test \"\$ESMTPDSTART\" != YES || $sbindir/esmtpd start");
    changed("$sbindir/esmtpd-msa stop ; . $sysconfdir/esmtpd-msa ; test \"\$ESMTPDSTART\" != YES || $sbindir/esmtpd-msa start");
    changed("$sbindir/esmtpd-ssl stop ; . $sysconfdir/esmtpd-ssl ; test \"\$ESMTPDSSLSTART\" != YES || $sbindir/esmtpd-ssl start");

    $errstr="\@SAVED\@";
}

$changed=0;

if ($cgi->param("addacceptmailfor"))
{
    addacceptmailfor($cgi->param("addacceptmailfor"));
    $changed=1;
}

if ($cgi->param("delacceptmailfor"))
{
    delacceptmailfor($cgi->param("delacceptmailfor"));
    $changed=1;
}




my $esmtp=ReadEnvVarConfigFile("esmtpd");
my $ssl=ReadEnvVarConfigFile("esmtpd-ssl");
my $msa=ReadEnvVarConfigFile("esmtpd-msa");
my $dodrop=$$esmtp{"DROP"} || "";

my $esmtpauth="";
my $esmtpauth_tls="";

my %esmtpauthhash;

grep {
    $esmtpauthhash{$_}=1;
} grep (/./, split(/\s+/, $$esmtp{'ESMTPAUTH'}));

grep {
    $esmtpauth .= "<input type=\"checkbox\" name='SASL_$_'"
	. ($esmtpauthhash{$_} ? " checked=\"checked\"":"") . " />&nbsp;<tt>$_</tt><br />";
} sort grep (/./, split(/\s+/, $$esmtp{'ESMTPAUTH_WEBADMIN'}));

%esmtpauthhash = ();

grep {
    $esmtpauthhash{$_}=1;
} grep (/./, split(/\s+/, $$esmtp{'ESMTPAUTH_TLS'}));

grep {
    $esmtpauth_tls .= "<input type=\"checkbox\" name='SASL_TLS_$_'"
	. ($esmtpauthhash{$_} ? " checked=\"checked\"":"") . " />&nbsp;<tt>$_</tt><br />";
} sort grep (/./, split(/\s+/, $$esmtp{'ESMTPAUTH_TLS_WEBADMIN'}));

my $blacklists="<table border=\"0\" cellspacing=\"16\">";

foreach (parsebl($$esmtp{'BLACKLISTS'}))
{
    my $zone=$$_{'ZONE'};
    my $ip=$$_{'IP'};

    $ip="/$ip" if $ip;

    my $temp_cgi=new CGI({
	"deleteblacklist" => "$zone$ip"});

    $blacklists .= "<tr><td colspan=\"2\"><tt>$zone$ip</tt></td><td><a href=\"30esmtp?"
	. $temp_cgi->query_string . "\">\@DELETE\@</a></td></tr>";
}

foreach (ReadMultiLineConfigFile("smtpaccess/webadmin"))
{
    next unless /^([a-fA-F0-9\.\:\-\/]+)\s+(.*)/;

    my ($ip, $action)=($1, $2);

    my $temp_cgi=new CGI({"deletesmtpaccess" => "$ip"});

    $blacklists .= "<tr><td><tt>$ip</tt></td><td><tt>$action</tt></td><td><a href=\"30esmtp?" . $temp_cgi->query_string . "\">\@DELETE\@</a></td></tr>";
}

$blacklists .= "</table>";

my %acceptdomains;

foreach (ReadMultiLineConfigFile("locals"))
{
    $acceptdomains{$_}="\@ISLOCAL\@";
}

foreach (ReadMultiLineConfigFile("hosteddomains/webadmin"))
{
    $acceptdomains{$_}="\@ISHOSTED\@";
}

foreach (keys %{readaliases()})
{
    next unless $_ =~ /^\@(.*)/;

    $acceptdomains{$1}="\@ISVIRTUAL\@";
}

my $acceptHTML="<table border=\"0\" cellspacing=\"16\">";

my @l=ReadMultiLineConfigFile("esmtpacceptmailfor.dir/webadmin");

foreach (sort @l)
{
    my $d=$_;

    my $args=new CGI( { "delacceptmailfor" => $d } );

    $acceptHTML .= "<tr><td><tt>$_</tt></td><td>"
	. ($acceptdomains{$d} ? $acceptdomains{$d}:
	   "<a href=\"30esmtp?" . $args->query_string() . "\">\@DELETE\@</a>")
	    . "</td></tr>\n";
}

$acceptHTML .= "</table>\n";

display_form("admin-30esmtp.html",
	     {
		 "ACCEPTMAILFORLIST" => $acceptHTML,
		 "BLACKLISTS" => $blacklists,
		 "NOTDROP" => "<input type=\"radio\" name='DROP' value=\"\""
		     . (!$dodrop ? " checked=\"checked\"":"")
		     . " />",
		 "DROP" => "<input type=\"radio\" name='DROP' value=\"drop\""
		     . ($dodrop ? " checked=\"checked\"":"")
		     . " />",
		 "ENABLED" => "<input type=\"checkbox\" name='ENABLED'" . (($$esmtp{"ESMTPDSTART"} =~ /[yY]/) ? " checked=\"checked\"":"") . " />",
		 "ENABLED_SSL" => "<input type=\"checkbox\" name='ENABLED_SSL'" . ($$ssl{"ESMTPDSSLSTART"} =~ /[yY]/ ? " checked=\"checked\"":"") . " />",
		 "ENABLED_MSA" => "<input type=\"checkbox\" name='ENABLED_MSA'" . ($$msa{"ESMTPDSTART"} =~ /[yY]/ ? " checked=\"checked\"":"") . " />",

		 "MSGID" => "<input type=\"checkbox\" name='MSGID'" . ($$esmtp{"NOADDMSGID"} ? "":" checked=\"checked\"") . " />",
		 "MSGID_SSL" => "<input type=\"checkbox\" name='MSGID_SSL'" . ($$ssl{"NOADDMSGID"} ? "":" checked=\"checked\"") . " />",
		 "MSGID_MSA" => "<input type=\"checkbox\" name='MSGID_MSA'" . ($$msa{"NOADDMSGID"} ? "":" checked=\"checked\"") . " />",
		 "DATE" => "<input type=\"checkbox\" name='DATE'" . ($$esmtp{"NOADDDATE"} ? "":" checked=\"checked\"") . " />",
		 "DATE_SSL" => "<input type=\"checkbox\" name='DATE_SSL'" . ($$ssl{"NOADDDATE"} ? "":" checked=\"checked\"") . " />",
		 "DATE_MSA" => "<input type=\"checkbox\" name='DATE_MSA'" . ($$msa{"NOADDDATE"} ? "":" checked=\"checked\"") . " />",

		 "AUTH" => "<input type=\"checkbox\" name='AUTH'" . ($$esmtp{"AUTH_REQUIRED"} ? " checked=\"checked\"":"") . " />",
		 "AUTH_SSL" => "<input type=\"checkbox\" name='AUTH_SSL'" . ($$ssl{"AUTH_REQUIRED"} ? " checked=\"checked\"":"") . " />",
		 "AUTH_MSA" => "<input type=\"checkbox\" name='AUTH_MSA'" . ($$msa{"AUTH_REQUIRED"} ? " checked=\"checked\"":"") . " />",

		 "CHECKDNS" => "<input type=\"checkbox\" name='CHECKDNS'" . ($$esmtp{"BOFHCHECKDNS"} ? " checked=\"checked\"":"") . " />",
		 "CHECKDNS_SSL" => "<input type=\"checkbox\" name='CHECKDNS_SSL'" . ($$ssl{"BOFHCHECKDNS"} ? " checked=\"checked\"":"") . " />",
		 "CHECKDNS_MSA" => "<input type=\"checkbox\" name='CHECKDNS_MSA'" . ($$msa{"BOFHCHECKDNS"} ? " checked=\"checked\"":"") . " />",

		 "ESMTPAUTH" => $esmtpauth,
		 "ESMTPAUTH_TLS" => $esmtpauth_tls,

		 "MAXDAEMONS" => $$esmtp{'MAXDAEMONS'},
		 "MAXDAEMONS_SSL" => $$ssl{'MAXDAEMONS'},
		 "MAXDAEMONS_MSA" => $$msa{'MAXDAEMONS'},
		 "MAXPERIP" => $$esmtp{'MAXPERIP'},
		 "MAXPERIP_SSL" => $$ssl{'MAXPERIP'},
		 "MAXPERIP_MSA" => $$msa{'MAXPERIP'},
		 "MAXPERC" => $$esmtp{'MAXPERC'},
		 "MAXPERC_SSL" => $$ssl{'MAXPERC'},
		 "MAXPERC_MSA" => $$msa{'MAXPERC'},

		 "ERROR" => $errstr,
		 "NETCIDRINFO" => ($Net::CIDR::VERSION =~ /./
				   ? "\@NETCIDR\@":"\@NONETCIDR\@")
	     }
	     );
