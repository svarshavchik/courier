#! perl
#
# TITLE: Outbound FAX
#
#
# Copyright 2002 Double Precision, Inc.  See COPYING for
# distribution information.

use webadmin;

my $errstr="";
local $changed=0;

if ($cgi->param("Save"))
{
  my $rc="";

  if ($cgi->param("DIALPLAN") eq "NANPA")
    {
      my $areacode=$cgi->param("nanpa_areacode");
      my @areacode10=grep(/./, split (/\s+/, $cgi->param("nanpa_10digit")));
      my $local_pfix=$cgi->param("nanpa_local_prefix");
      my $long_pfix=$cgi->param("nanpa_long_prefix");
      my $intl_pfix=$cgi->param("nanpa_intl_prefix");

      $areacode =~ s/\s//g;
      $local_pfix =~ s/\s//g;
      $long_pfix =~ s/\s//g;
      $intl_pfix =~ s/\s//g;

      $errstr =~ "\@BADAREACODE\@" unless $areacode =~ /^\d\d\d$/;

      $errstr =~ "\@BADPREFIX\@" if $local_pfix =~ /^1/;

      grep {
	$errstr =~ "\@BADAREACODE\@" unless $_ =~ /^\d\d\d$/;}
	@areacode10;

      $rc .= "webadmin-dialplan      nanpa\n\n" .
	"webadmin-nanpa-areacode     $areacode\n" .
	"webadmin-nanpa-10digit      " . join("-", @areacode10) . "\n" .
	"webadmin-nanpa-local-prefix $local_pfix\n" .
	"webadmin-nanpa-long-prefix  $long_pfix\n" .
	"webadmin-nanpa-intl-prefix  $intl_pfix\n" .
	"webadmin-nanpa-local-011    " . ($cgi->param("nanpa_local_011")
					  ? "1\n":"\n") .
	"webadmin-nanpa-esmtp-011    " . ($cgi->param("nanpa_esmtp_011")
					  ? "1\n\n":"\n\n");

      $rc .= "rw^*    1               \$9\n";
      $rc .= "rw^     011             1011\$9\n";
      $rc .= "rw^     n11             1\n";
      $rc .= "rw^     1011            011\$9\n";
      $rc .= "rw      (nnnnnnn)       $areacode\$1\n";

      foreach ("2", "3", "4", "5", "6", "7", "8", "9")
	{
	  $rc .= "accept   $_" . "nnnnnnnnn   local\n";
	  $rc .= "accept   $_" . "nnnnnnnnn   esmtp\n";
	}

      $rc .= "accept^ 011    local\n" if $cgi->param("nanpa_local_011");
      $rc .= "accept^ 011    esmtp\n" if $cgi->param("nanpa_esmtp_011");

      $rc .= "rw^    (.)       1\$1\$9\n";
      $rc .= "rw^    1011      011\$9\n";

      if ($#areacode10 < 0)
	{
	  $rc .= "rw^    1$areacode      $local_pfix\$9\n";
	}
      else
	{
	  $rc .= "rw^    1$areacode      $local_pfix$areacode\$9\n";
	}

      foreach (@areacode10)
	{
	  $rc .= "rw^    1$_      $local_pfix$_\$9\n";
	}

      $rc .= "rw^    1         $long_pfix\$9\n";
      $rc .= "rw^    011       $intl_pfix" . "011\$9\n";
    }
  else
    {
      $rc .= "webadmin-dialplan\tother\n\n" .

	"accept^  .    local\n" .
	"accept^  .    esmtp\n";
    }

  unless ($errstr =~ /./)
    {
      my %section;

      $section{'faxrc'}=$rc;

      SaveKWConfigFile("faxrc", \%section);
      changed("courier restart");
      $changed=1;
    }
}

$errstr="\@SAVED\@" if $changed;

my $faxrc=ReadKWConfigFile("faxrc");

$$faxrc{"webadmin-nanpa-local-prefix"} =~ s/\@/\&#64;/g;
$$faxrc{"webadmin-nanpa-long-prefix"} =~ s/\@/\&#64;/g;
$$faxrc{"webadmin-nanpa-intl-prefix"} =~ s/\@/\&#64;/g;
$$faxrc{"webadmin-nanpa-10digit"} =~ s/-/ /g;

display_form("admin-35fax.html",
	     {

	      "DIALNANPA" => ('<input type="radio" name="DIALPLAN" value="NANPA"'
			      . ($$faxrc{"webadmin-dialplan"} eq "nanpa" ?
				 " checked=\"checked\" />":" />")),
	      "DIALOTHER" => ('<input type="radio" name="DIALPLAN" value="OTHER"'
			      . ($$faxrc{"webadmin-dialplan"} eq "other" ?
				 " checked=\"checked\" />":" />")),
	      "NANPA_AREACODE" => $$faxrc{"webadmin-nanpa-areacode"},
	      "NANPA_10DIGIT" => $$faxrc{"webadmin-nanpa-10digit"},
	      "NANPA_LOCAL_PREFIX" => $$faxrc{"webadmin-nanpa-local-prefix"},
	      "NANPA_LONG_PREFIX" => (
				      $$faxrc{"webadmin-nanpa-long-prefix"} ?
				      $$faxrc{"webadmin-nanpa-long-prefix"}:"1"
				     ),
	      "NANPA_INTL_PREFIX" => $$faxrc{"webadmin-nanpa-intl-prefix"},

	      "NANPA_LOCAL_011" =>
	      ('<input type="checkbox" name="nanpa_local_011"'
	       . ( $$faxrc{"webadmin-nanpa-local-011"} ? " checked=\"checked\" />":" />")),
	      "NANPA_ESMTP_011" =>
	      ('<input type="checkbox" name="nanpa_esmtp_011"'
	       . ( $$faxrc{"webadmin-nanpa-esmtp-011"} ? " checked=\"checked\" />":" />")),

	      "ERROR" => $errstr
	     }
	    );
