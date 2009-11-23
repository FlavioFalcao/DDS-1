/*
 * $Id$
 *
 * Copyright 2009 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef DCPS_DISJOINTSEQUENCE_H
#define DCPS_DISJOINTSEQUENCE_H

#include "Definitions.h"
#include "dcps_export.h"

#include <set>

namespace OpenDDS {
namespace DCPS {

class OpenDDS_Dcps_Export DisjointSequence {
public:
  typedef std::pair<SequenceNumber, SequenceNumber> RangePair;
  typedef std::set<RangePair> RangeSet;

  explicit DisjointSequence(SequenceNumber value = SequenceNumber());

  SequenceNumber low() const;
  SequenceNumber high() const;

  size_t depth() const;
  bool disjoint() const;

  bool range(RangeSet& values, size_t max_interval);

  bool update(SequenceNumber value);
  void skip(SequenceNumber value);

  operator SequenceNumber() const;

private:
  typedef std::set<SequenceNumber> SequenceSet;
  SequenceSet values_;

  void normalize();
};

} // namespace DCPS
} // namespace OpenDDS

#ifdef __ACE_INLINE__
# include "DisjointSequence.inl"
#endif /* __ACE_INLINE__ */

#endif  /* DCPS_DISJOINTSEQUENCE_H */
