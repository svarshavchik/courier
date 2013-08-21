#!/usr/bin/perl

my $filename=shift @ARGV;

open(IN, $filename) || die "$filename: $!\n";
open(OUT, ">$filename.tmp") || die "$filename.tmp: $!\n";

my $l;

while (defined($l=<IN>))
{
    unless ($l =~ /::B::/)
    {
	$l =~ s/<pre/<hr \/><pre/;
	$l =~ s/<\/pre/<\/pre><hr \//;
	print OUT $l;
	next;
    }

    chomp $l;
    $l =~ s/::B::/<B>/;
    print OUT $l;
    $l=<IN>;
    chomp $l;
    $l=substr($l . (" " x 76), 0, 76);
    print OUT "$l</B>\n";
}
close(OUT);
rename "$filename.tmp", "$filename" || die "$filename: $!\n";
