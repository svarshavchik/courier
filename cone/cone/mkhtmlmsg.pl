#!/usr/bin/perl

my $html;
my $title="";

my $l;

while (defined($l=<STDIN>))
{
    $title=$1 if $l =~ /<title\s*>(.*)<\/title/ && $1 !~ /^\s*$/;
    last if $l =~ /<div class=\"(chapter|part)\"/;
}
$l =~ s/^>//;
$html .= $l;

while (defined($l=<STDIN>))
{
  if ($l =~ />(.*)<\/h[123456789]/ && $1 !~ /^\s*$/ && !$title)
    {
      $title=$1;
    }

  if ($l =~ /<div class=\"toc\"/)
    {
      do
	{
	  $l=<STDIN>;
	} while (defined $l && $l !~ /\/div/);
    }

  last if $l =~ /<div class=\"(navfooter)\"/;
  $html .= $l;
}
$html .= ">\n";

$title =~ s/\s+/ /g;

print "From nobody\@localhost " . (localtime) . "\n";
print "From: Online Help <nobody\@localhost>\n";
print "Subject: $title\n";
print "Mime-Version: 1.0\n";
print "Content-Type: text/html; charset=utf-8\n";
print "\n";
print "<body><html>$html</body></html>\n\n\n";
