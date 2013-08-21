#! perl
#
# TITLE: Password authentication modules
#
#
# Copyright 2001 Double Precision, Inc.  See COPYING for
# distribution information.

my $vars=ReadEnvVarConfigFile("authdaemonrc");

my $authmodulelist=$$vars{'authmodulelist'};

my @mods=grep(/./, split (/\s+/, $authmodulelist));

my $authmodule_param=$cgi->param("authmodulelist");

my $selected= -1;

my $errstr="";

if ($cgi->param("Up") && $authmodule_param)
{
    my $i;

    for ($i=0; $i <= $#mods; $i++)
    {
	next unless $mods[$i] eq $authmodule_param;

	if ($i > 0)
	{
	    $mods[$i]=$mods[$i-1];
	    $mods[$i-1]=$authmodule_param;

	    ReplaceEnvVarConfigFile("authdaemonrc", "authmodulelist",
				    join(" ", @mods));
	    changed("$authdaemond restart");
	    $selected= $i-1;
	    last;
	}
    }
}

if ($cgi->param("Down") && $authmodule_param)
{
    my $i;

    for ($i=0; $i <= $#mods; $i++)
    {
	next unless $mods[$i] eq $authmodule_param;

	if ($i < $#mods)
	{
	    $mods[$i]=$mods[$i+1];
	    $mods[$i+1]=$authmodule_param;

	    ReplaceEnvVarConfigFile("authdaemonrc", "authmodulelist",
				    join(" ", @mods));
	    changed("$authdaemond restart");
	    $selected=$i+1;
	    last;
	}
    }
}

if ($cgi->param("Delete") && $authmodule_param)
{
    my $i;

    for ($i=0; $i <= $#mods; $i++)
    {
	next unless $mods[$i] eq $authmodule_param;

	splice @mods, $i, 1;
	ReplaceEnvVarConfigFile("authdaemonrc", "authmodulelist",
				join(" ", @mods));
	changed("$authdaemond restart");
	last;
    }
}

if ($authmodule_param=$cgi->param("authmodulelistorig"))
{
    push @mods, $authmodule_param;
    ReplaceEnvVarConfigFile("authdaemonrc", "authmodulelist",
			    join(" ", @mods));
    changed("$authdaemond restart");
    $errstr="\@SAVED\@";
}

my $authmodulelist_current="<tt><select name=authmodulelist size=6>";

foreach (@mods)
{
    my $n=$_;

    $authmodulelist_current .=
	"<option value=\"$n\" " . (($selected--) ? "":"selected=\"selected\"") . ">$n\n";
}
$authmodulelist_current .= "</select></tt>";

my $authmodulelistorig=$$vars{'authmodulelistorig'};

my $authmodulelistorig_current="<tt><select name=authmodulelistorig><option>";

foreach (grep(/./, split (/\s+/, $authmodulelistorig)))
{
    $authmodulelistorig_current .= "<option>$_\n";
}
$authmodulelistorig_current .= "</select></tt>";

my $daemons=$$vars{'daemons'};

my $daemons_params=$cgi->param("daemons");

if ($daemons_params =~ /([1-9][0-9]*)/)
{
    $daemons_params=$1;

    ReplaceEnvVarConfigFile("authdaemonrc", "daemons", $daemons_params);
    $daemons=$daemons_params;
}

display_form("admin-10password.html",
	     {
		 "AUTHMODULELIST" => $authmodulelist_current,
		 "AUTHMODULELISTORIG" => $authmodulelistorig_current,
		 "DAEMONS" => $daemons
	     }
	     );
