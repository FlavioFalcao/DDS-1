/*
 * $Id$
 *
 * Copyright 2010 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef RAKEDATA_H
#define RAKEDATA_H

#include /**/ "ace/pre.h"
#include "dcps_export.h"
#include "ReceivedDataElementList.h"
#include "SubscriptionInstance.h"

#if !defined (ACE_LACKS_PRAGMA_ONCE)
# pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */

namespace OpenDDS {
namespace DCPS {

//class ReceivedDataElement;
//class SubscriptionInstance;

/// Rake is an abbreviation for "read or take".  This struct holds the
/// data used by the data structures in RakeResults<T>.
struct OpenDDS_Dcps_Export RakeData {
  ReceivedDataElement* rde_;
  SubscriptionInstance* si_;
  size_t index_in_instance_;
};

} // namespace DCPS
} // namespace OpenDDS

#endif
