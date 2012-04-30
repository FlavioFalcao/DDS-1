/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef DCPS_RECEIVEDDATASTRATEGY_H
#define DCPS_RECEIVEDDATASTRATEGY_H

#include "CoherentChangeControl.h"

namespace OpenDDS {
namespace DCPS {

class ReceivedDataElementList;
class ReceivedDataElement;

class OpenDDS_Dcps_Export ReceivedDataStrategy {
public:
  explicit ReceivedDataStrategy(ReceivedDataElementList& rcvd_samples);
  virtual ~ReceivedDataStrategy();

  virtual void add(ReceivedDataElement* data_sample);

#ifndef OPENDDS_NO_PRESENTATION_QOS
  virtual void accept_coherent(PublicationId& writer,
                               RepoId& publisher);

  virtual void reject_coherent(PublicationId& writer,
                               RepoId& publisher);
#endif

protected:
  ReceivedDataElementList& rcvd_samples_;
};

class OpenDDS_Dcps_Export ReceptionDataStrategy
  : public ReceivedDataStrategy {
public:
  explicit ReceptionDataStrategy(ReceivedDataElementList& rcvd_samples);

  ~ReceptionDataStrategy();
};

class OpenDDS_Dcps_Export SourceDataStrategy
  : public ReceivedDataStrategy {
public:
  explicit SourceDataStrategy(ReceivedDataElementList& rcvd_samples);

  ~SourceDataStrategy();

  virtual void add(ReceivedDataElement* data_sample);
};

} // namespace DCPS
} // namespace OpenDDS

#endif /* DCPS_RECEIVEDDATASTRATEGY_H */
