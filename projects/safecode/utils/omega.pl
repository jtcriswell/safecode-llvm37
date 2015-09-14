#!/usr/bin/perl
$output = 0;
$line = <STDIN>;
while (!eof) {
    if ($line =~ /FALSE/) {
	$output = 1;
    }
    $line = <STDIN>;
}
print $output;
