eval '(exit $?0)' && eval 'exec perl -S $0 ${1+"$@"}'
    & eval 'exec perl -S $0 $argv:q'
    if 0;

# $Id$
# -*- perl -*-

use Env (DDS_ROOT);
use lib "$DDS_ROOT/bin";
use Env (ACE_ROOT);
use lib "$ACE_ROOT/bin";
use PerlDDS::Run_Test;

my $status = 0;
my $debug = 1 ;
my $is_rtps_disc = 0;

my $dcpsrepo_ior = "dcps_ir.ior";
unlink $dcpsrepo_ior;

my $opts = "";

if ($ARGV[0] eq "rtps_disc") {
  $is_rtps_disc = 1;
  $opts = "-DCPSConfigFile rtps_disc.ini";
} else {
  $opts = "-k file://$dcpsrepo_ior";
}

unless ($is_rtps_disc) {
  $DCPSREPO = PerlDDS::create_process ("$ENV{DDS_ROOT}/bin/DCPSInfoRepo",
                                       "-NOBITS -o $dcpsrepo_ior");
}

$PUBLISHER = PerlDDS::create_process ("publisher", "$opts -q");
$SUBSCRIBER = PerlDDS::create_process ("subscriber", $opts);

unless ($is_rtps_disc) {
  print $DCPSREPO->CommandLine() . "\n" if $debug ;
  $DCPSREPO->Spawn ();
  if (PerlACE::waitforfile_timed ($dcpsrepo_ior, 30) == -1) {
    print STDERR "ERROR: cannot find file <$dcpsrepo_ior>\n";
    $DCPSREPO->Kill (); $DCPSREPO->TimedWait (1);
    exit 1;
  }
}

print $PUBLISHER->CommandLine() . "\n" if $debug ;
$PUBLISHER->Spawn ();
sleep 2;

print $SUBSCRIBER->CommandLine() . "\n" if $debug ;
$SUBSCRIBER->Spawn ();


$PubResult = $PUBLISHER->WaitKill (35);

if ($PubResult != 0) {
    print STDERR "ERROR: publisher returned $PubResult\n";
    $status = 1;
}

$SubResult = $SUBSCRIBER->WaitKill (35);

if ($SubResult != 0) {
    print STDERR "ERROR: subscriber returned $SubResult\n";
    $status = 1;
}

unless ($is_rtps_disc) {
  $ir = $DCPSREPO->TerminateWaitKill(30);

  if ($ir != 0) {
    print STDERR "ERROR: DCPSInfoRepo returned $ir\n";
    $status = 1;
  }
  unlink $dcpsrepo_ior;
}

exit $status;
