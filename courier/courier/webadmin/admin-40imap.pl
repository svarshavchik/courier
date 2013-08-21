#! perl
#
# TITLE: IMAP
#
#
# Copyright 2001 Double Precision, Inc.  See COPYING for
# distribution information.

use webadmin;

my $errstr="";

my $saved=0;

my %cap_all;
my %cap_imap;
my %cap_imap_ssl;

{
    my $imap=ReadEnvVarConfigFile("imapd");
    my $imapssl=ReadEnvVarConfigFile("imapd-ssl");

    foreach (grep (/./, split (/\s+/, $$imap{'IMAP_CAPABILITY_ORIG'})))
    {
	$cap_all{$_}=1;
	$cap_imap{$_}=1;
    }

    foreach (grep (/./, split (/\s+/, $$imap{'IMAP_CAPABILITY_TLS_ORIG'})))
    {
	$cap_all{$_}=1;
	$cap_imap_ssl{$_}=1;
    }
}

if ($cgi->param("Save"))
{
    ReplaceEnvVarConfigFile("imapd", "IMAPDSTART",
                            $cgi->param("ENABLED") ? "YES":"NO");
    ReplaceEnvVarConfigFile("imapd-ssl", "IMAPDSSLSTART",
                            $cgi->param("ENABLED_SSL") ? "YES":"NO");

    ReplaceEnvVarConfigFile("imapd-ssl", "IMAPDSTARTTLS",
                            $cgi->param("ENABLED_STARTTLS") ? "YES":"NO");

    ReplaceEnvVarConfigFile("imapd-ssl", "IMAP_TLS_REQUIRED",
                            $cgi->param("REQUIRE_STARTTLS") ? "1":"0");

    ReplaceEnvVarConfigFile("imapd", "MAXDAEMONS",
                            $cgi->param("MAXDAEMONS"));
    ReplaceEnvVarConfigFile("imapd", "MAXPERIP",
                            $cgi->param("MAXPERIP"));
    $saved=1;
}

if ($cgi->param("SaveCAPA"))
{
    my @capa=("IMAP4rev1", "NAMESPACE");
    my @capaSSL=@capa;

    foreach (sort keys %cap_all)
    {
	my $cap=$_;
	my $capfldname=$cap;

	$capfldname =~ s/=/--/g;

	next if $cap eq "IMAP4rev1" || $cap eq "NAMESPACE";

	push @capa, $cap if $cap_imap{$cap} && $cgi->param($capfldname);

	push @capaSSL, $cap if $cap_imap_ssl{$cap} &&
	    $cgi->param("SSL-$capfldname");
    }

    ReplaceEnvVarConfigFile("imapd", "IMAP_CAPABILITY",
			    join(" ", @capa));
    ReplaceEnvVarConfigFile("imapd", "IMAP_CAPABILITY_TLS",
			    join(" ", @capaSSL));

    ReplaceEnvVarConfigFile("imapd", "IMAP_IDLE_TIMEOUT",
                            $cgi->param("IMAP_IDLE_TIMEOUT"));
    ReplaceEnvVarConfigFile("imapd", "IMAP_DISABLETHREADSORT",
                            $cgi->param("IMAP_DISABLETHREADSORT") ? "1":"0");
    ReplaceEnvVarConfigFile("imapd", "IMAP_CHECK_ALL_FOLDERS",
                            $cgi->param("IMAP_CHECK_ALL_FOLDERS") ? "1":"0");
    ReplaceEnvVarConfigFile("imapd", "IMAP_OBSOLETE_CLIENT",
                            $cgi->param("IMAP_OBSOLETE_CLIENT") ? "1":"0");

    $saved=1;
}

if ($cgi->param("SaveADV"))
{
    ReplaceEnvVarConfigFile("imapd", "IMAP_ULIMITD",
                            $cgi->param("IMAP_ULIMITD"));
    ReplaceEnvVarConfigFile("imapd", "IMAP_USELOCKS",
                            $cgi->param("IMAP_USELOCKS") ? "1":"0");
    ReplaceEnvVarConfigFile("imapd", "IMAP_MOVE_EXPUNGE_TO_TRASH",
                            $cgi->param("IMAP_MOVE_EXPUNGE_TO_TRASH")
			    ? "1":"0");

    my @trashFolders;

    foreach (split(/\s/, $cgi->param("trashfolderlist")))
    {
	my $folder=$_;
	my $days=$cgi->param("empty$folder")+0;

	$days =~ s/\s//g;
	next unless $days;
	push @trashFolders, "$folder:$days";
    }

    my $folder=$cgi->param("addEmptyFolder");
    my $days=$cgi->param("addEmptyDays")+0;

    $folder =~ s/^\s+//;
    $folder =~ s/\s+$//;
    $folder =~ s/,//;
    $folder =~ s/\\//;

    push @trashFolders, "$folder:$days" if $folder ne "" && $days;

    ReplaceEnvVarConfigFile("imapd", "IMAP_EMPTYTRASH",
			    join(",", @trashFolders));
    $saved=1;
}

if ($saved)
{
    changed("$sbindir/imapd stop ; . $sysconfdir/imapd ; test \"\$IMAPDSTART\" != YES || $sbindir/imapd start");

    changed("$sbindir/imapd-ssl stop ; . $sysconfdir/imapd-ssl ; test \"\$IMAPDSSLSTART\" != YES || $sbindir/imapd-ssl start");
    $errstr="\@SAVED\@";
}

my $imap=ReadEnvVarConfigFile("imapd");
my $imapssl=ReadEnvVarConfigFile("imapd-ssl");


my %cap_imap_set;
my %cap_imap_ssl_set;


foreach (grep (/./, split (/\s+/, $$imap{'IMAP_CAPABILITY'})))
{
    $cap_imap_set{$_}=1;
}

foreach (grep (/./, split (/\s+/, $$imap{'IMAP_CAPABILITY_TLS'})))
{
    $cap_imap_ssl_set{$_}=1;
}

my $capHTML="<table border=\"0\" cellpadding=\"8\"><tr><th>\@CAPABILITIES\@</th><th>\@NONTLS\@</th><th>\@TLS\@</th></tr>\n";

foreach (sort keys %cap_all)
{
    my $cap=$_;
    my $capfldname=$cap;

    $capfldname =~ s/=/--/g;

    next if $cap eq "IMAP4rev1" || $cap eq "NAMESPACE";

    $capHTML .= "<tr><td><tt>$cap</tt></td><td>";

    if ($cap_imap{$cap})
    {
	$capHTML .= "<input type=\"checkbox\" name=\"$capfldname\"" . ($cap_imap_set{$cap} ? " checked=\"checked\"":"") . " />";
    }
    else
    {
	$capHTML .= "&nbsp;";
    }

    $capHTML .= "</td><td>";

    if ($cap_imap_ssl{$cap})
    {
	$capHTML .= "<input type=\"checkbox\" name=\"SSL-$capfldname\"" . ($cap_imap_ssl_set{$cap} ? " checked=\"checked\"":"") . " />";
    }
    else
    {
	$capHTML .= "&nbsp;";
    }
    $capHTML .= "</tr>\n";
}

$capHTML .= "</table>\n";

my $trashHTML="<table border=\"0\" cellspacing=\"4\"><tr><th>\@FOLDER\@</th><th>\@DAYS\@</th></tr>\n";

my @trashfolders;

foreach (split(/,/, $$imap{'IMAP_EMPTYTRASH'}))
{
    my $days=$_;
    my $folder="Trash";

    next unless $days;

    ($folder, $days)=($1,$2) if $days =~ /^(.*):([0-9]+)$/;

    push @trashfolders, $folder;

    $trashHTML .= "<tr><td><tt>$folder</tt></td><td><tt><input type=\"text\" name=\"empty$folder\" value=\"$days\" size=\"3\" /></tt></td></tr>\n";
}

$trashHTML .= "<tr><td><tt><input type=\"text\" name=\"addEmptyFolder\" size=\"9\" /></tt><input type=\"hidden\" name=\"trashfolderlist\" value=\""
    . join(" ", @trashfolders) . "\" /></td><td><tt><input type=\"text\" name=\"addEmptyDays\" size=\"3\" /></tt></td></tr></table>\n";

display_form("admin-40imap.html",
	     {
		 "ENABLED" => "<input type='checkbox' name='ENABLED'" . (($$imap{"IMAPDSTART"} =~ /[yY]/) ? " checked=\"checked\"":"") . " />",
		 "ENABLED_SSL" => "<input type='checkbox' name='ENABLED_SSL'" . ($$imapssl{"IMAPDSSLSTART"} =~ /[yY]/ ? " checked=\"checked\"":"") . " />",

		 "ENABLED_STARTTLS" => "<input type='checkbox' name='ENABLED_STARTTLS'" . ($$imapssl{"IMAPDSTARTTLS"} =~ /[yY]/ ? " checked=\"checked\"":"") . " />",

		 "REQUIRE_STARTTLS" => "<input type='checkbox' name='REQUIRE_STARTTLS'" . ($$imapssl{"IMAP_TLS_REQUIRED"} ? " checked=\"checked\"":"") . " />",

		 "IMAP_IDLE_TIMEOUT" => $$imap{"IMAP_IDLE_TIMEOUT"},

		 "IMAP_DISABLETHREADSORT" => "<input type='checkbox' name='IMAP_DISABLETHREADSORT'" . ($$imap{"IMAP_DISABLETHREADSORT"} ? " checked=\"checked\"":"") . " />",
		 "IMAP_CHECK_ALL_FOLDERS" => "<input type='checkbox' name='IMAP_CHECK_ALL_FOLDERS'" . ($$imap{"IMAP_CHECK_ALL_FOLDERS"} ? " checked=\"checked\"":"") . " />",
		 "IMAP_OBSOLETE_CLIENT" => "<input type='checkbox' name='IMAP_OBSOLETE_CLIENT'" . ($$imap{"IMAP_OBSOLETE_CLIENT"} ? " checked=\"checked\"":"") . " />",

		 "MAXDAEMONS" => $$imap{'MAXDAEMONS'},
		 "MAXPERIP" => $$imap{'MAXPERIP'},

		 "CAPA" => $capHTML,

		 "IMAP_ULIMITD" => $$imap{'IMAP_ULIMITD'},
		 "IMAP_USELOCKS" => "<input type='checkbox' name='IMAP_USELOCKS'" . ($$imap{"IMAP_USELOCKS"} ? " checked=\"checked\"":"") . " />",
		 "IMAP_MOVE_EXPUNGE_TO_TRASH" => "<input type='checkbox' name='IMAP_MOVE_EXPUNGE_TO_TRASH'" . ($$imap{"IMAP_MOVE_EXPUNGE_TO_TRASH"} ? " checked=\"checked\"":"") . " />",

		 "IMAP_EMPTYTRASH" => $trashHTML,

		 "ERROR" => $errstr
	     }
	     );
