// -*- C++ -*-
//
// $Id$
#ifndef TAO_DCPS_RECEIVELISTENERSETMAP_H
#define TAO_DCPS_RECEIVELISTENERSETMAP_H

#include "dds/DCPS/dcps_export.h"
#include "ReceiveListenerSet.h"
#include "ReceiveListenerSet_rch.h"
#include "dds/DCPS/Definitions.h"
#include "ace/Synch.h"

#include <map>

namespace TAO
{

  namespace DCPS
  {

    class TransportInterface;
    class TransportReceiveListener;
    class ReceivedDataSample;


    class TAO_DdsDcps_Export ReceiveListenerSetMap
    {
      public:
       
        typedef std::map<RepoId, ReceiveListenerSet_rch> MapType;

        ReceiveListenerSetMap();
        virtual ~ReceiveListenerSetMap();

        int insert(RepoId                    publisher_id,
                   RepoId                    subscriber_id,
                   TransportReceiveListener* receive_listener);

        ReceiveListenerSet* find(RepoId publisher_id);

        int remove(RepoId publisher_id, RepoId subscriber_id);

        /// This method is called when the (remote) subscriber is being
        /// released.  This method will return a 0 if the subscriber_id is
        /// successfully disassociated with the publisher_id *and* there
        /// are still other subscribers associated with the publisher_id.
        /// This method will return 1 if, after the disassociation, the
        /// publisher_id is no longer associated with any subscribers (which
        /// also means it's element was removed from our map_).
        int release_subscriber(RepoId publisher_id, RepoId subscriber_id);

        ReceiveListenerSet* remove_set(RepoId publisher_id);

        ssize_t size() const;

        /// Only called by the DataLink.
        int data_received(ReceivedDataSample& sample);

        /// Give access to the underlying map for iteration purposes.
        MapType& map();
        const MapType& map() const;


      private:

        ReceiveListenerSet* find_or_create(RepoId publisher_id);

        MapType  map_;
    };

  }  /* namespace DCPS */

}  /* namespace TAO */

#if defined (__ACE_INLINE__)
#include "ReceiveListenerSetMap.inl"
#endif /* __ACE_INLINE__ */

#endif /* TAO_DCPS_RECEIVELISTENERSETMAP_H */
