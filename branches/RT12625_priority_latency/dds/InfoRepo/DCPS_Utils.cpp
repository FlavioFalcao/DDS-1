#include "DcpsInfo_pch.h"
#include /**/ "DCPS_Utils.h"
#include "dds/DCPS/Qos_Helper.h"

#include "ace/ACE.h"  /* For ACE::wild_match() */
#include "ace/OS_NS_string.h"

namespace
{
  bool
  matching_partitions (::DDS::PartitionQosPolicy const & pub,
                       ::DDS::PartitionQosPolicy const & sub)
  {
    ::DDS::StringSeq const & publisher_partitions  = pub.name;
    ::DDS::StringSeq const & subscriber_partitions = sub.name;

    bool match = false;

    // Both publisher and subscriber are in the same (default)
    // partition if the partition string sequence lengths are both
    // zero.
    CORBA::ULong const pub_len = publisher_partitions.length ();
    CORBA::ULong const sub_len = subscriber_partitions.length ();

    if (pub_len == 0 && sub_len == 0)
      match = true;
    else if (pub_len == 0)
    {
      // Attempt to match an explicitly specified empty string
      // partition against the special zero length partition
      // sequence.
      for (CORBA::ULong n = 0; n < sub_len && !match; ++n)
        {
          char const * const sname = subscriber_partitions[n];
          if (*sname == 0)
            match = true;
        }
    }

    // We currently don't support "[]" wildcards.  See pattern_match()
    // function above for details.
    static char const wildcard[] = "*?" /* "[[*?])" */;

    // @todo Really slow (O(n^2)) search.  Optimize if partition name
    //       sequences will potentially be very long.  If they remain
    //       short, optimizing may be unnecessary or simply not worth
    //       the trouble.
    for (CORBA::ULong i = 0; i < pub_len && !match; ++i)
      {
        char const * const pname = publisher_partitions[i];

        if (sub_len == 0 && *pname == 0)
        {
          // Attempt to match an explicitly specified empty string
          // partition against the special zero length partition
          // sequence.
          match = true;
          continue;
        }

        // The DDS specification requires pattern matching
        // capabilities corresponding to those provided by the POSIX
        // fnmatch() function.  However, some platforms, such as
        // Windows, do not provide such a function.  To be
        // completely portable across all platforms we instead use
        // ACE::wild_match().  Currently this prevents matching of
        // patterns containing square brackets, i.e. "[]".
        bool const pub_is_wildcard =
          (ACE_OS::strcspn (pname, wildcard) != ACE_OS::strlen (pname));
          // ACE::wild_match (wildcard, pname);

        for (CORBA::ULong j = 0; j < sub_len && !match; ++j)
          {
            char const * const sname = subscriber_partitions[j];

            bool const sub_is_wildcard =
              (ACE_OS::strcspn (sname, wildcard) != ACE_OS::strlen (sname));
            // ACE::wild_match (wildcard, sname);

            if (pub_is_wildcard && sub_is_wildcard)
              continue;

            if (pub_is_wildcard)
              match = ACE::wild_match (sname, pname);
            else if (sub_is_wildcard)
              match = ACE::wild_match (pname, sname);
            else
              match = (ACE_OS::strcmp (pname, sname) == 0);
          }
      }

    return match;
  }
}

void
increment_incompatibility_count (OpenDDS::DCPS::IncompatibleQosStatus* status,
                                 ::DDS::QosPolicyId_t incompatible_policy)
{
  ++status->total_count;
  ++status->count_since_last_send;
  status->last_policy_id = incompatible_policy;
  CORBA::ULong const size = status->policies.length();
  CORBA::ULong count = 0;
  bool updated = false;
  for ( ; !updated && count < size; ++count)
  {
    if (status->policies[count].policy_id == incompatible_policy)
    {
      ++status->policies[count].count;
      updated = true;
    }
  }

  if (!updated)
  {
    ::DDS::QosPolicyCount policy;
    policy.policy_id = incompatible_policy;
    policy.count = 1;
    status->policies.length(count + 1);
    status->policies[count] = policy;
  }
}


// This is extremely simple now but will get very complex when more
// QOSes are supported.
bool
compatibleQOS (DCPS_IR_Publication  * publication,
               DCPS_IR_Subscription * subscription)
{
  bool compatible = true;
  OpenDDS::DCPS::IncompatibleQosStatus* writerStatus 
    = publication->get_incompatibleQosStatus();
  OpenDDS::DCPS::IncompatibleQosStatus* readerStatus
    = subscription->get_incompatibleQosStatus();

  if (publication->get_transport_id() != subscription->get_transport_id())
  {
    // Transports are not compatible.
    compatible = false;
    increment_incompatibility_count(writerStatus,
                                    ::OpenDDS::TRANSPORTTYPE_QOS_POLICY_ID);
    increment_incompatibility_count(readerStatus,
                                    ::OpenDDS::TRANSPORTTYPE_QOS_POLICY_ID);
  }

  ::DDS::DataWriterQos const * const writerQos =
    publication->get_datawriter_qos();
  ::DDS::DataReaderQos const * const readerQos =
      subscription->get_datareader_qos ();

  // Verify compatibility of DataWriterQos and DataReaderQos
  compatible = compatible && compatibleQOS (writerQos, readerQos, 
                                            writerStatus, readerStatus);

  ::DDS::PublisherQos const * const pubQos =
      publication->get_publisher_qos ();
  ::DDS::SubscriberQos const * const subQos =
      subscription->get_subscriber_qos ();

  // Verify publisher and subscriber are in a matching partition.
  //
  // According to the DDS spec:
  //
  //   Failure to match partitions is not considered an incompatible
  //   QoS and does not trigger any listeners nor conditions.
  //
  // Don't increment the incompatibity count.
  compatible = compatible && matching_partitions (pubQos->partition,
                                                  subQos->partition);

  return compatible;
}


bool
compatibleQOS (const ::DDS::PublisherQos * /*pubQos*/,
               const ::DDS::SubscriberQos * /*subQos*/)
{
  return true;
}

bool
compatibleQOS (const ::DDS::DataWriterQos * writerQos,
               const ::DDS::DataReaderQos * readerQos,
               OpenDDS::DCPS::IncompatibleQosStatus* writerStatus,
               OpenDDS::DCPS::IncompatibleQosStatus* readerStatus)
{
  bool compatible = true;

  // Check the RELIABILITY_QOS_POLICY_ID
  if (writerQos->reliability.kind < readerQos->reliability.kind )
  {
    compatible = false;

    increment_incompatibility_count(writerStatus,
                                    ::DDS::RELIABILITY_QOS_POLICY_ID);
    increment_incompatibility_count(readerStatus,
                                    ::DDS::RELIABILITY_QOS_POLICY_ID);
  }

  // Check the DURABILITY_QOS_POLICY_ID
  if ( writerQos->durability.kind < readerQos->durability.kind )
  {
    compatible = false;

    increment_incompatibility_count(writerStatus,
                                    ::DDS::DURABILITY_QOS_POLICY_ID);
    increment_incompatibility_count(readerStatus,
                                    ::DDS::DURABILITY_QOS_POLICY_ID);
  }

  // Check the LIVELINESS_QOS_POLICY_ID
  // invalid if offered kind is less than requested kind OR
  //         if offered liveliness duration greater than requested
  //         liveliness duration
  if (writerQos->liveliness.kind < readerQos->liveliness.kind
      || writerQos->liveliness.lease_duration
         > readerQos->liveliness.lease_duration)
  {
    compatible = false;

    increment_incompatibility_count(writerStatus,
                                    ::DDS::LIVELINESS_QOS_POLICY_ID);
    increment_incompatibility_count(readerStatus,
                                    ::DDS::LIVELINESS_QOS_POLICY_ID);
  }

  // Check the DEADLINE_QOS_POLICY_ID
  //   Offered deadline must be less than or equal to the requested
  //   deadline.
  if (writerQos->deadline.period > readerQos->deadline.period)
  {
    compatible = false;

    increment_incompatibility_count(writerStatus,
                                    ::DDS::DEADLINE_QOS_POLICY_ID);
    increment_incompatibility_count(readerStatus,
                                    ::DDS::DEADLINE_QOS_POLICY_ID);
  }

  // Check the LATENCY_BUDGET
  //   The reader's duration must be greater than or equal to the writer's
  if (readerQos->latency_budget.duration < writerQos->latency_budget.duration)
  {
    compatible = false;

    increment_incompatibility_count(writerStatus,
                                    ::DDS::LATENCYBUDGET_QOS_POLICY_ID);
    increment_incompatibility_count(readerStatus,
                                    ::DDS::LATENCYBUDGET_QOS_POLICY_ID);
  }

  return compatible;
}


bool should_check_compatibility_upon_change (const ::DDS::DataReaderQos & qos1,
                                             const ::DDS::DataReaderQos & qos2)
{
  return !(
    (qos1.deadline == qos2.deadline) &&
    (qos1.latency_budget == qos2.latency_budget)
    );
}


bool should_check_compatibility_upon_change (const ::DDS::DataWriterQos & qos1,
                                             const ::DDS::DataWriterQos & qos2)
{
  return !(
    (qos1.deadline == qos2.deadline) &&
    (qos1.latency_budget == qos2.latency_budget)
    );
}


bool should_check_compatibility_upon_change (const ::DDS::SubscriberQos & /*qos1*/,
                                             const ::DDS::SubscriberQos & /*qos2*/)
{
  return false;
}


bool should_check_compatibility_upon_change (const ::DDS::PublisherQos & /*qos1*/,
                                             const ::DDS::PublisherQos & /*qos2*/)
{
  return false;
}


bool should_check_compatibility_upon_change (const ::DDS::TopicQos & qos1,
                                             const ::DDS::TopicQos & qos2)
{
  return ! (qos1.deadline == qos2.deadline);
}


bool should_check_compatibility_upon_change (const ::DDS::DomainParticipantQos & /*qos1*/,
                                             const ::DDS::DomainParticipantQos & /*qos2*/)
{
  return false;
}

bool should_check_association_upon_change (const ::DDS::DataReaderQos & qos1,
                                           const ::DDS::DataReaderQos & qos2)
{
  return !(
    (qos1.deadline == qos2.deadline) &&
    (qos1.latency_budget == qos2.latency_budget)
    );
}


bool should_check_association_upon_change (const ::DDS::DataWriterQos & qos1,
                                           const ::DDS::DataWriterQos & qos2)
{
  return !(
    (qos1.deadline == qos2.deadline) &&
    (qos1.latency_budget == qos2.latency_budget)
    );
}


bool should_check_association_upon_change (const ::DDS::SubscriberQos & qos1,
                                           const ::DDS::SubscriberQos & qos2)
{
  return !(qos1.partition == qos2.partition);
}


bool should_check_association_upon_change (const ::DDS::PublisherQos & qos1,
                                           const ::DDS::PublisherQos & qos2)
{
  return !(qos1.partition == qos2.partition);
}


bool should_check_association_upon_change (const ::DDS::TopicQos & /*qos1*/,
                                           const ::DDS::TopicQos & /*qos2*/)
{
  return false;
}

bool should_check_association_upon_change (const ::DDS::DomainParticipantQos & /*qos1*/,
                                    const ::DDS::DomainParticipantQos & /*qos2*/)
{
  return false;
}



