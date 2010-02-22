eval '(exit $?0)' && eval 'exec perl -nS $0 ${1+"$@"}'
    & eval 'exec perl -nS $0 $argv:q'
    if 0;

use warnings;
use strict;
use File::Basename;

=pod

$Id$

genstats.pl - create GNUPlot statistical summary data string variables

SYNOPSIS
  genstats.pl <infile>

This script processes a comma separated value (CSV) input file and
creates an output file for each record of the input.  Each output file is
placed in the same directory that the input file was located in.  The
output file names are constructed using fields from the input record as:

  <transport>-<size>.stats

The input file is expected to be in the format produced by the extract.pl
data reduction script.  Each record (line) of the input file contains the
following fields:

  Field  1: transport type
  Field  2: test message size
  Field  3: latency mean statistic
  Field  4: latency standard deviation statistic
  Field  5: latency maximum statistic
  Field  6: latency minimum statistic
  Field  7: jitter mean statistic
  Field  8: jitter standard deviation statistic
  Field  9: jitter maximum statistic
  Field 10: jitter minimum statistic

The output includes two GNUPlot string variable definitions suitable for
'load' or 'call' operations within GNUPlot.  Some GNUPlot data
visualization scripts use these variables to generate label information
to place on some plots.  The variables are:

  latency_stats
  jitter_stats

Each variable contains the mean, standard deviation, maximum, and minimum
data values for the output file (transport/size) in a newline separated
single string suitable for use as a label within GNUPlot.

EXAMPLE

  genstats.pl data/latency.csv

=cut

# Skip comments.
next if /#/;

# Parse the CSV input file, ignoring blank lines and removing the line end.
my @fields = split ',';
next if $#fields eq 0;
chomp $fields[9];

# Establish the output file using the transport and size information.
my $filename = dirname($ARGV) . "/" . $fields[0] . "-" . $fields[1] . ".stats";
die "Unable to open output: $filename - $!"
  if not open( OUT, ">$filename");

# Create the 'latency_stats' GNUPlot string variable.
print OUT "latency_stats=\"";
print OUT "Mean: $fields[2],\\n";
print OUT "Std. Dev.: $fields[3],\\n";
print OUT "Maximum: $fields[4],\\n";
print OUT "Minimum: $fields[5]\\n\"\n";

# Create the 'jitter_stats' GNUPlot string variable.
print OUT "jitter_stats=\"";
print OUT "Mean: $fields[6],\\n";
print OUT "Std. Dev.: $fields[7],\\n";
print OUT "Maximum: $fields[8],\\n";
print OUT "Minimum: $fields[9]\\n\"\n";

# Current file is done.
close(OUT);

