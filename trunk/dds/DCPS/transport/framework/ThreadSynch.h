// -*- C++ -*-
//
// $Id$
#ifndef TAO_DCPS_THREADSYNCH_H
#define TAO_DCPS_THREADSYNCH_H

#include  "dds/DCPS/dcps_export.h"
#include  "ThreadSynchWorker.h"


namespace TAO
{
  namespace DCPS
  {

    class ThreadSynchResource;


    class TAO_DdsDcps_Export ThreadSynch
    {
      public:

        virtual ~ThreadSynch();

        /// The worker must introduce himself to this ThreadSynch object.
        /// It is the worker object that "owns" this ThreadSynch object.
        /// Returns 0 for success, -1 for failure.
        int register_worker(ThreadSynchWorker* worker);

        /// Our owner, the worker_, is breaking our relationship.
        void unregister_worker();

        /// The ThreadSynchWorker would like to have its perform_work() called
        /// from the appropriate thread once the ThreadSynchResource claims
        /// that it is_ready_for_work().
        virtual void work_available() = 0;


      protected:

        // This ThreadSynch object takes ownership of the resource.
        ThreadSynch(ThreadSynchResource* resource);

        /// This will tell the worker_ to perform_work().
        /// A return value of 0 means that the perform_work() did all the
        /// work necessary, and we shouldn't ask it to perform_work() again,
        /// unless we receive another work_available() call.  It won't hurt
        /// if we call perform_work() and it has nothing to do - it will
        /// immediately return 0 to indicate it has nothing more to do (at
        /// the moment).
        /// A return value of 1 means that the perform_work() completed, and
        /// there is still more work it could do.  The perform_work() should
        /// be called again in this case.
        ThreadSynchWorker::WorkOutcome perform_work();

        void wait_on_clogged_resource();

        /// The default implementation is to do nothing here.  The
        /// subclass may override the implementation in order to do
        /// something when the worker registers.
        /// Returns 0 for success, -1 for failure.
        virtual int register_worker_i();

        /// The default implementation is to do nothing here.  The
        /// subclass may override the implementation in order to do
        /// something when the worker unregisters.
        virtual void unregister_worker_i();


      private:

        ThreadSynchWorker* worker_;
        ThreadSynchResource* resource_;
    };

  } /* namespace DCPS */
} /* namespace TAO */

#if defined (__ACE_INLINE__)
#include "ThreadSynch.inl"
#endif /* __ACE_INLINE__ */

#endif  /* TAO_DCPS_THREADSYNCH_H */
