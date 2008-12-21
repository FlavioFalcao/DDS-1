// -*- C++ -*-
//
// $Id$

// ****  Code generated by the The ACE ORB (TAO) IDL Compiler ****
// TAO and the TAO IDL Compiler have been developed by:
//       Center for Distributed Object Computing
//       Washington University
//       St. Louis, MO
//       USA
//       http://www.cs.wustl.edu/~schmidt/doc-center.html
// and
//       Distributed Object Computing Laboratory
//       University of California at Irvine
//       Irvine, CA
//       USA
//       http://doc.ece.uci.edu/
//
// Information about TAO is available at:
//     http://www.cs.wustl.edu/~schmidt/TAO.html

// TAO_IDL - Generated from
// .\be\be_codegen.cpp:951

#ifndef DDSDCPSTOPIC_LISTENER_I_H_
#define DDSDCPSTOPIC_LISTENER_I_H_

#include "dds/DdsDcpsTopicS.h"

#if !defined (ACE_LACKS_PRAGMA_ONCE)
#pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */


//Class OPENDDS_DCPS_TopicListener_i
class OPENDDS_DCPS_TopicListener_i
  : public virtual OpenDDS::DCPS::LocalObject<DDS::TopicListener>
{
public:
  //Constructor
  OPENDDS_DCPS_TopicListener_i (void);

  //Destructor
  virtual ~OPENDDS_DCPS_TopicListener_i (void);



virtual void on_inconsistent_topic (
    ::DDS::Topic_ptr the_topic,
    const ::DDS::InconsistentTopicStatus & status
  )
  ACE_THROW_SPEC ((
    CORBA::SystemException
  ));



  ::DDS::InconsistentTopicStatus last_status_;

};


#endif /* DDSDCPSTOPIC_LISTENER_I_H_  */
