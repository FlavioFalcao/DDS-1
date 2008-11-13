// -*- C++ -*-
// $Id$

#include "DataReaderListener.h"
#include "TestTypeSupportImpl.h"

#include <dds/DCPS/Service_Participant.h>
#include <dds/DCPS/Marked_Default_Qos.h>
#include <dds/DCPS/SubscriberImpl.h>
#include <dds/DCPS/transport/framework/TheTransportFactory.h>
#include <dds/DCPS/transport/simpleTCP/SimpleTcpConfiguration.h>

#ifdef ACE_AS_STATIC_LIBS
#include <dds/DCPS/transport/simpleTCP/SimpleTcp.h>
#endif

int
main( int /* argc */, char /* *argv[] */)
{
  try {
    ACE_OS::exit( 1); // subscriptions go here.

  } catch( CORBA::Exception& e) {
    return 1;
  }

  return 0;
}
