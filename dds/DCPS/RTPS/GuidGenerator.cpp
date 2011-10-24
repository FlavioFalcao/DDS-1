// -*- C++ -*-
/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "dds/DCPS/RTPS/GuidGenerator.h"
#include "dds/DdsDcpsGuidTypeSupportImpl.h"
#include "ace/OS_NS_unistd.h"
#include "ace/OS_NS_stdlib.h"
#include "ace/OS_NS_netdb.h"

namespace OpenDDS {
  namespace RTPS {

GuidGenerator::GuidGenerator (void)
  : pid_ (ACE_OS::getpid()),
    counter_(0)
{
  ACE_OS::macaddr_node_t macaddress;
  int const result = ACE_OS::getmacaddress(&macaddress);

  if (-1 != result) {
    ACE_OS::memcpy(node_id_, macaddress.node, NODE_ID_SIZE);
  }
  else {
    node_id_[0] = static_cast<u_char>(ACE_OS::rand());
    node_id_[1] = static_cast<u_char>(ACE_OS::rand());
    node_id_[2] = static_cast<u_char>(ACE_OS::rand());
    node_id_[3] = static_cast<u_char>(ACE_OS::rand());
    node_id_[4] = static_cast<u_char>(ACE_OS::rand());
    node_id_[5] = static_cast<u_char>(ACE_OS::rand());
  }
}

uint16_t
GuidGenerator::getCount()
{
  ACE_Guard<ACE_SYNCH_MUTEX> guard(this->counter_lock_);
  return counter_++;
}

void
GuidGenerator::populate (DCPS::GUID_t &container)
{
  container.guidPrefix[0] = DCPS::VENDORID_OCI[0];
  container.guidPrefix[1] = DCPS::VENDORID_OCI[1];

  uint16_t count = this->getCount();
  ACE_OS::memcpy(&container.guidPrefix[2],node_id_,NODE_ID_SIZE);
  container.guidPrefix[8] = static_cast<uint8_t>(this->pid_ >> 8);
  container.guidPrefix[9] = static_cast<uint8_t>(this->pid_ & 0xFF);
  container.guidPrefix[10] = static_cast<uint8_t>(count >> 8);
  container.guidPrefix[11] = static_cast<uint8_t>(count & 0xFF);
}

} // namespace RTPS
} // namespace OpenDDS
