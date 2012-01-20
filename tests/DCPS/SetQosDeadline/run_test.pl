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

$status = 0;
my $is_rtps_disc = 0;

if ($ARGV[0] eq 'rtps_disc') {
  $is_rtps_disc = 1;
}

$pub_opts = "-DCPSConfigFile " . ($is_rtps_disc ? "rtps_disc.ini" : "pub.ini");
$sub_opts = "-DCPSConfigFile " . ($is_rtps_disc ? "rtps_disc.ini" : "sub.ini");

$dcpsrepo_ior = "repo.ior";

unlink $dcpsrepo_ior;

if (!$is_rtps_disc) {
  $DCPSREPO = PerlDDS::create_process ("$ENV{DDS_ROOT}/bin/DCPSInfoRepo",
                                       "-o $dcpsrepo_ior ");
}
$Subscriber = PerlDDS::create_process ("subscriber", "$sub_opts");
$Publisher = PerlDDS::create_process ("publisher", "$pub_opts");


if (!$is_rtps_disc) {
  print $DCPSREPO->CommandLine() . "\n";
  $DCPSREPO->Spawn ();
  if (PerlACE::waitforfile_timed ($dcpsrepo_ior, 30) == -1) {
    print STDERR "ERROR: waiting for DCPSInfo IOR file\n";
    $DCPSREPO->Kill ();
    exit 1;
  }
}

print $Publisher->CommandLine() . "\n";
print $Subscriber->CommandLine() . "\n";
$Publisher->Spawn ();

$Subscriber->Spawn ();

if (!$is_rtps_disc) {
  $PublisherResult = $Publisher->WaitKill (300);
  if ($PublisherResult != 0) {
    print STDERR "ERROR: publisher returned $PublisherResult \n";
    $status = 1;
  }
}

$SubscriberResult = $Subscriber->WaitKill (15);
if ($SubscriberResult != 0) {
    print STDERR "ERROR: subscriber returned $SubscriberResult \n";
    $status = 1;
}

$ir = $DCPSREPO->TerminateWaitKill(5);
if ($ir != 0) {
    print STDERR "ERROR: DCPSInfoRepo returned $ir\n";
    $status = 1;
}

unlink $dcpsrepo_ior;

if ($status == 0) {
  print "test PASSED.\n";
} else {
  print STDERR "test FAILED.\n";
}

exit $status;
