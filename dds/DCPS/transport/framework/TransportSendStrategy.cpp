/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "DCPS/DdsDcps_pch.h" //Only the _pch include should start with DCPS/
#include "TransportSendStrategy.h"
#include "RemoveAllVisitor.h"
#include "TransportInst.h"
#include "ThreadSynchStrategy.h"
#include "ThreadSynchResource.h"
#include "TransportQueueElement.h"
#include "TransportSendElement.h"
#include "TransportSendBuffer.h"
#include "BuildChainVisitor.h"
#include "QueueRemoveVisitor.h"
#include "PacketRemoveVisitor.h"
#include "TransportDefs.h"
#include "DirectPriorityMapper.h"
#include "dds/DCPS/DataSampleHeader.h"
#include "dds/DCPS/DataSampleList.h"
#include "dds/DCPS/Service_Participant.h"
#include "EntryExit.h"

#include "ace/Reverse_Lock_T.h"

#include <vector>

#if !defined (__ACE_INLINE__)
#include "TransportSendStrategy.inl"
#endif /* __ACE_INLINE__ */

namespace OpenDDS {
namespace DCPS {

//TBD: The number of chunks of the replace element allocator
//     is hard coded for now. This will be configurable when
//     we implement the dds configurations. This value should
//     be the number of marshalled DataSampleHeader that a
//     packet could contain.
#define NUM_REPLACED_ELEMENT_CHUNKS 20

namespace {
  /// Arbitrary small constant that represents the minimum
  /// amount of payload data we'll have in one fragment.
  /// In this case "payload data" includes the content-filtering
  /// GUID sequence, so this is chosen to be 4 + (16 * N).
  static const size_t MIN_FRAG = 68;
}

// I think 2 chunks for the header message block is enough
// - one for the original copy and one for duplicate which
// occurs every packet and is released after packet is sent.
// The data block only needs 1 chunk since the duplicate()
// just increases the ref count.
TransportSendStrategy::TransportSendStrategy(
  std::size_t id,
  const TransportInst_rch& transport_inst,
  ThreadSynchResource* synch_resource,
  CORBA::Long priority,
  const ThreadSynchStrategy_rch& thread_sync_strategy)
  : ThreadSynchWorker(id),
    max_samples_(transport_inst->max_samples_per_packet_),
    optimum_size_(transport_inst->optimum_packet_size_),
    max_size_(transport_inst->max_packet_size_),
    queue_(new QueueType(transport_inst->queue_messages_per_pool_,
                         transport_inst->queue_initial_pools_)),
    max_header_size_(0),
    header_block_(0),
    elems_(new QueueType(1, transport_inst->max_samples_per_packet_)),
    pkt_chain_(0),
    header_complete_(false),
    start_counter_(0),
    mode_(MODE_DIRECT),
    mode_before_suspend_(MODE_NOT_SET),
    delayed_delivered_notification_queue_(0),
    delayed_notification_mode_(0),
    num_delayed_notifications_(0),
    header_mb_allocator_(0),
    header_db_allocator_(0),
    synch_(0),
    lock_(),
    replaced_element_allocator_(NUM_REPLACED_ELEMENT_CHUNKS),
    replaced_element_mb_allocator_(NUM_REPLACED_ELEMENT_CHUNKS * 2),
    replaced_element_db_allocator_(NUM_REPLACED_ELEMENT_CHUNKS * 2),
    retained_element_allocator_(0),
    transport_inst_(transport_inst),
    graceful_disconnecting_(false),
    link_released_(true),
    send_buffer_(0),
    transport_shutdown_(false)
{
  DBG_ENTRY_LVL("TransportSendStrategy","TransportSendStrategy",6);

  // Create a ThreadSynch object just for us.
  DirectPriorityMapper mapper(priority);
  this->synch_ = thread_sync_strategy->create_synch_object(
                   synch_resource,
#ifdef ACE_WIN32
                   ACE_DEFAULT_THREAD_PRIORITY,
#else
                   mapper.thread_priority(),
#endif
                   TheServiceParticipant->scheduler());

  // We cache this value in data member since it doesn't change, and we
  // don't want to keep asking for it over and over.
  this->max_header_size_ = TransportHeader::max_marshaled_size();

  if (Transport_debug_level >= 2) {
    ACE_DEBUG((LM_DEBUG, "(%P|%t) TransportSendStrategy replaced_element_allocator %x with %d chunks\n",
               &replaced_element_allocator_, NUM_REPLACED_ELEMENT_CHUNKS));
  }

  this->delayed_delivered_notification_queue_ =
    new TransportQueueElement*[max_samples_];
  this->delayed_notification_mode_ = new SendMode[max_samples_];
}

TransportSendStrategy::~TransportSendStrategy()
{
  DBG_ENTRY_LVL("TransportSendStrategy","~TransportSendStrategy",6);

  delete this->synch_;

  if (this->delayed_delivered_notification_queue_) {
    delete [] this->delayed_delivered_notification_queue_;
    this->delayed_delivered_notification_queue_ = 0;
  }

  if (this->delayed_notification_mode_) {
    delete [] this->delayed_notification_mode_;
    this->delayed_notification_mode_ = 0;
  }

  delete this->elems_;
  delete this->queue_;
}

void
TransportSendStrategy::send_buffer(TransportSendBuffer* send_buffer)
{
  this->send_buffer_ = send_buffer;

  if (this->send_buffer_ != 0) {
    this->send_buffer_->bind(this);
  }
}

ThreadSynchWorker::WorkOutcome
TransportSendStrategy::perform_work()
{
  DBG_ENTRY_LVL("TransportSendStrategy","perform_work",6);

  SendPacketOutcome outcome;
  bool no_more_work = false;

  { // scope for the guard(this->lock_);
    GuardType guard(this->lock_);

    VDBG_LVL((LM_DEBUG, "(%P|%t) DBG: perform_work mode: %C\n", mode_as_str(this->mode_)), 5);

    if (this->mode_ == MODE_TERMINATED) {
      VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
                "Entered perform_work() and mode_ is MODE_TERMINATED - "
                "we lost connection and could not reconnect, just return "
                "WORK_OUTCOME_BROKEN_RESOURCE.\n"), 5);
      return WORK_OUTCOME_BROKEN_RESOURCE;
    }

    // The perform_work() is called by our synch_ object using
    // a thread designated to call this method when it thinks
    // we need to be called in order to "service" the queue_ and/or
    // deal with a partially sent current packet.
    //
    // We will return a 0 if we don't see a need to have our perform_work()
    // called again, and we will return a 1 if we do see the need to have our
    // perform_work() method called again.

    // First, make sure that the mode_ indicates that we are, indeed, in
    // the MODE_QUEUE mode.  If we are not in MODE_QUEUE mode (meaning we are
    // in MODE_DIRECT), then it means we didn't need to have this perform_work()
    // method called - in this case, do nothing other than return
    // WORK_OUTCOME_NO_MORE_TO_DO to tell our caller that we really don't
    // see a need for it to call our perform_work() again (at least not
    // right now).
    if (this->mode_ != MODE_QUEUE && this->mode_ != MODE_SUSPEND) {
      VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
                "Entered perform_work() and mode_ is %C - just return "
                "WORK_OUTCOME_NO_MORE_TO_DO.\n", mode_as_str(this->mode_)), 5);
      return WORK_OUTCOME_NO_MORE_TO_DO;
    }

    // Check the "state" of the current packet.  We will either find that the
    // current packet is in a state of being "partially sent", or we will find
    // it in a state of being "empty".  When the current packet is "empty", it
    // means that it is time to build up the current packet using elements
    // extracted from the queue_, and then we will attempt to send the
    // packet.  When we find the current packet in the "partially sent" state,
    // we will not touch the queue_ - we will just try to send the unsent
    // bytes in the current (partially sent) packet.
    const size_t header_length = this->header_.length_;

    if (header_length == 0) {
      VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
                "The current packet doesn't have any unsent bytes - we "
                "need to 'populate' the current packet with elems from "
                "the queue.\n"), 5);

      // The current packet is "empty".  Build up the current packet using
      // elements from the queue_, and prepare the current packet to be sent.

      // Before we build the packet from the queue_, let's make sure that
      // there is actually something on the queue_ to build from.
      if (this->queue_->size() == 0) {
        VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
                  "But the queue is empty.  We have cleared the "
                  "backpressure situation.\n"),5);
        // We are here because the queue_ is empty, and there isn't
        // any "partial packet" bytes left to send.  We have overcome
        // the backpressure situation and don't have anything to do
        // right now.

        VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
                  "Flip mode to MODE_DIRECT, and return "
                  "WORK_OUTCOME_NO_MORE_TO_DO.\n"), 5);

        // Flip the mode back to MODE_DIRECT.
        this->mode_ = MODE_DIRECT;

        // And return WORK_OUTCOME_NO_MORE_TO_DO to tell our caller that
        // perform_work() doesn't need to be called again (at this time).
        return WORK_OUTCOME_NO_MORE_TO_DO;
      }

      VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
                "There is at least one elem in the queue - get the packet "
                "elems from the queue.\n"), 5);

      // There is stuff in the queue_ if we get to this point in the logic.
      // Build-up the current packet using element(s) from the queue_.
      this->get_packet_elems_from_queue();

      VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
                "Prepare the packet from the packet elems_.\n"), 5);

      // Now we can prepare the new packet to be sent.
      this->prepare_packet();

      VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
                "Packet has been prepared from packet elems_.\n"), 5);

    } else {
      VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
                "We have a current packet that still has unsent bytes.\n"), 5);
    }

    VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
              "Attempt to send the current packet.\n"), 5);

    // Now we can attempt to send the current packet - whether it is
    // a "partially sent" packet or one that we just built-up using elements
    // from the queue_ (and subsequently prepared for sending) - it doesn't
    // matter.  Just attempt to send as many of the "unsent" bytes in the
    // packet as possible.
    outcome = this->send_packet(DELAY_NOTIFICATION);

    // If we sent the whole packet (eg, partial_send is false), and the queue_
    // is now empty, then we've cleared the backpressure situation.
    if ((outcome == OUTCOME_COMPLETE_SEND) && (this->queue_->size() == 0)) {
      VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
                "Flip the mode to MODE_DIRECT, and then return "
                "WORK_OUTCOME_NO_MORE_TO_DO.\n"), 5);

      // Revert back to MODE_DIRECT mode.
      this->mode_ = MODE_DIRECT;
      no_more_work = true;
    }
  } // End of scope for guard(this->lock_);

  VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
            "The outcome of the send_packet() was %d.\n", outcome), 5);

  send_delayed_notifications();

  // If we sent the whole packet (eg, partial_send is false), and the queue_
  // is now empty, then we've cleared the backpressure situation.
  if (no_more_work) {
    VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
              "We sent the whole packet, and there is nothing left on "
              "the queue now.\n"), 5);

    // Return WORK_OUTCOME_NO_MORE_TO_DO to tell our caller that we
    // don't desire another call to this perform_work() method.
    return WORK_OUTCOME_NO_MORE_TO_DO;
  }

  VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
            "We still have unsent bytes in the current packet AND/OR there "
            "are still elements in the queue.\n"), 5);

  if ((outcome == OUTCOME_PEER_LOST) || (outcome == OUTCOME_SEND_ERROR)) {
    VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
              "We lost our connection, or had some fatal connection "
              "error.  Return WORK_OUTCOME_BROKEN_RESOURCE.\n"), 5);

    VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
              "Now flip to MODE_SUSPEND before we try to reconnect.\n"), 5);

    bool do_suspend = true;
    this->relink(do_suspend);

    if (this->mode_ == MODE_SUSPEND) {
      VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
                "The reconnect has not done yet and we are still in MODE_SUSPEND. "
                "Return WORK_OUTCOME_CLOGGED_RESOURCE.\n"), 5);
      // Return WORK_OUTCOME_NO_MORE_TO_DO to tell our caller that we
      // don't desire another call to this perform_work() method.
      return WORK_OUTCOME_NO_MORE_TO_DO;

    } else if (this->mode_ == MODE_TERMINATED) {
      VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
                "Reconnect failed, now we are in MODE_TERMINATED\n"), 5);
      return WORK_OUTCOME_BROKEN_RESOURCE;

    } else {
      VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
                "Reconnect succeeded, Notify synch thread of work "
                "availablity.\n"), 5);
      // If the datalink is re-established then notify the synch
      // thread to perform work.  We do not hold the object lock at
      // this point.
      this->synch_->work_available();
    }
  }

  VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
            "We still have an 'unbroken' connection.\n"), 5);

  if (outcome == OUTCOME_BACKPRESSURE) {
    VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
              "We experienced backpressure on our attempt to send the "
              "packet.  Return WORK_OUTCOME_CLOGGED_RESOURCE.\n"), 5);
    // We have a "clogged resource".
    return WORK_OUTCOME_CLOGGED_RESOURCE;
  }

  VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
            "We may have sent the whole current packet, but still have "
            "elements on the queue.\n"), 5);
  VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
            "Or, we may have only partially sent the current packet.\n"), 5);

  VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
            "Either way, we return WORK_OUTCOME_MORE_TO_DO now.\n"), 5);

  // We may have had an OUTCOME_COMPLETE_SEND, but there is still stuff
  // in the queue_ to be sent.  *OR* we have have had an OUTCOME_PARTIAL_SEND,
  // which equates to the same thing - we still have work to do.

  // We are still in MODE_QUEUE mode, thus there is still work to be
  // done to service the queue_ and/or a partially sent current packet.
  // Return WORK_OUTCOME_MORE_TO_DO so that our caller knows that we still
  // want it to call this perform_work() method.
  return WORK_OUTCOME_MORE_TO_DO;
}

// Now we need to "peel off" those message blocks that were fully
// sent, adjust the first message block with an unsent byte to have
// its rd_ptr() pointing to that first unsent byte, and set the
// pkt_chain_ to that first message block with an unsent byte.
// As we "peel off" fully sent message blocks, we need to also deal with
// fully sent elements by removing them from the elems_ and
// calling their data_delivered() method.  In addition, as we peel off
// the message blocks that are fully sent, we need to untie them from
// the chain and release them.
// And finally, don't forget to adjust the header_.length_ to
// account for the num_bytes_sent (beware that some of the num_bytes_sent
// may be packet header bytes and shouldn't affect the header_.length_
// which doesn't include the packet header bytes.
int
TransportSendStrategy::adjust_packet_after_send(ssize_t num_bytes_sent,
  UseDelayedNotification delay_notification)
{
  DBG_ENTRY_LVL("TransportSendStrategy", "adjust_packet_after_send", 6);

  //Used to track elements that will need to be notified after releasing lock
  std::vector<TransportQueueElement*> elems_to_notify;

  VDBG((LM_DEBUG, "(%P|%t) DBG:   "
        "Adjusting the current packet because %d bytes of the packet "
        "have been sent.\n", num_bytes_sent));

  ssize_t num_bytes_left = num_bytes_sent;
  ssize_t num_non_header_bytes_sent = 0;

  VDBG((LM_DEBUG, "(%P|%t) DBG:   "
        "Set num_bytes_left to %d.\n", num_bytes_left));
  VDBG((LM_DEBUG, "(%P|%t) DBG:   "
        "Set num_non_header_bytes_sent to %d.\n",
        num_non_header_bytes_sent));

  VDBG((LM_DEBUG, "(%P|%t) DBG:   "
        "Peek at the element at the front of the packet elems_.\n"));

  // This is the element currently at the front of elems_.
  TransportQueueElement* element = this->elems_->peek();

  if(!element){
    ACE_DEBUG((LM_INFO, "(%P|%t) WARNING: adjust_packet_after_send skipping due to NULL element\n"));
  } else {
    VDBG((LM_DEBUG, "(%P|%t) DBG:   "
          "Use the element's msg() to find the last block in "
          "the msg() chain.\n"));

    // Get a pointer to the last message block in the element.
    const ACE_Message_Block* elem_tail_block = element->msg();

    VDBG((LM_DEBUG, "(%P|%t) DBG:   "
          "Start with tail block == element->msg().\n"));

    while (elem_tail_block->cont() != 0) {
      VDBG((LM_DEBUG, "(%P|%t) DBG:   "
            "Set tail block to its cont() block (next in chain).\n"));
      elem_tail_block = elem_tail_block->cont();
    }

    VDBG((LM_DEBUG, "(%P|%t) DBG:   "
          "Tail block now set (because tail block's cont() is 0).\n"));

    VDBG((LM_DEBUG, "(%P|%t) DBG:   "
          "Start the 'while (num_bytes_left > 0)' loop.\n"));

    while (num_bytes_left > 0) {
      VDBG((LM_DEBUG, "(%P|%t) DBG:   "
            "At top of 'num bytes left' loop.  num_bytes_left == [%d].\n",
            num_bytes_left));

      const int block_length = static_cast<int>(this->pkt_chain_->length());

      VDBG((LM_DEBUG, "(%P|%t) DBG:   "
            "Length of block at front of pkt_chain_ is [%d].\n",
            block_length));

      if (block_length <= num_bytes_left) {
        VDBG((LM_DEBUG, "(%P|%t) DBG:   "
              "The whole block at the front of pkt_chain_ was sent.\n"));

        // The entire message block at the front of the chain has been sent.
        // Detach the head message block from the chain and adjust
        // the pkt_chain_ to point to the next block (if any) in
        // the chain.
        VDBG((LM_DEBUG, "(%P|%t) DBG:   "
              "Extract the fully sent block from the pkt_chain_.\n"));

        ACE_Message_Block* fully_sent_block = this->pkt_chain_;

        VDBG((LM_DEBUG, "(%P|%t) DBG:   "
              "Set pkt_chain_ to pkt_chain_->cont().\n"));

        this->pkt_chain_ = this->pkt_chain_->cont();

        VDBG((LM_DEBUG, "(%P|%t) DBG:   "
              "Set the fully sent block's cont() to 0.\n"));

        fully_sent_block->cont(0);

        // Update the num_bytes_left to indicate that we have
        // processed the entire length of the block.
        num_bytes_left -= block_length;

        VDBG((LM_DEBUG, "(%P|%t) DBG:   "
              "Updated num_bytes_left to account for fully sent "
              "block (block_length == [%d]).\n", block_length));
        VDBG((LM_DEBUG, "(%P|%t) DBG:   "
              "Now, num_bytes_left == [%d].\n", num_bytes_left));

        if (!this->header_complete_) {
          VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                "Since the header_complete_ flag is false, it means "
                "that the packet header block was still in the "
                "pkt_chain_.\n"));

          VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                "Not anymore...  Set the header_complete_ flag "
                "to true.\n"));

          // That was the packet header block.  And now we know that it
          // has been completely sent.
          this->header_complete_ = true;

          VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                "Release the fully sent block.\n"));

          // Release the fully_sent_block
          fully_sent_block->release();

        } else {
          VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                "Since the header_complete_ flag is true, it means "
                "that the packet header block was not in the "
                "pkt_chain_.\n"));
          VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                "So, the fully sent block was part of an element.\n"));

          // That wasn't the packet header block.  It was from the
          // element currently at the front of the elems_
          // collection.  If it was the last block from the
          // element, then we need to extract the element from the
          // elems_ collection and invoke data_delivered() on it.
          num_non_header_bytes_sent += block_length;

          VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                "Updated num_non_header_bytes_sent to account for "
                "fully sent block (block_length == [%d]).\n",
                block_length));

          VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                "Now, num_non_header_bytes_sent == [%d].\n",
                num_non_header_bytes_sent));

          if (fully_sent_block->base() == elem_tail_block->base()) {
            VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                  "Ok.  The fully sent block was a duplicate of "
                  "the tail block of the element that is at the "
                  "front of the packet elems_.\n"));

            VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                  "This means that we have completely sent the "
                  "element at the front of the packet elems_.\n"));

            // This means that we have completely sent the element
            // that is currently at the front of the elems_ collection.

            VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                  "We can release the fully sent block now.\n"));

            // Release the fully_sent_block
            fully_sent_block->release();

            VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                  "We can extract the element from the front of "
                  "the packet elems_ (we were just peeking).\n"));

            // Extract the element from the elems_ collection
            element = this->elems_->get();

            VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                  "Tell the element that a decision has been made "
                  "regarding its fate - data_delivered().\n"));

            // Inform the element that the data has been delivered.

            if ((delay_notification == DELAY_NOTIFICATION) &&
                (this->num_delayed_notifications_ < this->max_samples_)) {
              // If we can delay notification.
              this->add_delayed_notification(element);

            } else {
              // later, inform the element that the data has been delivered.
              elems_to_notify.push_back(element);

//              // Ciju: If not, do a best effort immediate notification.
//              // I have noticed instances of immediate notification producing
//              // cores ocassionally. I believe the reason is
//              // if (delay_notification == NOTIFY_IMMEADIATELY)
//
//              // ciju: We need to release this->lock_ before
//              // calling data_dropped/data_delivered. Else we have a potential
//              // deadlock in our hands.
//              const SendMode mode = this->mode_;
//              ACE_Reverse_Lock<LockType> reverse(this->lock_);
//              ACE_Guard<ACE_Reverse_Lock<LockType> > reverseGuard(reverse);
//              if (mode == MODE_TERMINATED) {
//                element->data_dropped(true /*dropped_by_transport*/);
//
//              } else {
//                element->data_delivered();
//
//              }
            }

            VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                  "Peek at the next element in the packet "
                  "elems_.\n"));

            // Set up for the next element in elems_ by peek()'ing.
            element = this->elems_->peek();

            if (element != 0) {
              VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                    "The is an element still in the packet "
                    "elems_ (we are peeking at it now).\n"));

              VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                    "We are going to find the tail block for the "
                    "current element (we are peeking at).\n"));

              // There was a "next element".  Determine the
              // elem_tail_block for it.
              elem_tail_block = element->msg();

              VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                    "Start w/tail block == element->msg().\n"));

              while (elem_tail_block->cont() != 0) {
                VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                      "Set tail block to next in chain.\n"));
                elem_tail_block = elem_tail_block->cont();
              }

              VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                    "Done finding tail block.\n"));
            }

          } else {
            VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                  "Ok.  The fully sent block is *not* a "
                  "duplicate of the tail block of the element "
                  "at the front of the packet elems_.\n"));

            VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                  "Thus, we have not completely sent the "
                  "element yet.\n"));

            // We didn't completely send the element - it has more
            // message blocks that haven't been sent (that we know of).

            VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                  "We can release the fully_sent_block now.\n"));

            // Release the fully_sent_block
            fully_sent_block->release();
          }
        }

      } else {
        VDBG((LM_DEBUG, "(%P|%t) DBG:   "
              "Only part of the block at the front of pkt_chain_ "
              "was sent.\n"));

        VDBG((LM_DEBUG, "(%P|%t) DBG:   "
              "Advance the rd_ptr() of the front block (of pkt_chain_) "
              "by the num_bytes_left (%d).\n", num_bytes_left));

        // Only part of the current block was sent.
        this->pkt_chain_->rd_ptr(num_bytes_left);

        if (this->header_complete_) {
          VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                "And since the packet header block has already been "
                "completely sent, add num_bytes_left to the "
                "num_non_header_bytes_sent.\n"));

          VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                "Before, num_non_header_bytes_sent == %d.\n",
                num_non_header_bytes_sent));

          // We know that the current block isn't the packet header
          // block because the packet header block has already been
          // completely sent.  We need to count these bytes in the
          // num_non_header_bytes_sent.
          num_non_header_bytes_sent += num_bytes_left;

          VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                "After, num_non_header_bytes_sent == %d.\n",
                num_non_header_bytes_sent));
        }

        VDBG((LM_DEBUG, "(%P|%t) DBG:   "
              "Set the num_bytes_left to 0 now.\n"));

        num_bytes_left = 0;
      }
    }
  }

  VDBG((LM_DEBUG, "(%P|%t) DBG:   "
        "The 'num_bytes_left' loop has completed.\n"));

  VDBG((LM_DEBUG, "(%P|%t) DBG:   "
        "Adjust the header_.length_ to account for the "
        "num_non_header_bytes_sent.\n"));
  VDBG((LM_DEBUG, "(%P|%t) DBG:   "
        "Before, header_.length_ == %d.\n",
        this->header_.length_));

  // Adjust the packet header_.length_ to indicate how many non header
  // bytes are left to send.
  this->header_.length_ -= static_cast<ACE_UINT32>(num_non_header_bytes_sent);

  VDBG((LM_DEBUG, "(%P|%t) DBG:   "
        "After, header_.length_ == %d.\n",
        this->header_.length_));

  // Returns 0 if the entire packet was sent, and returns 1 otherwise.
  int rc = (this->header_.length_ == 0) ? 0 : 1;
  // Now notify the pending elements
  {
    std::vector<TransportQueueElement*>::iterator eiter;
    for (eiter = elems_to_notify.begin();
         eiter != elems_to_notify.end();
         ++eiter)
    {
      const SendMode mode = this->mode_;
      {
        ACE_Reverse_Lock<LockType> reverse(this->lock_);
        ACE_Guard<ACE_Reverse_Lock<LockType> > reverseGuard(reverse);
        if (mode == MODE_TERMINATED) {
          (*eiter)->data_dropped(true); // dropped_by_transport

        } else {
          (*eiter)->data_delivered();
        }
      }
    }
  }
  VDBG((LM_DEBUG, "(%P|%t) DBG:   "
        "Adjustments all done.  Returning [%d].  0 means entire packet "
        "has been sent.  1 means otherwise.\n",
        rc));

  return rc;
}

bool
TransportSendStrategy::send_delayed_notifications(const TransportQueueElement::MatchCriteria* match)
{
  DBG_ENTRY_LVL("TransportSendStrategy","send_delayed_notifications",6);

  TransportQueueElement* sample = 0;
  SendMode mode = MODE_NOT_SET;

  std::vector<TransportQueueElement*> samples;
  std::vector<SendMode> modes;

  size_t num_delayed_notifications = 0;
  bool found_element = false;

  {
    GuardType guard(lock_);

    num_delayed_notifications = num_delayed_notifications_;

    if (num_delayed_notifications == 0) {
      return false;

    } else if (num_delayed_notifications == 1) {
      // Optimization for the most common case (doesn't need vectors)

      if (!match || match->matches(*delayed_delivered_notification_queue_[0])) {
        found_element = true;
        sample = delayed_delivered_notification_queue_[0];
        mode = delayed_notification_mode_[0];

        delayed_delivered_notification_queue_[0] = 0;
        delayed_notification_mode_[0] = MODE_NOT_SET;
        num_delayed_notifications_ = 0;
      }

    } else {
      for (size_t i = 0; i < num_delayed_notifications; ++i) {
        sample = delayed_delivered_notification_queue_[i];
        if (!match || match->matches(*sample)) {
          found_element = true;
          samples.push_back(sample);
          modes.push_back(delayed_notification_mode_[i]);

          delayed_delivered_notification_queue_[i] = 0;
          delayed_notification_mode_[i] = MODE_NOT_SET;
        }
      }

      if (found_element) {
        for (size_t i = 0, gap = 0; i < num_delayed_notifications; ++i) {
          if (!delayed_delivered_notification_queue_[i]) {
            ++gap;
            --num_delayed_notifications_;
          } else if (gap) {
            delayed_delivered_notification_queue_[i - gap] =
              delayed_delivered_notification_queue_[i];
            delayed_notification_mode_[i - gap] = delayed_notification_mode_[i];
          }
        }
      }
    }
  }

  if (!found_element)
    return false;

  if (num_delayed_notifications == 1) {
    // optimization for the common case
    if (mode == MODE_TERMINATED) {
      if (!transport_shutdown_ || sample->owned_by_transport()) {
        sample->data_dropped(true);
      }
    } else {
      if (!transport_shutdown_ || sample->owned_by_transport()) {
        sample->data_delivered();
      }
    }

  } else {
    for (size_t i = 0; i < samples.size(); ++i) {
      if (modes[i] == MODE_TERMINATED) {
        if (!transport_shutdown_ || samples[i]->owned_by_transport()) {
          samples[i]->data_dropped(true);
        }
      } else {
        if (!transport_shutdown_ || samples[i]->owned_by_transport()) {
          samples[i]->data_delivered();
        }
      }
    }
  }
  return true;
}

/// Remove all samples in the backpressure queue and packet queue.
void
TransportSendStrategy::terminate_send(bool graceful_disconnecting)
{
  DBG_ENTRY_LVL("TransportSendStrategy","terminate_send",6);

  bool reset_flag = true;

  {
    GuardType guard(this->lock_);

    // If the terminate_send call due to a non-graceful disconnection before
    // a datalink shutdown then we will not try to send the graceful disconnect
    // message.
    if ((this->mode_ == MODE_TERMINATED || this->mode_ == MODE_SUSPEND)
        && !this->graceful_disconnecting_) {
      VDBG((LM_DEBUG, "(%P|%t) DBG:   "
            "It was already terminated non gracefully, will not set to graceful disconnecting \n"));
      reset_flag = false;
    }
  }

  VDBG((LM_DEBUG, "(%P|%t) DBG:  Now flip to MODE_TERMINATED \n"));

  this->clear(MODE_TERMINATED);

  if (reset_flag) {
    GuardType guard(this->lock_);
    this->graceful_disconnecting_ = graceful_disconnecting;
  }
}

void
TransportSendStrategy::clear(SendMode mode)
{
  DBG_ENTRY_LVL("TransportSendStrategy","clear",6);

  send_delayed_notifications();
  QueueType* elems = 0;
  QueueType* queue = 0;
  {
    GuardType guard(this->lock_);

    if (this->header_.length_ > 0) {
      // Clear the messages in the pkt_chain_ that is partially sent.
      // We just reuse these functions for normal partial send except actual sending.
      int num_bytes_left = static_cast<int>(this->pkt_chain_->total_length());
      int result = this->adjust_packet_after_send(num_bytes_left, DELAY_NOTIFICATION);

      if (result == 0) {
        VDBG((LM_DEBUG, "(%P|%t) DBG:   "
              "The adjustment logic says that the packet is cleared.\n"));

      } else {
        VDBG((LM_DEBUG, "(%P|%t) DBG:   "
              "The adjustment returned partial sent.\n"));
      }
    }

    elems = this->elems_;
    this->elems_ = new QueueType(1, this->transport_inst_->max_samples_per_packet_);
    queue = this->queue_;
    this->queue_ = new QueueType(this->transport_inst_->queue_messages_per_pool_,
                                 this->transport_inst_->queue_initial_pools_);

    this->header_.length_ = 0;
    this->pkt_chain_ = 0;
    this->header_complete_ = false;
    this->start_counter_ = 0;
    this->mode_ = mode;
    this->mode_before_suspend_ = MODE_NOT_SET;
  }

  // We need remove the queued elements outside the lock,
  // otherwise we have a deadlock situation when remove vistor
  // calls the data_droped on each dropped elements.

  // Clear all samples in queue.
  RemoveAllVisitor remove_all_visitor;

  elems->accept_remove_visitor(remove_all_visitor);
  queue->accept_remove_visitor(remove_all_visitor);

  delete elems;
  delete queue;
}

int
TransportSendStrategy::start()
{
  DBG_ENTRY_LVL("TransportSendStrategy","start",6);

  {
    GuardType guard(this->lock_);

    if (!this->start_i()) {
      return -1;
    }
  }

  size_t header_chunks(1);

  // If a secondary send buffer is bound, sent headers should
  // be cached to properly maintain the buffer:
  if (this->send_buffer_ != 0) {
    header_chunks += this->send_buffer_->capacity();

  } else {
    header_chunks += 1;
  }

  ACE_NEW_RETURN(this->header_db_allocator_,
                 TransportDataBlockAllocator(header_chunks),
                 -1);

  ACE_NEW_RETURN(this->header_mb_allocator_,
                 TransportMessageBlockAllocator(header_chunks),
                 -1);

  // Since we (the TransportSendStrategy object) are a reference-counted
  // object, but the synch_ object doesn't necessarily know this, we need
  // to give a "copy" of a reference to ourselves to the synch_ object here.
  // We will do the reverse when we unregister ourselves (as a worker) from
  // the synch_ object.
  //MJM: The synch thingie knows to not "delete" us, right?
  this->_add_ref();

  if (this->synch_->register_worker(this) == -1) {
    // Take back our "copy".
    this->_remove_ref();
    ACE_ERROR_RETURN((LM_ERROR,
                      "(%P|%t) ERROR: TransportSendStrategy failed to register "
                      "as a worker with the ThreadSynch object.\n"),
                     -1);
  }

  return 0;
}

void
TransportSendStrategy::stop()
{
  DBG_ENTRY_LVL("TransportSendStrategy","stop",6);

  if (this->header_block_ != 0) {
    this->header_block_->release ();
    this->header_block_ = 0;
  }

  this->synch_->unregister_worker();

  // Since we gave the synch_ a "copy" of a reference to ourselves, we need
  // to take it back now.
  this->_remove_ref();

  {
    GuardType guard(this->lock_);

    if (this->pkt_chain_ != 0) {
      size_t size = this->pkt_chain_->total_length();

      if (size > 0) {
        this->pkt_chain_->release();
        ACE_DEBUG((LM_WARNING,
                   ACE_TEXT("(%P|%t) WARNING: TransportSendStrategy::stop() - ")
                   ACE_TEXT("terminating with %d unsent bytes.\n"),
                   size));
      }
    }
  }

  delete this->header_mb_allocator_;
  delete this->header_db_allocator_;

  {
    GuardType guard(this->lock_);

    this->stop_i();
  }

  // TBD SOON - What about all of the samples that may still be stuck in
  //            our queue_ and/or elems_?
}

void
TransportSendStrategy::send(TransportQueueElement* element, bool relink)
{
  if (DCPS_debug_level > 9) {
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) TransportSendStrategy::send() [%d] - ")
               ACE_TEXT("sending data at 0x%x.\n"),
               id(), element));
  }

  DBG_ENTRY_LVL("TransportSendStrategy", "send", 6);
  //used for send_delayed_notifications at end of send(), must create local copy of pub_id
  //now to prevent element being destroyed prior to accessing for publication_id() for match creation
  RepoId pub_id(element->publication_id());

  {
    GuardType guard(this->lock_);

    if (this->link_released_) {

      if (this->num_delayed_notifications_ < this->max_samples_) {
        this->add_delayed_notification(element);
      } else {
        const SendMode mode = this->mode_;
        ACE_Reverse_Lock<LockType> reverse(this->lock_);
        ACE_Guard<ACE_Reverse_Lock<LockType> > reverseGuard(reverse);
        if (mode == MODE_TERMINATED) {
          element->data_dropped(true /*dropped_by_transport*/);
        } else {
          element->data_delivered();
        }
      }

    } else {
      if (this->mode_ == MODE_TERMINATED && !this->graceful_disconnecting_) {
        VDBG((LM_DEBUG, "(%P|%t) DBG:   "
              "TransportSendStrategy::send: mode is MODE_TERMINATED and not in "
              "graceful disconnecting, so discard message.\n"));
        element->data_dropped(true);
        return;
      }

      size_t element_length = element->msg()->total_length();

      VDBG((LM_DEBUG, "(%P|%t) DBG:   "
            "Send element msg() has total_length() == [%d].\n",
            element_length));

      VDBG((LM_DEBUG, "(%P|%t) DBG:   "
            "this->max_header_size_ == [%d].\n",
            this->max_header_size_));

      VDBG((LM_DEBUG, "(%P|%t) DBG:   "
            "this->max_size_ == [%d].\n",
            this->max_size_));

      const size_t max_message_size = this->max_message_size();

      // Really an assert.  We can't accept any element that wouldn't fit into
      // a transport packet by itself (ie, it would be the only element in the
      // packet).  This max_size_ is the user-configurable maximum, not based
      // on the transport's inherent maximum message size.  If max_message_size
      // is non-zero, we will fragment so max_size_ doesn't apply per-element.
      if (max_message_size == 0 &&
          this->max_header_size_ + element_length > this->max_size_) {
        ACE_ERROR((LM_ERROR,
                   "(%P|%t) ERROR: Element too large (%Q) "
                   "- won't fit into packet.\n", ACE_UINT64(element_length)));
        return;
      }

      // Check the mode_ to see if we simply put the element on the queue.
      if (this->mode_ == MODE_QUEUE || this->mode_ == MODE_SUSPEND) {
        VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
                  "this->mode_ == %C, so queue elem and leave.\n",
                  mode_as_str(this->mode_)), 5);

        this->queue_->put(element);

        if (this->mode_ != MODE_SUSPEND) {
          this->synch_->work_available();
        }

        return;
      }

      VDBG((LM_DEBUG, "(%P|%t) DBG:   "
            "this->mode_ == MODE_DIRECT.\n"));

      // We are in the MODE_DIRECT send mode.  When in this mode, the send()
      // calls will "build up" the transport packet to be sent directly when it
      // reaches the optimal size, contains the maximum number of samples, etc.

      // We need to check if the current element (the arg passed-in to this
      // send() method) should be appended to the transport packet, or if the
      // transport packet should be sent (directly) first, dealing with the
      // current element afterwards.

      // We will decide to send the packet as it is now, under two circumstances:
      //
      //    Either:
      //
      //    (1) The current element won't fit into the current packet since it
      //        would violate the max_packet_size_.
      //
      //    -OR-
      //
      //    (2) There is at least one element already in the current packet,
      //        and the current element says that it must be sent in an
      //        exclusive packet (ie, in a packet all by itself).
      //
      const bool exclusive = element->requires_exclusive_packet();

      VDBG((LM_DEBUG, "(%P|%t) DBG:   "
            "The element %C require an exclusive packet.\n",
            (exclusive ? "DOES" : "does NOT")
          ));

      const size_t space_needed =
        (max_message_size > 0)
        ? /* fragmenting */ DataSampleHeader::max_marshaled_size() + MIN_FRAG
        : /* not fragmenting */ element_length;

      if ((exclusive && (this->elems_->size() != 0))
          || (this->space_available() < space_needed)) {

        VDBG((LM_DEBUG, "(%P|%t) DBG:   "
              "Element won't fit in current packet or requires exclusive"
              " - send current packet (directly) now.\n"));

        VDBG((LM_DEBUG, "(%P|%t) DBG:   "
              "max_header_size_: %d, header_.length_: %d, element_length: %d\n"
              , this->max_header_size_, this->header_.length_, element_length));

        VDBG((LM_DEBUG, "(%P|%t) DBG:   "
              "Tot possible length: %d, max_len: %d\n"
              , this->max_header_size_ + this->header_.length_ + element_length
              , this->max_size_));
        VDBG((LM_DEBUG, "(%P|%t) DBG:   "
              "current elem size: %d\n"
              , this->elems_->size()));

        // Send the current packet, and deal with the current element
        // afterwards.
        // The invocation's relink status should dictate the direct_send's
        // relink. We don't want a (relink == false) invocation to end up
        // doing a relink. Think of (relink == false) as a non-blocking call.
        this->direct_send(relink);

        // Now check to see if we flipped into MODE_QUEUE, which would mean
        // that the direct_send() experienced backpressure, and the
        // packet was only partially sent.  If this has happened, we deal with
        // the current element by placing it on the queue (and then we are done).
        //
        // Otherwise, if the mode_ is still MODE_DIRECT, we can just
        // "drop" through to the next step in the logic where we append the
        // current element to the current packet.
        if (this->mode_ == MODE_QUEUE) {
          VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
                    "We experienced backpressure on that direct send, as "
                    "the mode_ is now MODE_QUEUE or MODE_SUSPEND.  "
                    "Queue elem and leave.\n"), 5);
          this->queue_->put(element);
          this->synch_->work_available();

          return;
        }
      }

      // Loop for sending 'element', in fragments if needed
      bool first_pkt = true; // enter the loop 1st time through unconditionally
      for (TransportQueueElement* next_fragment = 0;
           (first_pkt || next_fragment)
           && (this->mode_ == MODE_DIRECT || this->mode_ == MODE_TERMINATED);) {
           // We do need to send in MODE_TERMINATED (GRACEFUL_DISCONNECT msg)

        if (next_fragment) {
          element = next_fragment;
          element_length = next_fragment->msg()->total_length();
          this->header_.first_fragment_ = false;
        }

        this->header_.last_fragment_ = false;
        if (max_message_size) { // fragmentation enabled
          const size_t avail = this->space_available();
          if (element_length > avail) {
            VDBG_LVL((LM_TRACE, "(%P|%t) DBG:   Fragmenting\n"), 0);
            ElementPair ep = element->fragment(avail);
            element = ep.first;
            element_length = element->msg()->total_length();
            next_fragment = ep.second;
            this->header_.first_fragment_ = first_pkt;
          } else if (next_fragment) {
            // We are sending the "tail" element of a previous fragment()
            // operation, and this element didn't itself require fragmentation
            this->header_.last_fragment_ = true;
            next_fragment = 0;
          }
        }
        first_pkt = false;

        VDBG((LM_DEBUG, "(%P|%t) DBG:   "
              "Start the 'append elem' to current packet logic.\n"));

        VDBG((LM_DEBUG, "(%P|%t) DBG:   "
              "Put element into current packet elems_.\n"));

        // Now that we know the current element should go into the current
        // packet, we can just go ahead and "append" the current element to
        // the current packet.

        // Add the current element to the collection of packet elements.
        this->elems_->put(element);

        VDBG((LM_DEBUG, "(%P|%t) DBG:   "
              "Before, the header_.length_ == [%d].\n",
              this->header_.length_));

        // Adjust the header_.length_ to account for the length of the element.
        this->header_.length_ += static_cast<ACE_UINT32>(element_length);
        const size_t message_length = this->header_.length_;

        VDBG((LM_DEBUG, "(%P|%t) DBG:   "
              "After adding element's length, the header_.length_ == [%d].\n",
              message_length));

        // The current packet now contains the current element.  We need to
        // check to see if the conditions are such that we should go ahead and
        // attempt to send the packet "directly" now, or if we can just leave
        // and send the current packet later (in another send() call or in a
        // send_stop() call).

        // There a few conditions that will cause us to attempt to send the
        // packet (directly) right now:
        // - Fragmentation was needed
        // - The current packet has the maximum number of samples per packet.
        // - The current packet's total length exceeds the optimum packet size.
        // - The current element (currently part of the packet elems_)
        //   requires an exclusive packet.
        //
        if (next_fragment || (this->elems_->size() >= this->max_samples_)
            || (this->max_header_size_ + message_length > this->optimum_size_)
            || exclusive) {
          VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                "Now the current packet looks full - send it (directly).\n"));

          this->direct_send(relink);

          if (next_fragment && this->mode_ != MODE_DIRECT) {
            if (this->mode_ == MODE_QUEUE) {
              this->queue_->put(next_fragment);
              this->synch_->work_available();

            } else {
              next_fragment->data_dropped(true /* dropped by transport */);
            }
          } else if (mode_ == MODE_QUEUE) {
            // Background thread handles packets in progress
            this->synch_->work_available();
          }

          VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                "Back from the direct_send() attempt.\n"));

          VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                "And we %C as a result of the direct_send() call.\n",
                ((this->mode_ == MODE_QUEUE) ? "flipped into MODE_QUEUE"
                                             : "stayed in MODE_DIRECT")));

        } else {
          VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                "Packet not sent. Send conditions weren't satisfied.\n"));
          VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                "elems_->size(): %d, max_samples_: %d\n",
                int(this->elems_->size()), int(this->max_samples_)));
          VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                "header_size_: %d, optimum_size_: %d\n",
                int(this->max_header_size_ + message_length),
                int(this->optimum_size_)));
          VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                "element_requires_exclusive_packet: %d\n", int(exclusive)));

          if (this->mode_ == MODE_QUEUE) {
            VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                  "We flipped into MODE_QUEUE.\n"));

          } else {
            VDBG((LM_DEBUG, "(%P|%t) DBG:   "
                  "We stayed in MODE_DIRECT.\n"));
          }
        }
      }
    }
  }
  const TransportQueueElement::MatchOnPubId match(pub_id);
  send_delayed_notifications(&match);
  //send_delayed_notifications();
}

void
TransportSendStrategy::send_stop(RepoId repoId)
{
  DBG_ENTRY_LVL("TransportSendStrategy","send_stop",6);
  {
    GuardType guard(this->lock_);

    if (this->link_released_)
      return;

    if (this->start_counter_ == 0) {
      // This is an indication of a logic error.  This is more of an assert.
      VDBG_LVL((LM_ERROR,
                "(%P|%t) ERROR: Received unexpected send_stop() call.\n"), 5);
      return;
    }

    --this->start_counter_;

    if (this->start_counter_ != 0) {
      // This wasn't the last send_stop() that we are expecting.  We only
      // really honor the first send_start() and the last send_stop().
      // We can return without doing anything else in this case.
      return;
    }

    if (this->mode_ == MODE_TERMINATED && !this->graceful_disconnecting_) {
      VDBG((LM_DEBUG, "(%P|%t) DBG:   "
            "TransportSendStrategy::send_stop: dont try to send current packet "
            "since mode is MODE_TERMINATED and not in graceful disconnecting.\n"));
      return;
    }

    VDBG((LM_DEBUG, "(%P|%t) DBG:   "
          "This is an 'important' send_stop() event since our "
          "start_counter_ is 0.\n"));

    // We just caused the start_counter_ to become zero.  This
    // means that we aren't expecting another send() or send_stop() at any
    // time in the near future (ie, it isn't imminent).

    // If our mode_ is currently MODE_QUEUE or MODE_SUSPEND, then we don't have
    // anything to do here because samples have already been going to the
    // queue.

    // We only need to do something if the mode_ is
    // MODE_DIRECT.  It means that we may have some sample(s) in the
    // current packet that have never been sent.  This is our
    // opportunity to send the current packet directly if this is the case.
    if (this->mode_ == MODE_QUEUE || this->mode_ == MODE_SUSPEND) {
      VDBG((LM_DEBUG, "(%P|%t) DBG:   "
            "But since we are in %C, we don't have to do "
            "anything more in this important send_stop().\n",
            mode_as_str(this->mode_)));
      // We don't do anything if we are in MODE_QUEUE.  Just leave.
      return;
    }

    size_t header_length = this->header_.length_;
    VDBG((LM_DEBUG, "(%P|%t) DBG:   "
          "We are in MODE_DIRECT in an important send_stop() - "
          "header_.length_ == [%d].\n", header_length));

    // Only attempt to send the current packet (directly) if the current
    // packet actually contains something (it could be empty).
    if ((header_length > 0) &&
        //(this->elems_->size ()+this->not_yet_pac_q_->size() > 0))
        (this->elems_->size() > 0)) {
      VDBG((LM_DEBUG, "(%P|%t) DBG:   "
            "There is something in the current packet - attempt to send "
            "it (directly) now.\n"));
      // If a relink needs to be done for this packet to be sent, do it.
      this->direct_send(true);
      VDBG((LM_DEBUG, "(%P|%t) DBG:   "
            "Back from the attempt to send leftover packet directly.\n"));

      VDBG((LM_DEBUG, "(%P|%t) DBG:   "
            "But we %C as a result.\n",
            ((this->mode_ == MODE_QUEUE)? "flipped into MODE_QUEUE":
                                          "stayed in MODE_DIRECT" )));
    }
  }
  if(repoId == GUID_UNKNOWN) {
    //Then this is a ThreadPerConnectionSendTask call and there will be no deadlock by
    //TransportSendStrategy::send_stop calling into send_delayed_notifications() with no match
    send_delayed_notifications();
  } else {
    const TransportQueueElement::MatchOnPubId match(repoId);
    send_delayed_notifications(&match);
  }
}

void
TransportSendStrategy::remove_all_msgs(RepoId pub_id)
{
  DBG_ENTRY_LVL("TransportSendStrategy","remove_all_msgs",6);

  const TransportQueueElement::MatchOnPubId match(pub_id);
  send_delayed_notifications(&match);

  GuardType guard(this->lock_);

  if (this->send_buffer_ != 0) {
    // If a secondary send buffer is bound, removed samples must
    // be retained in order to properly maintain the buffer:
    this->send_buffer_->retain_all(pub_id);
  }

  do_remove_sample(match);
}

RemoveResult
TransportSendStrategy::remove_sample(const DataSampleListElement* sample)
{
  DBG_ENTRY_LVL("TransportSendStrategy", "remove_sample", 6);

  VDBG_LVL((LM_DEBUG, "(%P|%t)  Removing sample: %@\n", sample->sample_), 5);

  // The sample to remove is either in temporary delayed notification list or
  // internal list (elems_ or queue_). If it's going to be removed from temporary delayed
  // notification list by transport thread, it needs acquire WriterDataContainer lock for
  // data_dropped/data_delivered callback, then it needs wait for this remove_sample() call
  // complete as this call already hold the WriterContainer's lock. So this call is safe to
  // access the sample to remove. If it's going to be removed by this remove_sample() calling
  // thread, it will be removed either from delayed notification list or from internal list
  // in which case the element carry the info if the sample is released so the datalinkset
  // can stop calling rest datalinks to remove this sample if it's already released..

  const char* const payload = sample->sample_->cont()->rd_ptr();
  const TransportQueueElement::MatchOnDataPayload modp(payload);
  if (send_delayed_notifications(&modp)) {
    return REMOVE_RELEASED;
  }

  GuardType guard(this->lock_);
  return do_remove_sample(modp);
}

RemoveResult
TransportSendStrategy::do_remove_sample(
  const TransportQueueElement::MatchCriteria& criteria)
{
  DBG_ENTRY_LVL("TransportSendStrategy", "do_remove_sample", 6);

  //ciju: Tim had the idea that we could do the following check
  // if ((this->mode_ == MODE_DIRECT) ||
  //     ((this->pkt_chain_ == 0) && (queue_ == empty)))
  // then we can assume that the sample can be safely removed (no need for
  // replacement) from the elems_ queue.
  if ((this->mode_ == MODE_DIRECT)
      || ((this->pkt_chain_ == 0) && (this->queue_->size() == 0))) {
    //ciju: I believe this is the only mode where a safe
    // assumption can be made that the samples
    // in the elems_ queue aren't part of a packet.
    VDBG((LM_DEBUG, "(%P|%t) DBG:   "
          "The mode is MODE_DIRECT, or the queue is empty and no "
          "transport packet is in progress.\n"));

    QueueRemoveVisitor simple_rem_vis(criteria);
    this->elems_->accept_remove_visitor(simple_rem_vis);

    const RemoveResult status = simple_rem_vis.status();

    if (status == REMOVE_RELEASED || status == REMOVE_FOUND) {
      this->header_.length_ -= simple_rem_vis.removed_bytes();

    } else if (status == REMOVE_NOT_FOUND) {
      VDBG((LM_DEBUG, "(%P|%t) DBG:   "
            "Failed to find the sample to remove.\n"));
    }

    return status;
  }

  VDBG((LM_DEBUG, "(%P|%t) DBG:   "
        "Visit the queue_ with the RemoveElementVisitor.\n"));

  QueueRemoveVisitor simple_rem_vis(criteria);
  this->queue_->accept_remove_visitor(simple_rem_vis);

  RemoveResult status = simple_rem_vis.status();

  if (status == REMOVE_RELEASED || status == REMOVE_FOUND) {
    VDBG((LM_DEBUG, "(%P|%t) DBG:   "
          "The sample was removed from the queue_.\n"));
    // This means that the visitor did not encounter any fatal error
    // along the way, *AND* the sample was found in the queue_,
    // and has now been removed.  We are done.
    return status;
  }

  if (status == REMOVE_ERROR) {
    VDBG((LM_DEBUG, "(%P|%t) DBG:   "
          "The RemoveElementVisitor encountered a fatal error in queue_.\n"));
    // This means that the visitor encountered some fatal error along
    // the way (and it already reported something to the log).
    // Return our failure code.
    return status;
  }

  VDBG((LM_DEBUG, "(%P|%t) DBG:   "
        "The RemoveElementVisitor did not find the sample in queue_.\n"));

  // We get here if the visitor did not encounter any fatal error, but it
  // also didn't find the sample - and hence it didn't perform any
  // "remove sample" logic.

  // Now we need to turn our attention to the current transport packet,
  // since the packet is likely in a "partially sent" state, and the
  // sample may still be contributing unsent bytes in the pkt_chain_.

  VDBG((LM_DEBUG, "(%P|%t) DBG:   "
        "Visit our elems_ with the PacketRemoveVisitor.\n"));

  PacketRemoveVisitor pac_rem_vis(criteria,
                                  this->pkt_chain_,
                                  this->header_block_,
                                  this->replaced_element_allocator_,
                                  this->replaced_element_mb_allocator_,
                                  this->replaced_element_db_allocator_);

  this->elems_->accept_replace_visitor(pac_rem_vis);

  status = pac_rem_vis.status();

  if (status == REMOVE_ERROR) {
    VDBG((LM_DEBUG, "(%P|%t) DBG:   "
          "The PacketRemoveVisitor encountered a fatal error.\n"));

  } else if (status == REMOVE_NOT_FOUND) {
    VDBG((LM_DEBUG, "(%P|%t) DBG:   "
          "The PacketRemoveVisitor didn't find the sample.\n"));

  } else {
    VDBG((LM_DEBUG, "(%P|%t) DBG:   "
          "The PacketRemoveVisitor found the sample and removed it.\n"));
  }

  return status;
}

void
TransportSendStrategy::direct_send(bool relink)
{
  DBG_ENTRY_LVL("TransportSendStrategy", "direct_send", 6);

  VDBG((LM_DEBUG, "(%P|%t) DBG:   "
        "Prepare the current packet for a direct send attempt.\n"));

  // Prepare the packet for sending.
  this->prepare_packet();

  VDBG((LM_DEBUG, "(%P|%t) DBG:   "
        "Now attempt to send the packet.\n"));

  // We will try resend the packet if the send() fails and then connection
  // is re-established.  Only loops if the "continue" line is hit.
  while (true) {
    // ciju: I have changed the default. Immediate notification
    // path enters the Data Store with the local locks held. This
    // can lead to deadlocks often. I have added temporary fixes
    // for the immediate notification, but they don't guarantee
    // deadlock prevention. The flaw is in the design and
    // my solution is just best effort in the given context.
    /* ChangeLog comment
       In TransportSendStrategy set DELAY_NOTIFICATION to be default
       for direct_send(). This could delay things a bit in the normal
       case but doing an immediate notification has been observed to
       deadlock in the DCPS layer in stress conditions. This occurs
       when the data_deliverd () call winds its way from the transport
       to DCPS layer while another thread is winding its way up.
    */
    //UseDelayedNotification when = NOTIFY_IMMEADIATELY;
    UseDelayedNotification when = DELAY_NOTIFICATION;

    // If the thread per connection is used we can not hold the send strategy
    // lock and try to call data_delivered since the
    // DataWriterImpl::remove_sample() might hold the locks in reverse order
    // and cause datalock.
    if (this->transport_inst_->thread_per_connection_) {
      when = DELAY_NOTIFICATION;
    }

    // Attempt to send the packet
    const SendPacketOutcome outcome = this->send_packet(when);

    VDBG((LM_DEBUG, "(%P|%t) DBG:   "
          "The outcome of the send_packet() was %d.\n", outcome));

    if ((outcome == OUTCOME_BACKPRESSURE) ||
        (outcome == OUTCOME_PARTIAL_SEND)) {
      VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
                "The outcome of the send_packet() was either "
                "OUTCOME_BACKPRESSURE or OUTCOME_PARTIAL_SEND.\n"), 5);

      VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
                "Flip into the MODE_QUEUE mode_.\n"), 5);

      // We encountered backpressure, or only sent part of the packet.
      this->mode_ = MODE_QUEUE;

    } else if ((outcome == OUTCOME_PEER_LOST) ||
               (outcome == OUTCOME_SEND_ERROR)) {
      if (outcome == OUTCOME_SEND_ERROR) {
        ACE_ERROR((LM_WARNING,
                   ACE_TEXT("(%P|%t) WARNING: Problem detected in ")
                   ACE_TEXT("send buffer management: %p.\n"),
                   ACE_TEXT("send_bytes")));

        if (Transport_debug_level > 0) {
          this->transport_inst_->dump();
        }
      } else {
        VDBG((LM_DEBUG, "(%P|%t) DBG:   "
              "The outcome of the send_packet() was "
              "OUTCOME_PEER_LOST.\n"));
      }

      VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
                "Now flip to MODE_SUSPEND before we try to reconnect.\n"), 5);

      if (this->mode_ != MODE_SUSPEND) {
        this->mode_before_suspend_ = this->mode_;
        this->mode_ = MODE_SUSPEND;
      }

      if (relink) {
        bool do_suspend = false;
        this->relink(do_suspend);

        if (this->mode_ == MODE_SUSPEND) {
          VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
                    "The reconnect has not done yet and we are "
                    "still in MODE_SUSPEND.\n"), 5);

        } else if (this->mode_ == MODE_TERMINATED) {
          VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
                    "Reconnect failed, we are in MODE_TERMINATED\n"), 5);

        } else {
          VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
                    "Try send the packet again since the connection "
                    "is re-established.\n"), 5);

          // Try send the packet again since the connection is re-established.
          continue;
        }
      }

    } else {
      VDBG((LM_DEBUG, "(%P|%t) DBG:   "
            "The outcome of the send_packet() must have been "
            "OUTCOME_COMPLETE_SEND.\n"));
      VDBG((LM_DEBUG, "(%P|%t) DBG:   "
            "So, we will just stay in MODE_DIRECT.\n"));
    }

    break;
  }

  // We stay in MODE_DIRECT mode if we didn't encounter any backpressure.
}

void
TransportSendStrategy::get_packet_elems_from_queue()
{
  DBG_ENTRY_LVL("TransportSendStrategy", "get_packet_elems_from_queue", 6);

  for (TransportQueueElement* element = this->queue_->peek(); element != 0;
       element = this->queue_->peek()) {

    // Total number of bytes in the current element's message block chain.
    size_t element_length = element->msg()->total_length();

    // Flag used to determine if the element requires a packet all to itself.
    const bool exclusive_packet = element->requires_exclusive_packet();

    const size_t avail = this->space_available();

    bool frag = false;
    if (element_length > avail) {
      // The current element won't fit into the current packet
      if (this->max_message_size()) { // fragmentation enabled
        this->header_.first_fragment_ = !element->is_fragment();
        VDBG_LVL((LM_TRACE, "(%P|%t) DBG:   Fragmenting from queue\n"), 0);
        ElementPair ep = element->fragment(avail);
        element = ep.first;
        element_length = element->msg()->total_length();
        this->queue_->replace_head(ep.second);
        frag = true; // queue_ is already taken care of, don't get() later
      } else {
        break;
      }
    }

    // If exclusive and the current packet is empty, we won't violate the
    // exclusive_packet requirement by put()'ing the element
    // into the elems_ collection.
    if ((exclusive_packet && this->elems_->size() == 0)
        || !exclusive_packet) {
      // At this point, we have passed all of the pre-conditions and we can
      // now extract the current element from the queue_, put it into the
      // packet elems_, and adjust the packet header_.length_.
      this->elems_->put(frag ? element : this->queue_->get());
      if (this->header_.length_ == 0) {
        this->header_.last_fragment_ = !frag && element->is_fragment();
      }
      this->header_.length_ += static_cast<ACE_UINT32>(element_length);
      VDBG_LVL((LM_TRACE, "(%P|%t) DBG:   Packetizing from queue\n"), 0);
    }

    // With exclusive and (elems_.size() != 0), we don't use the current
    // element as part of the packet.  We know that there is already
    // at least one element in the packet, and the current element
    // is going to need its own (exclusive) packet.  We will just
    // use the packet elems_ as it is now.  Always break once
    // we've encountered and dealt with the exclusive_packet case.
    // Also break if fragmentation was required.
    if (exclusive_packet || frag
        // If the current number of packet elems_ has reached the maximum
        // number of samples per packet, then we are done.
        || this->elems_->size() == this->max_samples_
        // If the current value of the header_.length_ exceeds (or equals)
        // the optimum_size_ for a packet, then we are done.
        || this->header_.length_ >= this->optimum_size_) {
      break;
    }
  }
}

void
TransportSendStrategy::prepare_header()
{
  DBG_ENTRY_LVL("TransportSendStrategy", "prepare_header", 6);

  // Increment header sequence for packet:
  this->header_.sequence_ = ++this->header_sequence_;

  // Allow the specific implementation the opportunity to set
  // values in the packet header.
  this->prepare_header_i();
}

void
TransportSendStrategy::prepare_header_i()
{
  DBG_ENTRY_LVL("TransportSendStrategy","prepare_header_i",6);

  // Default implementation does nothing.
}

void
TransportSendStrategy::prepare_packet()
{
  DBG_ENTRY_LVL("TransportSendStrategy", "prepare_packet", 6);

  // Prepare the header for sending.
  this->prepare_header();

  VDBG((LM_DEBUG, "(%P|%t) DBG:   "
        "Marshal the packet header.\n"));

  if (this->header_block_ != 0) {
    this->header_block_->release();
  }

  ACE_NEW_MALLOC(this->header_block_,
    static_cast<ACE_Message_Block*>(this->header_mb_allocator_->malloc()),
    ACE_Message_Block(this->max_header_size_,
                      ACE_Message_Block::MB_DATA,
                      0,
                      0,
                      0,
                      0,
                      ACE_DEFAULT_MESSAGE_BLOCK_PRIORITY,
                      ACE_Time_Value::zero,
                      ACE_Time_Value::max_time,
                      this->header_db_allocator_,
                      this->header_mb_allocator_));

  marshal_transport_header(this->header_block_);

  this->pkt_chain_ = this->header_block_->duplicate();

  VDBG((LM_DEBUG, "(%P|%t) DBG:   "
        "Use a BuildChainVisitor to visit the packet elems_.\n"));

  // Build up a chain of blocks by duplicating the message block chain
  // held by each element (in elems_), and then chaining the new duplicate
  // blocks together to form one long chain.
  BuildChainVisitor visitor;
  this->elems_->accept_visitor(visitor);

  VDBG((LM_DEBUG, "(%P|%t) DBG:   "
        "Attach the visitor's chain of blocks to the lone (packet "
        "header) block currently in the pkt_chain_.\n"));

  // Attach the visitor's chain of blocks to the packet header block.
  this->pkt_chain_->cont(visitor.chain());

  VDBG((LM_DEBUG, "(%P|%t) DBG:   "
        "Increment header sequence for next packet.\n"));

  // Allow the specific implementation the opportunity to process the
  // newly prepared packet.
  this->prepare_packet_i();

  VDBG((LM_DEBUG, "(%P|%t) DBG:   "
        "Set the header_complete_ flag to false.\n"));

  // Set the header_complete_ to false to indicate
  // that the first block in the pkt_chain_ is the packet header block
  // (actually a duplicate() of the packet header_block_).
  this->header_complete_ = false;
}

void
TransportSendStrategy::marshal_transport_header(ACE_Message_Block* mb)
{
  *mb << this->header_;
}

void
TransportSendStrategy::prepare_packet_i()
{
  DBG_ENTRY_LVL("TransportSendStrategy","prepare_packet_i",6);

  // Default implementation does nothing.
}

ssize_t
TransportSendStrategy::do_send_packet(const ACE_Message_Block* packet, int& bp)
{
  if (DCPS_debug_level > 9) {
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) TransportSendStrategy::do_send_packet() [%d] - ")
               ACE_TEXT("sending data at 0x%x.\n"),
               id(), packet));
  }
  DBG_ENTRY_LVL("TransportSendStrategy", "do_send_packet", 6);

  VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
            "Populate the iovec array using the packet.\n"), 5);

  iovec iov[MAX_SEND_BLOCKS];

  int num_blocks = mb_to_iov(*packet, iov);

  VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
            "There are [%d] number of entries in the iovec array.\n",
            num_blocks), 5);

  VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
            "Attempt to send_bytes() now.\n"), 5);

  const ssize_t num_bytes_sent = this->send_bytes(iov, num_blocks, bp);

  VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
            "The send_bytes() said that num_bytes_sent == [%d].\n",
            num_bytes_sent), 5);

  return num_bytes_sent;
}

TransportSendStrategy::SendPacketOutcome
TransportSendStrategy::send_packet(UseDelayedNotification delay_notification)
{
  DBG_ENTRY_LVL("TransportSendStrategy", "send_packet", 6);

  int bp_flag = 0;
  const ssize_t num_bytes_sent =
    this->do_send_packet(this->pkt_chain_, bp_flag);

  if (num_bytes_sent == 0) {
    VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
              "Since num_bytes_sent == 0, return OUTCOME_PEER_LOST.\n"), 5);
    // This means that the peer has disconnected.
    return OUTCOME_PEER_LOST;
  }

  if (num_bytes_sent < 0) {
    VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
              "Since num_bytes_sent < 0, check the backpressure flag.\n"), 5);

    // Check for backpressure...
    if (bp_flag == 1) {
      VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
                "Since backpressure flag is true, return "
                "OUTCOME_BACKPRESSURE.\n"), 5);
      // Ok.  Not really an error - just backpressure.
      return OUTCOME_BACKPRESSURE;
    }

    VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
              "Since backpressure flag is false, return "
              "OUTCOME_SEND_ERROR.\n"), 5);

    // Not backpressure - it's a real error.
    // Note: moved this to send_bytes so the errno msg could be written.
    //ACE_ERROR((LM_ERROR,
    //           "(%P|%t) ERROR: Call to peer().send() failed with negative "
    //           "return code.\n"));

    return OUTCOME_SEND_ERROR;
  }

  if (this->send_buffer_ != 0) {
    // If a secondary send buffer is bound, sent samples must
    // be inserted in order to properly maintain the buffer:
    this->send_buffer_->insert(this->header_.sequence_,
      this->elems_, this->pkt_chain_);
  }

  VDBG_LVL((LM_DEBUG, "(%P|%t) DBG:   "
            "Since num_bytes_sent > 0, adjust the packet to account for "
            "the bytes that did get sent.\n"),5);

  // We sent some bytes - adjust the current packet (elems_ and pkt_chain_)
  // to account for the bytes that have been sent.
  const int result =
    this->adjust_packet_after_send(num_bytes_sent, delay_notification);

  if (result == 0) {
    VDBG((LM_DEBUG, "(%P|%t) DBG:   "
          "The adjustment logic says that the complete packet was "
          "sent.  Return OUTCOME_COMPLETE_SEND.\n"));
    return OUTCOME_COMPLETE_SEND;
  }

  VDBG((LM_DEBUG, "(%P|%t) DBG:   "
        "The adjustment logic says that only a part of the packet was "
        "sent. Return OUTCOME_PARTIAL_SEND.\n"));

  return OUTCOME_PARTIAL_SEND;
}

ssize_t
TransportSendStrategy::non_blocking_send(const iovec iov[], int n, int& bp)
{
  int val = 0;
  ACE_HANDLE handle = this->get_handle();

  if (handle == ACE_INVALID_HANDLE)
    return -1;

  ACE::record_and_set_non_blocking_mode(handle, val);

  // Set the back-pressure flag to false.
  bp = 0;

  // Clear errno
  errno = 0;

  ssize_t result = this->send_bytes_i(iov, n);

  if (result == -1) {
    if ((errno == EWOULDBLOCK) || (errno == ENOBUFS)) {
      VDBG((LM_DEBUG,"(%P|%t) DBG:   "
            "Backpressure encountered.\n"));
      // Set the back-pressure flag to true
      bp = 1;

    } else {
      VDBG_LVL((LM_ERROR, "(%P|%t) TransportSendStrategy::send_bytes: ERROR: %p iovec count: %d\n",
                ACE_TEXT("sendv"), n),1);

      // try to get the application to core when "Bad Address" is returned
      // by looking at the iovec
      for (int ii = 0; ii < n; ii++) {
        ACE_DEBUG((LM_DEBUG, "(%P|%t) send_bytes: iov[%d].iov_len = %d .iov_base =%X\n",
                   ii, iov[ii].iov_len, iov[ii].iov_base));
      }
    }
  }

  VDBG_LVL((LM_DEBUG,"(%P|%t) DBG:   "
            "The sendv() returned [%d].\n", result), 5);

  ACE::restore_non_blocking_mode(handle, val);

  return result;
}

void
TransportSendStrategy::add_delayed_notification(TransportQueueElement* element)
{
  const size_t ndn = this->num_delayed_notifications_;
  this->delayed_delivered_notification_queue_[ndn] = element;
  this->delayed_notification_mode_[ndn] = this->mode_;
  ++this->num_delayed_notifications_;
}

size_t
TransportSendStrategy::space_available() const
{
  const size_t used = this->max_header_size_ + this->header_.length_,
    max_msg = this->max_message_size();
  if (max_msg) {
    return std::min(this->max_size_ - used, max_msg - used);
  }
  return this->max_size_ - used;
}

int
TransportSendStrategy::mb_to_iov(const ACE_Message_Block& msg, iovec* iov)
{
  int num_blocks = 0;
#ifdef _MSC_VER
#pragma warning(push)
// iov_len is 32-bit on 64-bit VC++, but we don't want a cast here
// since on other platforms iov_len is 64-bit
#pragma warning(disable : 4267)
#endif
  for (const ACE_Message_Block* block = &msg;
       block && num_blocks < MAX_SEND_BLOCKS;
       block = block->cont()) {
    iov[num_blocks].iov_len = block->length();
    iov[num_blocks++].iov_base = block->rd_ptr();
  }
#ifdef _MSC_VER
#pragma warning(pop)
#endif
  return num_blocks;
}

// close namespaces
}
}
