/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef OPENDDS_TCPTRANSPORT_RCH_H
#define OPENDDS_TCPTRANSPORT_RCH_H

#include "dds/DCPS/RcHandle_T.h"

namespace OpenDDS {
namespace DCPS {

class TcpTransport;

typedef RcHandle<TcpTransport> TcpTransport_rch;

} // namespace DCPS
} // namespace OpenDDS

#endif /* OPENDDS_TCPTRANSPORT_RCH_H */
