eval '(exit $?0)' && eval 'exec perl -S $0 ${1+"$@"}'
    & eval 'exec perl -S $0 $argv:q'
    if 0;

# $Id$
# -*- perl -*-

use Env (ACE_ROOT);
use lib "$ACE_ROOT/bin";
use PerlACE::Run_Test;

$status = 0;


# single reader with single instances test
$num_messages=500;
$data_size=13;
$num_writers=1;
$num_readers=4;
$num_msgs_btwn_rec=20;
$pub_writer_id=0;

# need $num_msgs_btwn_rec unread samples plus 20 for good measure
# (possibly allocated by not yet queue by the transport because of greedy read).
$num_samples=$num_msgs_btwn_rec + 20;

$domains_file = PerlACE::LocalFile ("domain_ids");
$dcpsrepo_ior = PerlACE::LocalFile ("repo.ior");

unlink $dcpsrepo_ior;

$DCPSREPO = new PerlACE::Process ("$ENV{DDS_ROOT}/bin/DCPSInfoRepo",
                             "-NOBITS -o $dcpsrepo_ior"
                             . " -d $domains_file");


print $DCPSREPO->CommandLine(), "\n";

$svc_config=" -ORBSvcConf ../../tcp.conf ";
$sub_parameters = "-DCPSConfigFile conf.ini"
#              . " -DCPSDebugLevel 6"
   . "$svc_config"
              . "  -p $num_writers"
              . " -i $num_msgs_btwn_rec"
              . " -n $num_messages -d $data_size"
              . " -msi $num_samples -mxs $num_samples";
#use -msi $num_messages to avoid rejected samples
#use -mxs $num_messages to avoid using the heap
#   (could be less than $num_messages but I am not sure of the limit).

$Sub1 = new PerlACE::Process ("subscriber", $sub_parameters);
print $Sub1->CommandLine(), "\n";
$Sub2 = new PerlACE::Process ("subscriber", $sub_parameters);
print $Sub2->CommandLine(), "\n";
$Sub3 = new PerlACE::Process ("subscriber", $sub_parameters);
print $Sub3->CommandLine(), "\n";
$Sub4 = new PerlACE::Process ("subscriber", $sub_parameters);
print $Sub4->CommandLine(), "\n";


#NOTE: above 1000 queue samples does not give any better performance.
$pub_parameters = "-DCPSConfigFile conf.ini"
#              . " -DCPSDebugLevel 6"
   . "$svc_config"
              . " -p 1 -i $pub_writer_id"
              . " -r $num_readers"
              . " -n $num_messages -d $data_size"
              . " -msi 1000 -mxs 1000";

$Publisher = new PerlACE::Process ("publisher", $pub_parameters);
print $Publisher->CommandLine(), "\n";


$DCPSREPO->Spawn ();

if (PerlACE::waitforfile_timed ($dcpsrepo_ior, 30) == -1) {
    print STDERR "ERROR: waiting for DCPSInfo IOR file\n";
    $DCPSREPO->Kill ();
    exit 1;
}


$Sub1->Spawn ();
$Sub2->Spawn ();
$Sub3->Spawn ();
$Sub4->Spawn ();

$Publisher->Spawn ();

$PublisherResult = $Publisher->WaitKill (1200);
if ($PublisherResult != 0) {
    print STDERR "ERROR: publisher returned $PublisherResult \n";
    $status = 1;
}



$Sub1Result = $Sub1->WaitKill (1200);
if ($Sub1Result != 0) {
    print STDERR "ERROR: subscriber 1 returned $Sub1Result\n";
    $status = 1;
}


$Sub2Result = $Sub2->WaitKill (1200);
if ($Sub2Result != 0) {
    print STDERR "ERROR: subscriber 2 returned $Sub2Result \n";
    $status = 1;
}


$Sub3Result = $Sub3->WaitKill (1200);
if ($Sub3Result != 0) {
    print STDERR "ERROR: subscriber 3 returned $Sub3Result \n";
    $status = 1;
}


$Sub4Result = $Sub4->WaitKill (1200);
if ($Sub4Result != 0) {
    print STDERR "ERROR: subscriber 4 returned $Sub4Result \n";
    $status = 1;
}



$ir = $DCPSREPO->TerminateWaitKill(10);
if ($ir != 0) {
    print STDERR "ERROR: DCPSInfoRepo returned $ir\n";
    $status = 1;
}


exit $status;
