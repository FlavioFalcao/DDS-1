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

#ifndef DCPSDATAREADERI_H_
#define DCPSDATAREADERI_H_

#include "dds/DdsDcpsDataReaderRemoteS.h"
#include "dds/DCPS/Definitions.h"

#if !defined (ACE_LACKS_PRAGMA_ONCE)
#pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */

//Class TAO_DDS_DCPSDataReader_i
class TAO_DDS_DCPSDataReader_i   
  : public virtual POA_OpenDDS::DCPS::DataReaderRemote
{
public:
  //Constructor
  TAO_DDS_DCPSDataReader_i (void);

  //Destructor
  virtual ~TAO_DDS_DCPSDataReader_i (void);


  virtual ::DDS::ReturnCode_t enable_specific (
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
        )) { return ::DDS::RETCODE_OK;};


  virtual void add_associations (
      const ::OpenDDS::DCPS::RepoId& yourId,
      const OpenDDS::DCPS::WriterAssociationSeq & writers
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ));

  virtual void remove_associations (
      const OpenDDS::DCPS::WriterIdSeq & writers,
      ::CORBA::Boolean notify_lost
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ));

  virtual void update_incompatible_qos (
      const OpenDDS::DCPS::IncompatibleQosStatus & status
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ));
};


#endif /* DCPSDATAREADERI_H_  */
