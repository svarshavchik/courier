#!/usr/bin/perl

open (c, "ChangeLog") || die "$!\n";

my $seen_changelog=0;

my $html=0;

while (<c>)
{
	my $l=$_;

	if ($html)
	{
	    $l =~ s/\&/\&amp;/g;
	    $l =~ s/</\&lt;/g;
	    $l =~ s/>/\&gt;/g;
	}

	if ($l =~ /^200[45]/)
	{
	    if ($html)
	    {
		print "</ul>\n" if $inul;
		print "<h3>$l</h3>\n";
	    }
	    else
	    {
		print "\n$l";
	    }

	    $inul=0;
	    $seen_changelog=1;
	    next;
	}

	last if $seen_changelog && $l =~ /^[0-9\.]+$/;

	next unless $l =~ /^[\t ]+/;

	if (!$html)
	{
	    print "\n" if $l =~ /^\s+\*/;
	    print $l;
	}
	elsif ($l =~ s/^[\t ]+\*[\t ]*//)
	{
	    print (($inul ? "<br><br>":"<ul>\n") . "<li>$l");
	    $inul=1;
	}
	else
	{
	    print $l;
	}
}
