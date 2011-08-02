/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "ace/OS_main.h"
#include <dds/DCPS/Service_Participant.h>
#include "ace/Configuration_Import_Export.h"
#include "tao/corba.h"

#include "dds/DCPS/transport/framework/TransportRegistry.h"
#include "dds/DCPS/transport/tcp/TcpInst.h"
#include "dds/DCPS/transport/tcp/TcpInst_rch.h"
#include "dds/DCPS/debug.h"
#include "dds/DCPS/transport/framework/TransportDebug.h"

#ifdef ACE_AS_STATIC_LIBS
#include "dds/DCPS/transport/tcp/Tcp.h"
#endif

#include "../common/TestSupport.h"

#include <iostream>

using namespace OpenDDS::DCPS;

int
ACE_TMAIN(int argc, ACE_TCHAR* argv[])
{
  DDS::DomainParticipantFactory_var dpf =
    TheParticipantFactoryWithArgs(argc, argv);
  TEST_CHECK(dpf.in() != 0);

  TEST_CHECK(OpenDDS::DCPS::DCPS_debug_level==1);
  TEST_CHECK(TheServiceParticipant->n_chunks()==10);
  TEST_CHECK(TheServiceParticipant->association_chunk_multiplier()==5);
  TEST_CHECK(TheServiceParticipant->liveliness_factor()==70);
  TEST_CHECK(TheServiceParticipant->bit_transport_port()==1234);
  TEST_CHECK(TheServiceParticipant->bit_lookup_duration_msec()==1000);
  TEST_CHECK(TheServiceParticipant->get_BIT()==false);
  TEST_CHECK(OpenDDS::DCPS::Transport_debug_level==1);
  TEST_CHECK(TheServiceParticipant->pending_timeout()==ACE_Time_Value(10));
  TEST_CHECK(TheServiceParticipant->publisher_content_filter()==false);
  TEST_CHECK(TheServiceParticipant->federation_recovery_duration()==800);
  TEST_CHECK(TheServiceParticipant->federation_initial_backoff_seconds()==2);
  TEST_CHECK(TheServiceParticipant->federation_backoff_multiplier()==3);
  TEST_CHECK(TheServiceParticipant->federation_liveliness()==70);

  TransportInst_rch inst = TransportRegistry::instance()->get_inst("mytcp");
  TEST_CHECK(inst != 0);

  TcpInst_rch tcp_inst = dynamic_rchandle_cast<TcpInst>(inst);
  TEST_CHECK(tcp_inst != 0);

  // tcp_inst->dump(std::cout);

  TEST_CHECK(tcp_inst->name() == "mytcp");
  TEST_CHECK(tcp_inst->queue_messages_per_pool_ == 9);
  TEST_CHECK(tcp_inst->queue_initial_pools_ == 2);
  TEST_CHECK(tcp_inst->max_packet_size_ == 2000000000);
  TEST_CHECK(tcp_inst->max_samples_per_packet_ == 9);
  TEST_CHECK(tcp_inst->optimum_packet_size_ == 2048);
  TEST_CHECK(tcp_inst->thread_per_connection_ == true);
  TEST_CHECK(tcp_inst->datalink_release_delay_ == 5000);
  TEST_CHECK(tcp_inst->datalink_control_chunks_ == 16);
  TEST_CHECK(tcp_inst->local_address_str_ == "localhost");
  TEST_CHECK(tcp_inst->enable_nagle_algorithm_ == true);
  TEST_CHECK(tcp_inst->conn_retry_initial_delay_ == 1000);
  TEST_CHECK(tcp_inst->conn_retry_backoff_multiplier_ == 4);
  TEST_CHECK(tcp_inst->conn_retry_attempts_ == 4);
  TEST_CHECK(tcp_inst->passive_reconnect_duration_ == 4000);
  TEST_CHECK(tcp_inst->max_output_pause_period_ == 1000);

  TransportInst_rch inst2 = TransportRegistry::instance()->get_inst("anothertcp");
  TEST_CHECK(inst2 != 0);
  TEST_CHECK(inst2->name() == "anothertcp");

  TransportConfig_rch config = TransportRegistry::instance()->get_config("myconfig");
  TEST_CHECK(config != 0);
  TEST_CHECK(config->instances_.size() == 2);
  TEST_CHECK(config->instances_[0] == inst);
  TEST_CHECK(config->instances_[1] == inst2);
  TEST_CHECK(config->swap_bytes_ == true);
  TEST_CHECK(config->passive_connect_duration_ == 20000);

  TransportConfig_rch default_config =
    TransportRegistry::instance()->get_config("test1.ini");
  TEST_CHECK(default_config != 0);
  //std::cout << "size=" << default_config->instances_.size() << std::endl;
  //for (unsigned int i = 0; i < default_config->instances_.size(); ++i) {
  //  std::cout << "  " << default_config->instances_[i]->name() << std::endl;
  //}
  // Should be in alpa-sorted order
  TEST_CHECK(default_config->instances_.size() == 11);
  TEST_CHECK(default_config->instances_[0] == inst2);  // anothertcp
  TEST_CHECK(default_config->instances_[2] == inst);   // mytcp
  TEST_CHECK(default_config->instances_[9]->name() == std::string("tcp7"));
  TEST_CHECK(default_config->swap_bytes_ == false);
  TEST_CHECK(default_config->passive_connect_duration_ == 10000);

  TransportConfig_rch global_config =
    TransportRegistry::instance()->global_config();
  TEST_CHECK(global_config->name() == "myconfig");

  return 0;
}
