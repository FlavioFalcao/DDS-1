eval '(exit $?0)' && eval 'exec perl -S $0 ${1+"$@"}'
    & eval 'exec perl -S $0 $argv:q'
    if 0;

# $Id$
# -*- perl -*-

use Env (ACE_ROOT);
use lib "$ACE_ROOT/bin";
use PerlACE::Run_Test;

$status = 0;

$domains_file = PerlACE::LocalFile ("domain_ids");
$dcpsrepo_ior = PerlACE::LocalFile ("repo.ior");

$svc_config = new PerlACE::ConfigList->check_config ('STATIC') ? ''
    : "-ORBSvcConf ../../tcp.conf";

unlink $dcpsrepo_ior;

# -b
$parameters = "-DcpsBit 0 $svc_config ";
# or could have
# $parameters = "-b -DcpsBit 1 $svc_config ";

if ($ARGV[0] eq 'by_instance') {
  $parameters .= " -i";
}

if (PerlACE::is_vxworks_test()) {
  # -ORBDebugLevel 1 -NOBITS
  $DCPSREPO = new PerlACE::ProcessVX ("$ENV{DDS_ROOT}/bin/DCPSInfoRepo",
                                      "$svc_config -o $dcpsrepo_ior"
                                      . " -d $domains_file -NOBITS");

  $ZCTest = new PerlACE::ProcessVX ("main", $parameters);

}
else {
  # -ORBDebugLevel 1 -NOBITS
  $DCPSREPO = new PerlACE::Process ("$ENV{DDS_ROOT}/bin/DCPSInfoRepo",
                                    "$svc_config -o $dcpsrepo_ior"
                                    . " -d $domains_file -NOBITS");

  $ZCTest = new PerlACE::Process ("main", $parameters);
}

print $DCPSREPO->CommandLine(), "\n";
if ($DCPSREPO->Spawn () != 0) {
    print STDERR "ERROR: Couldn't spawn InfoRepo\ntest FAILED.\n";
    return 1;
}

if (PerlACE::waitforfile_timed ($dcpsrepo_ior, 30) == -1) {
    print STDERR "ERROR: waiting for DCPSInfo IOR file\n";
    $DCPSREPO->Kill ();
    exit 1;
}

print $ZCTest->CommandLine(), "\n";
if ($ZCTest->Spawn () != 0) {
    print STDERR "ERROR: Couldn't spawn main\ntest FAILED.\n";
    return 1;
}

$result = $ZCTest->WaitKill (60);

if ($result != 0) {
    print STDERR "ERROR: main returned $result \n";
    $status = 1;
}

$ir = $DCPSREPO->TerminateWaitKill(5);

if ($ir != 0) {
    print STDERR "ERROR: DCPSInfoRepo returned $ir\n";
    $status = 1;
}

if ($status == 0) {
  print "test PASSED.\n";
}
else {
  print STDERR "test FAILED.\n";
}
exit $status;
