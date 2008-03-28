// -*- C++ -*-
//
// $Id$
#include "FederatorLinkListener.h"
#include "FederatorManager.h"
#include "LinkStateTypeSupportImpl.h"

namespace OpenDDS { namespace Federator {

FederatorLinkListener::FederatorLinkListener( FederatorManager& manager)
 : manager_( manager)
{
  ACE_DEBUG((LM_DEBUG,
    ACE_TEXT("(%P|%t) FederatorLinkListener::FederatorLinkListener\n")
  ));
}

FederatorLinkListener::~FederatorLinkListener (void)
{
  ACE_DEBUG((LM_DEBUG,
    ACE_TEXT("(%P|%t) FederatorLinkListener::~FederatorLinkListener\n")
  ));
}

void FederatorLinkListener::on_data_available(
  ::DDS::DataReader_ptr reader
)
ACE_THROW_SPEC ((
  CORBA::SystemException
))
{
  if( OpenDDS::DCPS::DCPS_debug_level > 0) {
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) FederatorLinkListener::on_data_available\n")));
  }

  try
  {
    // Get the type specific reader.
    ::OpenDDS::Federator::LinkStateDataReader_var dataReader
      = ::OpenDDS::Federator::LinkStateDataReader::_narrow( reader);
    if( CORBA::is_nil( dataReader.in ())) {
      ACE_ERROR ((LM_ERROR,
        ACE_TEXT("(%P|%t) FederatorLinkListener::on_data_available - _narrow failed.\n")));
      return;
    }

    ::OpenDDS::Federator::LinkState sample;
    ::DDS::SampleInfo   info;
    ::DDS::ReturnCode_t status = dataReader->read_next_sample( sample, info);

    if( status == ::DDS::RETCODE_OK) {
      // Delegate processing to the federation manager.
      this->manager_.updateLinkState( sample, info);

    } else {
      ACE_ERROR((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: FederatorLinkListener::on_data_available: read status==%d\n"),
        status
      ));
    }

  } catch( const CORBA::Exception& ex) {
    ex._tao_print_exception ("(%P|%t) FederatorLinkListener::read - ");
  }
}

void FederatorLinkListener::on_requested_deadline_missed (
    ::DDS::DataReader_ptr /* reader */,
    const ::DDS::RequestedDeadlineMissedStatus & /* status */)
  ACE_THROW_SPEC ((CORBA::SystemException))
{
  if( OpenDDS::DCPS::DCPS_debug_level > 0) {
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) ")
               ACE_TEXT("FederatorLinkListener::on_requested_deadline_missed\n")));
  }
}

void FederatorLinkListener::on_requested_incompatible_qos (
  ::DDS::DataReader_ptr /* reader */,
  const ::DDS::RequestedIncompatibleQosStatus & /* status */)
  ACE_THROW_SPEC ((CORBA::SystemException))
{
  if( OpenDDS::DCPS::DCPS_debug_level > 0) {
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) FederatorLinkListener::")
               ACE_TEXT("on_requested_incompatible_qos\n")));
  }
}

void FederatorLinkListener::on_liveliness_changed (
    ::DDS::DataReader_ptr /* reader */,
    const ::DDS::LivelinessChangedStatus & /* status */)
  ACE_THROW_SPEC (( CORBA::SystemException))
{
  if( OpenDDS::DCPS::DCPS_debug_level > 0) {
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) FederatorLinkListener::on_liveliness_changed\n")));
  }
}

void FederatorLinkListener::on_subscription_match (
    ::DDS::DataReader_ptr /* reader */,
    const ::DDS::SubscriptionMatchStatus & /* status */)
  ACE_THROW_SPEC ((CORBA::SystemException))
{
  if( OpenDDS::DCPS::DCPS_debug_level > 0) {
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) FederatorLinkListener::on_subscription_match\n")));
  }
}

void FederatorLinkListener::on_sample_rejected(
    ::DDS::DataReader_ptr /* reader */,
   const DDS::SampleRejectedStatus& /* status */)
  ACE_THROW_SPEC ((CORBA::SystemException))
{
  if( OpenDDS::DCPS::DCPS_debug_level > 0) {
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) FederatorLinkListener::on_sample_rejected\n")));
  }
}

void FederatorLinkListener::on_sample_lost(::DDS::DataReader_ptr /* reader */,
                                  const DDS::SampleLostStatus& /* status */)
  ACE_THROW_SPEC ((CORBA::SystemException))
{
  if( OpenDDS::DCPS::DCPS_debug_level > 0) {
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) FederatorLinkListener::on_sample_lost\n")));
  }
}

}} // End of namespace OpenDDS::Federator

