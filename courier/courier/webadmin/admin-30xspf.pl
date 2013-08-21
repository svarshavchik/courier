#! perl
#
# TITLE: >>>Sender Policy Framework
#
#
# Copyright 2004 Double Precision, Inc.  See COPYING for
# distribution information.

use webadmin;

my $error="";

if ($cgi->param("Save"))
{
    my @l =
	grep( $_ !~ /^opt\s+(BOFHSPFHELO|BOFHSPFMAILFROM|BOFHSPFFROM|BOFHSPFNOVERBOSE|BOFHSPFTRUSTME|BOFHSPFHARDERROR)/,
	      ReadMultiLineConfigFile("bofh"));

    foreach ("helo","mailfrom","from")
    {
	my $lc=$_;
	my $uc=uc($_);

	my $radio=$cgi->param($uc);

	next if $radio eq "disabled";

	my $mailfromok="";

	$mailfromok=",mailfromok"
	    if $lc eq "from" && $cgi->param("MAILFROMOK");

	if ($radio eq "enabled")
	{
	    push @l, "opt BOFHSPF$uc=pass,none,neutral,softfail,unknown$mailfromok";
	}
	else
	{
	    my $flags="pass$mailfromok";

	    foreach ("fail", "none","neutral","softfail","error","unknown")
	    {
		$flags .= ",$_"
		    if $cgi->param($uc . uc($_));
	    }
	    push @l, "opt BOFHSPF$uc=$flags";
	}
    }

    my @hard;

    foreach ("fail", "none","neutral","softfail","error","unknown")
    {
	push @hard, $_ if $cgi->param(uc("hard$_"));
    }
    my $hard=join(",", @hard);

    push @l, "opt BOFHSPFHARDERROR=$hard"
	unless $hard eq "fail,softfail";

    push @l, "opt BOFHSPFNOVERBOSE=1"
	if $cgi->param("BOFHSPFNOVERBOSE");

    push @l, "opt BOFHSPFTRUSTME=1"
	if $cgi->param("BOFHSPFTRUSTME");

    SaveMultiLineConfigFile("bofh", \@l);
    changed("");
    $errmsg="\@SAVED\@";
}

my %bofhspfhelo;
my %bofhspfmailfrom;
my %bofhspffrom;
my %bofhspfharderror;

$bofhspfhelo{"off"}=1;
$bofhspfmailfrom{"off"}=1;
$bofhspffrom{"off"}=1;

my $bofhspfnoverbose=0;
my $bofhspftrustme=0;

my $hasbofhspfharderror=0;

grep {
    $bofhspfnoverbose=$1 if /^opt\s+BOFHSPFNOVERBOSE=(.*)/;
    $bofhspftrustme=$1 if /^opt\s+BOFHSPFTRUSTME=(.*)/;
    grep {delete $bofhspfhelo{"off"};
	  $bofhspfhelo{$_}=1; } split(/,/,$1) if /^opt\s+BOFHSPFHELO=(.*)/;
    grep {delete $bofhspfmailfrom{"off"};
	  $bofhspfmailfrom{$_}=1; } split(/,/,$1) if /^opt\s+BOFHSPFMAILFROM=(.*)/;
    grep {delete $bofhspffrom{"off"};
	  $bofhspffrom{$_}=1; } split(/,/,$1) if /^opt\s+BOFHSPFFROM=(.*)/;
    grep {$hasbofhspfharderror=1;
	  $bofhspfharderror{$_}=1; } split(/,/,$1) if /^opt\s+BOFHSPFHARDERROR=(.*)/;
} ReadMultiLineConfigFile("bofh");

%bofhspfharderror=( "fail" => 1, "softfail" => 1)
    unless  $hasbofhspfharderror;

sub is_hash_disabled {
    my $hash=shift @_;

    return $$hash{"off"} ? 1:0;
}

sub is_hash_enabled {
    my $hash=shift @_;

    return 0
	if is_hash_disabled($hash) || $$hash{"fail"} || $$hash{"error"} || $$hash{"all"};

    return 0 unless
	$$hash{"none"} && $$hash{"pass"} &&
	$$hash{"softfail"} && $$hash{"neutral"} && $$hash{"unknown"};

    return 1;
}

sub is_hash_custom {
    my $hash=shift @_;

    return 0 if is_hash_disabled($hash) || is_hash_enabled($hash);
    return 1;
}

sub is_disabled {
    my $var=shift @_;
    my $hash=shift @_;

    return '<input type="radio" name="' . $var
	. '" value="disabled" '
	. (is_hash_disabled($hash) ? 'checked="checked"':'') . '>';
}

sub is_enabled {
    my $var=shift @_;
    my $hash=shift @_;

    return '<input type="radio" name="' . $var
	. '" value="enabled" '
	. (is_hash_enabled($hash) ? 'checked="checked"':'') . '>';
}

sub is_custom {
    my $var=shift @_;
    my $hash=shift @_;

    return '<input type="radio" name="' . $var
	. '" value="custom" '
	. (is_hash_custom($hash) ? 'checked="checked"':'') . '>';
}

sub is_custom_checkbox {
    my $field=shift @_;
    my $hash=shift @_;
    my $value=shift @_;

    return '&nbsp;&nbsp;&nbsp;<input type="checkbox" name="' . $field
	. '" '
	. (is_hash_custom($hash) &&
	   $$hash{$value} ? 'checked="checked"':'') . '>';
}

sub is_harderror {
    my $name=shift @_;
    my $hash=shift @_;

    return '<input type="checkbox" name="HARD' . uc($name)
	. '" '
	. ($$hash{$name} ? 'checked="checked"':'') . '>';
}

display_form("admin-30xspf.html",
	     {
		 "SPFEHDISABLED" => is_disabled("HELO", \%bofhspfhelo),
		 "SPFMFDISABLED" => is_disabled("MAILFROM", \%bofhspfmailfrom),
		 "SPFFRDISABLED" => is_disabled("FROM", \%bofhspffrom),

		 "SPFEHENABLED" => is_enabled("HELO", \%bofhspfhelo),
		 "SPFMFENABLED" => is_enabled("MAILFROM", \%bofhspfmailfrom),
		 "SPFFRENABLED" => is_enabled("FROM", \%bofhspffrom),

		 "SPFEHCUSTOM" => is_custom("HELO", \%bofhspfhelo),
		 "SPFMFCUSTOM" => is_custom("MAILFROM", \%bofhspfmailfrom),
		 "SPFFRCUSTOM" => is_custom("FROM", \%bofhspffrom),


		 "SPFEHNONE" => is_custom_checkbox("HELONONE", \%bofhspfhelo, "none"),
		 "SPFMFNONE" => is_custom_checkbox("MAILFROMNONE", \%bofhspfmailfrom, "none"),
		 "SPFFRNONE" => is_custom_checkbox("FROMNONE", \%bofhspffrom, "none"),

		 "SPFEHNEUTRAL" => is_custom_checkbox("HELONEUTRAL", \%bofhspfhelo, "neutral"),
		 "SPFMFNEUTRAL" => is_custom_checkbox("MAILFROMNEUTRAL", \%bofhspfmailfrom, "neutral"),
		 "SPFFRNEUTRAL" => is_custom_checkbox("FROMNEUTRAL", \%bofhspffrom, "neutral"),

		 "SPFEHSOFTFAIL" => is_custom_checkbox("HELOSOFTFAIL", \%bofhspfhelo, "softfail"),
		 "SPFMFSOFTFAIL" => is_custom_checkbox("MAILFROMSOFTFAIL", \%bofhspfmailfrom, "softfail"),
		 "SPFFRSOFTFAIL" => is_custom_checkbox("FROMSOFTFAIL", \%bofhspffrom, "softfail"),

		 "SPFEHERROR" => is_custom_checkbox("HELOERROR", \%bofhspfhelo, "error"),
		 "SPFMFERROR" => is_custom_checkbox("MAILFROMERROR", \%bofhspfmailfrom, "error"),
		 "SPFFRERROR" => is_custom_checkbox("FROMERROR", \%bofhspffrom, "error"),

		 "SPFEHUNKNOWN" => is_custom_checkbox("HELOUNKNOWN", \%bofhspfhelo, "unknown"),
		 "SPFMFUNKNOWN" => is_custom_checkbox("MAILFROMUNKNOWN", \%bofhspfmailfrom, "unknown"),
		 "SPFFRUNKNOWN" => is_custom_checkbox("FROMUNKNOWN", \%bofhspffrom, "unknown"),

		 "SPFEHFAIL" => is_custom_checkbox("HELOFAIL", \%bofhspfhelo, "fail"),
		 "SPFMFFAIL" => is_custom_checkbox("MAILFROMFAIL", \%bofhspfmailfrom, "fail"),
		 "SPFFRFAIL" => is_custom_checkbox("FROMFAIL", \%bofhspffrom, "fail"),

		 "BOFHSPFNOVERBOSE" => '<input type="checkbox" name="BOFHSPFNOVERBOSE" '
		     . ($bofhspfnoverbose ? ' checked="checked"':'') . '>',
		 "BOFHSPFTRUSTME" => '<input type="checkbox" name="BOFHSPFTRUSTME" '
		     . ($bofhspftrustme ? ' checked="checked"':'') . '>',
		 "MAILFROMOK" => '<input type="checkbox" name="MAILFROMOK" '
		     . ($bofhspffrom{"mailfromok"} ? ' checked="checked"':'') . '>',

		 "HARDNONE" => is_harderror("none", \%bofhspfharderror),
		 "HARDNEUTRAL" => is_harderror("neutral", \%bofhspfharderror),
		 "HARDSOFTFAIL" => is_harderror("softfail", \%bofhspfharderror),
		 "HARDFAIL" => is_harderror("fail", \%bofhspfharderror),
		 "HARDERROR" => is_harderror("error", \%bofhspfharderror),
		 "HARDUNKNOWN" => is_harderror("unknown", \%bofhspfharderror),

		 "ERROR" => $errmsg
	     }
	     );
