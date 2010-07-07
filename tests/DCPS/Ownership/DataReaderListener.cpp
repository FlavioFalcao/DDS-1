/*
 * $Id$
 *
 * Copyright 2010 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include <ace/Log_Msg.h>
#include <ace/OS_NS_stdlib.h>

#include <dds/DdsDcpsSubscriptionC.h>
#include <dds/DCPS/Service_Participant.h>

#include "DataReaderListener.h"
#include "MessengerTypeSupportC.h"
#include "MessengerTypeSupportImpl.h"

#include <iostream>

extern int testcase;

DataReaderListenerImpl::DataReaderListenerImpl(const char* reader_id)
  : num_reads_(0), reader_id_ (reader_id), verify_result_ (true)
{
  this->current_strength_[0] = 0;
  this->current_strength_[1] = 0;
}

DataReaderListenerImpl::~DataReaderListenerImpl()
{
}

void DataReaderListenerImpl::on_data_available(DDS::DataReader_ptr reader)
throw(CORBA::SystemException)
{
  num_reads_ ++;
  
  try {
    Messenger::MessageDataReader_var message_dr =
      Messenger::MessageDataReader::_narrow(reader);

    if (CORBA::is_nil(message_dr.in())) {
      ACE_ERROR((LM_ERROR,
                 ACE_TEXT("%N:%l: on_data_available()")
                 ACE_TEXT(" ERROR: _narrow failed!\n")));
      ACE_OS::exit(-1);
    }

    Messenger::Message message;
    DDS::SampleInfo si;

    DDS::ReturnCode_t status = message_dr->take_next_sample(message, si) ;

    if (status == DDS::RETCODE_OK) {
      if (si.valid_data) {
        ACE_DEBUG ((LM_DEBUG, ACE_TEXT("(%P|%t)%X %s->%s subject_id: %d ")
          ACE_TEXT("count: %d strength: %d\n"),
          this, message.from.in(), this->reader_id_, message.subject_id, 
          message.count, message.strength)); 
        std::cout << message.from.in() << "->" << this->reader_id_ 
        << " subject_id: " << message.subject_id  
        << " count: " << message.count 
        << " strength: " << message.strength 
        << std::endl;
        bool result = verify (message);
        this->verify_result_ = result ? this->verify_result_ : false;
        
      } else if (si.instance_state == DDS::NOT_ALIVE_DISPOSED_INSTANCE_STATE) {
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("%N:%l: INFO: instance is disposed\n")));

      } else if (si.instance_state == DDS::NOT_ALIVE_NO_WRITERS_INSTANCE_STATE) {
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("%N:%l: INFO: instance is unregistered\n")));

      } else {
        ACE_ERROR((LM_ERROR,
                   ACE_TEXT("%N:%l: on_data_available()")
                   ACE_TEXT(" ERROR: unknown instance state: %d\n"),
                   si.instance_state));
      }

    } else {
      ACE_ERROR((LM_ERROR,
                 ACE_TEXT("%N:%l: on_data_available()")
                 ACE_TEXT(" ERROR: unexpected status: %d\n"),
                 status));
    }

  } catch (const CORBA::Exception& e) {
    e._tao_print_exception("Exception caught in on_data_available():");
    ACE_OS::exit(-1);
  }
}

void DataReaderListenerImpl::on_requested_deadline_missed(
  DDS::DataReader_ptr,
  const DDS::RequestedDeadlineMissedStatus & status)
throw(CORBA::SystemException)
{
  ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t)%X: on_requested_deadline_missed(): ")
          ACE_TEXT(" handle %d total_count_change %d \n"),
          this, status.last_instance_handle, status.total_count_change));
}

void DataReaderListenerImpl::on_requested_incompatible_qos(
  DDS::DataReader_ptr,
  const DDS::RequestedIncompatibleQosStatus &)
throw(CORBA::SystemException)
{
  ACE_DEBUG((LM_DEBUG, ACE_TEXT("%N:%l: INFO: on_requested_incompatible_qos()\n")));
}

void DataReaderListenerImpl::on_liveliness_changed(
  DDS::DataReader_ptr,
  const DDS::LivelinessChangedStatus & status)
throw(CORBA::SystemException)
{
  ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t)%X: on_liveliness_changed(): ")
          ACE_TEXT(" handle %d alive_count_change %d not_alive_count_change %d \n"),
          this, status.last_publication_handle, status.alive_count_change, 
          status.alive_count_change));
}

void DataReaderListenerImpl::on_subscription_matched(
  DDS::DataReader_ptr,
  const DDS::SubscriptionMatchedStatus &)
throw(CORBA::SystemException)
{
  ACE_DEBUG((LM_DEBUG, ACE_TEXT("%N:%l: INFO: on_subscription_matched()\n")));
}

void DataReaderListenerImpl::on_sample_rejected(
  DDS::DataReader_ptr,
  const DDS::SampleRejectedStatus&)
throw(CORBA::SystemException)
{
  ACE_DEBUG((LM_DEBUG, ACE_TEXT("%N:%l: INFO: on_sample_rejected()\n")));
}

void DataReaderListenerImpl::on_sample_lost(
  DDS::DataReader_ptr,
  const DDS::SampleLostStatus&)
throw(CORBA::SystemException)
{
  ACE_DEBUG((LM_DEBUG, ACE_TEXT("%N:%l: INFO: on_sample_lost()\n")));
}

bool
DataReaderListenerImpl::verify (const Messenger::Message& msg)
{
  if (msg.subject_id != msg.count % 2)
    return false;

  switch (testcase) {
  case strength:
  {
    // strength should be not be less then before  
    if (msg.strength < this->current_strength_[msg.subject_id]) {
      return false;
    } 
    // record the strength of writer that sample is from.
    this->current_strength_[msg.subject_id] = msg.strength;
  }
  break;
  case liveliness_change:
  case miss_deadline:
  {
    // record the strength of writer that sample is from.
    this->current_strength_[msg.subject_id] = msg.strength;
  }
  break;
  case update_strength:
  {
    // strength should be not be less then before  
    if (msg.strength < this->current_strength_[msg.subject_id]) {
      return false;
    } 
    // record the strength of writer that sample is from.
    this->current_strength_[msg.subject_id] = msg.strength;
  }
  break;
  default:
  ACE_OS::exit(1);
  break;
  }
  
  return true;
}


bool 
DataReaderListenerImpl::verify_result ()  
{
  switch (testcase) {
  case strength:
  {
    this->verify_result_ 
      = this->verify_result_ 
        && this->current_strength_[0] == this->current_strength_[1]
        && this->current_strength_[0] == 12;
  }
  break;
  case liveliness_change:
  case miss_deadline:
  {
    // The liveliness is changed for both writers in the middle of sending
    // total messages but finally, the higher strength writer takes ownership.
    this->verify_result_ 
      = this->verify_result_ 
        && this->current_strength_[0] == this->current_strength_[1]
        && this->current_strength_[0] == 12;
  }
  break;
  case update_strength:
  {
    this->verify_result_ 
      = this->verify_result_ 
        && this->current_strength_[0] == this->current_strength_[1]
        && this->current_strength_[0] == 15;
  }
  break;
  default:
  ACE_OS::exit(1);
  break;
  }
  return this->verify_result_;
}
