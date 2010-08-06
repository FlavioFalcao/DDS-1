// -*- C++ -*-
// ============================================================================
/**
 *  @file   subscriber.cpp
 *
 *  $Id$
 *
 *
 */
// ============================================================================


#include "DataReaderListener.h"
#include "TestTypeSupportImpl.h"
#include "Partition_Table.h"

#include <dds/DCPS/Service_Participant.h>
#include <dds/DCPS/Marked_Default_Qos.h>
#include <dds/DCPS/SubscriberImpl.h>
#include <dds/DCPS/transport/framework/TheTransportFactory.h>
#include <dds/DCPS/transport/simpleTCP/SimpleTcpConfiguration.h>

#ifdef ACE_AS_STATIC_LIBS
#include <dds/DCPS/transport/simpleTCP/SimpleTcp.h>
#endif

#include <vector>
#include <algorithm>
#include <iostream>

using std::cerr;
using std::endl;


OpenDDS::DCPS::TransportIdType transport_impl_id = 1;

int ACE_TMAIN(int argc, ACE_TCHAR* argv[])
{
  try
    {
      DDS::DomainParticipantFactory_var dpf;
      DDS::DomainParticipant_var participant;

      dpf = TheParticipantFactoryWithArgs(argc, argv);
      participant = dpf->create_participant(411,
                                            PARTICIPANT_QOS_DEFAULT,
                                            DDS::DomainParticipantListener::_nil(),
                                            ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);
      if (CORBA::is_nil (participant.in ())) {
        cerr << "create_participant failed." << endl;
        return 1 ;
      }

      Test::DataTypeSupportImpl * const dts_servant =
        new Test::DataTypeSupportImpl;

      if (DDS::RETCODE_OK != dts_servant->register_type(participant.in (),
                                                        ""))
        {
          cerr << "Failed to register the DataTypeSupport." << endl;
          exit(1);
        }

      CORBA::String_var type_name = dts_servant->get_type_name ();

      DDS::TopicQos topic_qos;
      participant->get_default_topic_qos(topic_qos);
      DDS::Topic_var topic =
        participant->create_topic("Data",
                                  type_name.in (),
                                  topic_qos,
                                  DDS::TopicListener::_nil(),
                                  ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);
      if (CORBA::is_nil (topic.in ())) {
        cerr << "Failed to create_topic." << endl;
        exit(1);
      }

      // Initialize the transport
      OpenDDS::DCPS::TransportImpl_rch tcp_impl =
        TheTransportFactory->create_transport_impl (transport_impl_id,
                                                    ::OpenDDS::DCPS::AUTO_CONFIG);

      size_t const num_partitions =
        sizeof (Test::Requested::PartitionConfigs)
        / sizeof (Test::Requested::PartitionConfigs[0]);

      Test::PartitionConfig const * const begin =
        Test::Requested::PartitionConfigs;
      Test::PartitionConfig const * const end =
        begin + num_partitions;

      // Keep the readers around long enough for the publications and
      // subscriptions to match.
      std::vector<DDS::DataReader_var> readers (num_partitions);

      for (Test::PartitionConfig const * i = begin; i != end; ++i)
      {
        DDS::SubscriberQos sub_qos;
        participant->get_default_subscriber_qos (sub_qos);

        // Specify partitions we're requesting.
        CORBA::ULong n = 0;
        DDS::StringSeq & names = sub_qos.partition.name;
        for (char const * const * s = (*i).partitions;
             s != 0 && *s != 0;
             ++s, ++n)
        {
          CORBA::ULong const new_len = names.length () + 1;
          names.length (new_len);
          names[n] = *s;
        }

        // Create the subscriber and attach to the corresponding
        // transport.
        DDS::Subscriber_var sub =
          participant->create_subscriber (sub_qos,
                                          DDS::SubscriberListener::_nil (),
                                          ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);
        if (CORBA::is_nil (sub.in ()))
        {
          cerr << "Failed to create_subscriber." << endl;
          exit(1);
        }

        // Attach the subscriber to the transport.
        OpenDDS::DCPS::SubscriberImpl* const sub_impl =
          dynamic_cast<OpenDDS::DCPS::SubscriberImpl*> (sub.in ());
        if (sub_impl == 0)
        {
          cerr << "Failed to obtain subscriber servant\n" << endl;
          exit(1);
        }

        OpenDDS::DCPS::AttachStatus const status =
          sub_impl->attach_transport(tcp_impl.in());
        if (status != OpenDDS::DCPS::ATTACH_OK)
        {
          std::string status_str;
          switch (status)
          {
          case OpenDDS::DCPS::ATTACH_BAD_TRANSPORT:
            status_str = "ATTACH_BAD_TRANSPORT";
            break;
          case OpenDDS::DCPS::ATTACH_ERROR:
            status_str = "ATTACH_ERROR";
            break;
          case OpenDDS::DCPS::ATTACH_INCOMPATIBLE_QOS:
            status_str = "ATTACH_INCOMPATIBLE_QOS";
            break;
          default:
            status_str = "Unknown Status";
            break;
          }
          cerr << "Failed to attach to the transport. Status == "
               << status_str.c_str() << endl;
          exit(1);
        }

        DDS::DataReaderListener_var listener (
          new Test::DataReaderListener ((*i).expected_matches));

        // Create the Datareaders
        DDS::DataReaderQos dr_qos;
        sub->get_default_datareader_qos (dr_qos);
        DDS::DataReader_var dr = sub->create_datareader (topic.in (),
                                                         dr_qos,
                                                         listener.in (),
                                                         ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);
        if (CORBA::is_nil (dr.in ())) {
          cerr << "create_datareader failed." << endl;
          exit(1);
        }

        readers.push_back (dr);
      }

      ACE_OS::sleep (15);

//       {
//         // Force contents of writers vector to be destroyed now.
//         std::vector<DDS::DataReader_var> tmp;
//         tmp.swap (readers);
//       }

      if (!CORBA::is_nil (participant.in ())) {
        participant->delete_contained_entities();
      }
      if (!CORBA::is_nil (dpf.in ())) {
        dpf->delete_participant(participant.in ());
      }
      ACE_OS::sleep(2);

      TheTransportFactory->release();
      TheServiceParticipant->shutdown ();
    }
  catch (CORBA::Exception& e)
    {
      cerr << "SUB: Exception caught in main ():" << endl << e << endl;
      return 1;
    }

  return 0;
}
