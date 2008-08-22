// -*- C++ -*-
// ============================================================================
/**
 *  @file   DCPSInfo_i.h
 *
 *  $Id$
 *
 *
 */
// ============================================================================

#ifndef DCPSINFO_I_H
#define DCPSINFO_I_H



#include /**/ "DCPS_IR_Topic.h"
#include /**/ "DCPS_IR_Topic_Description.h"
#include /**/ "DCPS_IR_Participant.h"
#include /**/ "DCPS_IR_Publication.h"
#include /**/ "DCPS_IR_Subscription.h"
#include /**/ "DCPS_IR_Domain.h"
#include "GuidGenerator.h"
#include "UpdateManager.h"

#include /**/ "dds/DdsDcpsInfoS.h"
#include /**/ "dds/DdsDcpsDataReaderRemoteC.h"
#include /**/ "dds/DdsDcpsDataWriterRemoteC.h"

#include "tao/ORB_Core.h"

#if !defined (ACE_LACKS_PRAGMA_ONCE)
#pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */

// typedef declarations
typedef ACE_Map_Manager< ::DDS::DomainId_t, DCPS_IR_Domain*, ACE_Null_Mutex> DCPS_IR_Domain_Map;

// Forward declaration
namespace Update { class Manager; }

/**
 * @class TAO_DDS_DCPSInfo_i
 *
 * @brief Implementation of the DCPSInfo
 *
 * This is the Information Repository object.  Clients of
 * the system will use the CORBA reference of this object.
 */
class  TAO_DDS_DCPSInfo_i : public virtual POA_OpenDDS::DCPS::DCPSInfo
{
public:
  //Constructor
  TAO_DDS_DCPSInfo_i (CORBA::ORB_ptr orb, bool reincarnate, long federation = 0);

  //Destructor
  virtual ~TAO_DDS_DCPSInfo_i (void);

  virtual CORBA::Boolean attach_participant (
      ::DDS::DomainId_t domainId,
      const OpenDDS::DCPS::RepoId& participantId
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
      , OpenDDS::DCPS::Invalid_Domain
      , OpenDDS::DCPS::Invalid_Participant
    ));

  virtual OpenDDS::DCPS::TopicStatus assert_topic (
      OpenDDS::DCPS::RepoId_out topicId,
      ::DDS::DomainId_t domainId,
      const OpenDDS::DCPS::RepoId& participantId,
      const char * topicName,
      const char * dataTypeName,
      const ::DDS::TopicQos & qos
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
      , OpenDDS::DCPS::Invalid_Domain
      , OpenDDS::DCPS::Invalid_Participant
    ));

  /**
   * @brief Add a previously existing topic to the repository.
   *
   * @param topicId       the Topic Entity GUID Id to use.
   * @param domainId      the Domain in which the Topic is contained.
   * @param participantId the Participant in which the Topic is contained.
   * @param topicName     the name of the Topic.
   * @param dataTypeName  the name of the data type.
   * @param qos           the QoS value to use for the Topic.
   *
   * Adds a Topic Entity to the repository using a specified TopicId
   * value.  If the TopicId indicates that this Topic was created by
   * within this repository (the federation Id is the current repositories
   * federation Id), this method will ensure that any subsequent calls to
   * add a Topic and obtain a newly generated Id value will return an Id
   * value greater than the Id value of the current one.
   */
  bool add_topic (const OpenDDS::DCPS::RepoId& topicId,
                  ::DDS::DomainId_t domainId,
                  const OpenDDS::DCPS::RepoId& participantId,
                  const char* topicName,
                  const char* dataTypeName,
                  const ::DDS::TopicQos& qos);

  virtual OpenDDS::DCPS::TopicStatus find_topic (
      ::DDS::DomainId_t domainId,
      const char * topicName,
      CORBA::String_out dataTypeName,
      ::DDS::TopicQos_out qos,
      OpenDDS::DCPS::RepoId_out topicId
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
      , OpenDDS::DCPS::Invalid_Domain
    ));

  virtual OpenDDS::DCPS::TopicStatus remove_topic (
      ::DDS::DomainId_t domainId,
      const OpenDDS::DCPS::RepoId& participantId,
      const OpenDDS::DCPS::RepoId& topicId
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
      , OpenDDS::DCPS::Invalid_Domain
      , OpenDDS::DCPS::Invalid_Participant
      , OpenDDS::DCPS::Invalid_Topic
    ));

  virtual OpenDDS::DCPS::TopicStatus enable_topic (
      ::DDS::DomainId_t domainId,
      const OpenDDS::DCPS::RepoId& participantId,
      const OpenDDS::DCPS::RepoId& topicId
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
      , OpenDDS::DCPS::Invalid_Domain
      , OpenDDS::DCPS::Invalid_Participant
      , OpenDDS::DCPS::Invalid_Topic
    ));

  virtual OpenDDS::DCPS::RepoId add_publication (
      ::DDS::DomainId_t domainId,
      const OpenDDS::DCPS::RepoId& participantId,
      const OpenDDS::DCPS::RepoId& topicId,
      OpenDDS::DCPS::DataWriterRemote_ptr publication,
      const ::DDS::DataWriterQos & qos,
      const OpenDDS::DCPS::TransportInterfaceInfo & transInfo,
      const ::DDS::PublisherQos & publisherQos
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
      , OpenDDS::DCPS::Invalid_Domain
      , OpenDDS::DCPS::Invalid_Participant
      , OpenDDS::DCPS::Invalid_Topic
    ));

  /**
   * @brief Add a previously existing publication to the repository.
   *
   * @param domainId      the Domain in which the Publication is contained.
   * @param participantId the Participant in which the Publication is contained.
   * @param topicId       the Topic of the Publication.
   * @param pubId         the GUID Id value to use for the Publication.
   * @param pub_str       stringified publication callback to DataWriter.
   * @param qos           the QoS value of the DataWriter.
   * @param transInfo     the transport information for the Publication.
   * @param publisherQos  the QoS value of the Publisher.
   *
   * Adds a Publication to the repository using a specified Publication
   * GUID Id value.  If the PublicationId indicates that this Publication
   * was created by within this repository (the federation Id is the
   * current repositories federation Id), this method will ensure that any
   * subsequent calls to add a Publication and obtain a newly generated
   * Id value will return an Id value greater than the Id value of the
   * current one.
   */
  bool add_publication (::DDS::DomainId_t domainId,
                        const OpenDDS::DCPS::RepoId& participantId,
                        const OpenDDS::DCPS::RepoId& topicId,
                        const OpenDDS::DCPS::RepoId& pubId,
                        const char* pub_str,
                        const ::DDS::DataWriterQos & qos,
                        const OpenDDS::DCPS::TransportInterfaceInfo & transInfo,
                        const ::DDS::PublisherQos & publisherQos);

    virtual void remove_publication (
      ::DDS::DomainId_t domainId,
      const OpenDDS::DCPS::RepoId& participantId,
      const OpenDDS::DCPS::RepoId& publicationId
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
      , OpenDDS::DCPS::Invalid_Domain
      , OpenDDS::DCPS::Invalid_Participant
      , OpenDDS::DCPS::Invalid_Publication
    ));

  virtual OpenDDS::DCPS::RepoId add_subscription (
      ::DDS::DomainId_t domainId,
      const OpenDDS::DCPS::RepoId& participantId,
      const OpenDDS::DCPS::RepoId& topicId,
      OpenDDS::DCPS::DataReaderRemote_ptr subscription,
      const ::DDS::DataReaderQos & qos,
      const OpenDDS::DCPS::TransportInterfaceInfo & transInfo,
      const ::DDS::SubscriberQos & subscriberQos
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
      , OpenDDS::DCPS::Invalid_Domain
      , OpenDDS::DCPS::Invalid_Participant
      , OpenDDS::DCPS::Invalid_Topic
    ));

  /**
   * @brief Add a previously existing subscription to the repository.
   *
   * @param domainId      the Domain in which the Subscription is contained.
   * @param participantId the Participant in which the Subscription is contained.
   * @param topicId       the Topic of the Subscription.
   * @param subId         the GUID Id value to use for the Subscription.
   * @param sub_str       stringified publication callback to DataReader.
   * @param qos           the QoS value of the DataReader.
   * @param transInfo     the transport information for the Subscription.
   * @param subscriberQos the QoS value of the Subscriber.
   *
   * Adds a Subscription to the repository using a specified Subscription
   * GUID Id value.  If the SubscriptionId indicates that this Subscription
   * was created by within this repository (the federation Id is the
   * current repositories federation Id), this method will ensure that any
   * subsequent calls to add a Publication and obtain a newly generated
   * Id value will return an Id value greater than the Id value of the
   * current one.
   */
  bool add_subscription (::DDS::DomainId_t domainId,
                         const OpenDDS::DCPS::RepoId& participantId,
                         const OpenDDS::DCPS::RepoId& topicId,
                         const OpenDDS::DCPS::RepoId& subId,
                         const char* sub_str,
                         const ::DDS::DataReaderQos & qos,
                         const OpenDDS::DCPS::TransportInterfaceInfo & transInfo,
                         const ::DDS::SubscriberQos & subscriberQos);

    virtual void remove_subscription (
      ::DDS::DomainId_t domainId,
      const OpenDDS::DCPS::RepoId& participantId,
      const OpenDDS::DCPS::RepoId& subscriptionId
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
      , OpenDDS::DCPS::Invalid_Domain
      , OpenDDS::DCPS::Invalid_Participant
      , OpenDDS::DCPS::Invalid_Subscription
    ));

  virtual OpenDDS::DCPS::RepoId add_domain_participant (
      ::DDS::DomainId_t domain,
      const ::DDS::DomainParticipantQos & qos
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
      , OpenDDS::DCPS::Invalid_Domain
    ));

  /**
   * @brief Add a previously existing participant to the repository.
   *
   * @param domainId      the Domain in which the Participant is contained.
   * @param participantId the GUID Id value to use for the Participant.
   * @param qos           the QoS value of the Participant.
   *
   * Adds a Participant to the repository using a specified Participant
   * GUID Id value.  If the ParticipantId indicates that this Participant
   * was created by within this repository (the federation Id is the
   * current repositories federation Id), this method will ensure that any
   * subsequent calls to add a Publication and obtain a newly generated
   * Id value will return an Id value greater than the Id value of the
   * current one.
   */
  bool add_domain_participant (::DDS::DomainId_t domainId
                               , const OpenDDS::DCPS::RepoId& participantId
                               , const ::DDS::DomainParticipantQos & qos);

    virtual void remove_domain_participant (
      ::DDS::DomainId_t domainId,
      const OpenDDS::DCPS::RepoId& participantId
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
      , OpenDDS::DCPS::Invalid_Domain
      , OpenDDS::DCPS::Invalid_Participant
    ));

  virtual void ignore_domain_participant (
      ::DDS::DomainId_t domainId,
      const OpenDDS::DCPS::RepoId& myParticipantId,
      CORBA::Long ignoreKey
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
      , OpenDDS::DCPS::Invalid_Domain
      , OpenDDS::DCPS::Invalid_Participant
    ));

  virtual void ignore_topic (
      ::DDS::DomainId_t domainId,
      const OpenDDS::DCPS::RepoId& myParticipantId,
      CORBA::Long ignoreKey
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
      , OpenDDS::DCPS::Invalid_Domain
      , OpenDDS::DCPS::Invalid_Participant
      , OpenDDS::DCPS::Invalid_Topic
    ));

  virtual void ignore_subscription (
      ::DDS::DomainId_t domainId,
      const OpenDDS::DCPS::RepoId& myParticipantId,
      CORBA::Long ignoreKey
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
      , OpenDDS::DCPS::Invalid_Domain
      , OpenDDS::DCPS::Invalid_Participant
      , OpenDDS::DCPS::Invalid_Subscription
    ));

  virtual void ignore_publication (
      ::DDS::DomainId_t domainId,
      const OpenDDS::DCPS::RepoId& myParticipantId,
      CORBA::Long ignoreKey
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
      , OpenDDS::DCPS::Invalid_Domain
      , OpenDDS::DCPS::Invalid_Participant
      , OpenDDS::DCPS::Invalid_Publication
    ));


  virtual CORBA::Boolean update_publication_qos (
      ::DDS::DomainId_t domainId,
      const OpenDDS::DCPS::RepoId& partId,
      const OpenDDS::DCPS::RepoId& dwId,
      const ::DDS::DataWriterQos & qos,
      const ::DDS::PublisherQos & publisherQos
    )
    ACE_THROW_SPEC ((
    CORBA::SystemException
    , OpenDDS::DCPS::Invalid_Domain
    , OpenDDS::DCPS::Invalid_Participant
    , OpenDDS::DCPS::Invalid_Publication
    ));

  /// Entry for federation updates of DataWriterQos values.
  void update_publication_qos (
    ::DDS::DomainId_t            domainId,
    const OpenDDS::DCPS::RepoId& partId,
    const OpenDDS::DCPS::RepoId& dwId,
    const ::DDS::DataWriterQos&  qos
  );

  /// Entry for federation updates of PublisherQos values.
  void update_publication_qos (
    ::DDS::DomainId_t            domainId,
    const OpenDDS::DCPS::RepoId& partId,
    const OpenDDS::DCPS::RepoId& dwId,
    const ::DDS::PublisherQos&   qos
  );

  virtual CORBA::Boolean update_subscription_qos (
      ::DDS::DomainId_t domainId,
      const OpenDDS::DCPS::RepoId& partId,
      const OpenDDS::DCPS::RepoId& drId,
      const ::DDS::DataReaderQos & qos,
      const ::DDS::SubscriberQos & subscriberQos
    )
    ACE_THROW_SPEC ((
    CORBA::SystemException
    , OpenDDS::DCPS::Invalid_Domain
    , OpenDDS::DCPS::Invalid_Participant
    , OpenDDS::DCPS::Invalid_Subscription
    ));


  /// Entry for federation updates of DataReaderQos values.
  void update_subscription_qos (
    ::DDS::DomainId_t            domainId,
    const OpenDDS::DCPS::RepoId& partId,
    const OpenDDS::DCPS::RepoId& drId,
    const ::DDS::DataReaderQos&  qos
  );

  /// Entry for federation updates of SubscriberQos values.
  void update_subscription_qos (
    ::DDS::DomainId_t            domainId,
    const OpenDDS::DCPS::RepoId& partId,
    const OpenDDS::DCPS::RepoId& drId,
    const ::DDS::SubscriberQos&  qos
  );

  virtual CORBA::Boolean update_topic_qos (
      const OpenDDS::DCPS::RepoId& topicId,
      ::DDS::DomainId_t domainId,
      const OpenDDS::DCPS::RepoId& participantId,
      const ::DDS::TopicQos & qos
    )
    ACE_THROW_SPEC ((
    CORBA::SystemException
    , OpenDDS::DCPS::Invalid_Domain
    , OpenDDS::DCPS::Invalid_Participant
    , OpenDDS::DCPS::Invalid_Topic
    ));


  virtual CORBA::Boolean update_domain_participant_qos (
    ::DDS::DomainId_t domain,
    const ::OpenDDS::DCPS::RepoId& participantId,
    const ::DDS::DomainParticipantQos & qos
  )
  ACE_THROW_SPEC ((
    ::CORBA::SystemException,
    ::OpenDDS::DCPS::Invalid_Domain,
    ::OpenDDS::DCPS::Invalid_Participant
  ));

  /**
   * @brief assert new ownership for a participant and its contained entities.
   *
   * @param domainId      the domain in which the participant resides.
   * @param participantId the participant to be owned.
   * @param sender        the repository sending the update data.
   * @param owner         the repository which is to make callbacks for
   *                      entities within the participant.
   *
   * This establishes @c owner as the new owner of the participant.
   * Ownership consists of calling back to the reader and writer remote
   * interfaces when associations are established and removed from a
   * publication or subscription.  Owner may be the special value of
   * OWNER_NONE to indicate that the previous owner is no longer
   * available to make callbacks and the application has not indicated
   * which repository is to replace it in this capacity.
   *
   * The @c sender of the update is included so that the participant can
   * check that transitions to OWNER_NONE are only honored when initiated
   * by the current owner of the participant.
   */
  void changeOwnership(
         ::DDS::DomainId_t              domainId,
         const ::OpenDDS::DCPS::RepoId& participantId,
         long                           sender,
         long                           owner
       );
 

  /// Called to load the domains
  /// returns the number of domains that were loaded.
  /// Currently after loading domains, it
  ///  invoke 'init_persistence'
  int load_domains (const ACE_TCHAR* filename, bool use_bit);

  /// Initialize the transport for the Built-In Topics
  /// Returns 0 (zero) if succeeds
  int init_transport (int listen_address_given,
                      const ACE_INET_Addr listen);

  bool receive_image (const Update::UImage& image);

  /// Add an additional Updater interface.
  void add( Update::Updater* updater);

private:

  bool init_persistence (void);

private:
  DCPS_IR_Domain_Map domains_;
  CORBA::ORB_var orb_;

  long          federation_;
  GuidGenerator participantIdGenerator_;

  Update::Manager* um_;
  bool reincarnate_;
};


#endif /* DCPSINFO_I_H_  */
