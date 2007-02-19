// -*- C++ -*-
//
// $Id$

#include "ReliableMulticast_pch.h"
#include "EventHandler.h"
#include <iostream>
#include <stdexcept>

#if !defined (__ACE_INLINE__)
#include "EventHandler.inl"
#endif /* __ACE_INLINE__ */

void
TAO::DCPS::ReliableMulticast::detail::EventHandler::send(
  char* buffer,
  size_t size,
  const ACE_INET_Addr& dest
  )
{
  ACE_Guard<ACE_Thread_Mutex> lock(output_mutex_);
  bool reregister = output_queue_.empty();

  output_queue_.push(std::make_pair(std::string(buffer, size), dest));
  if (reregister)
  {
    if (reactor()->register_handler(
      this,
      ACE_Event_Handler::WRITE_MASK
      ) == -1)
    {
      throw std::runtime_error("failure to register_handler");
    }
  }
}

ACE_HANDLE
TAO::DCPS::ReliableMulticast::detail::EventHandler::get_handle() const
{
  return socket_.get_handle();
}

int
TAO::DCPS::ReliableMulticast::detail::EventHandler::handle_input(
  ACE_HANDLE fd
  )
{
  ACE_UNUSED_ARG(fd);

  char buffer[1024];
  ACE_INET_Addr peer;

  ssize_t bytes_read = socket_.recv(
    static_cast<void*>(buffer), sizeof(buffer), peer
    );
  if (bytes_read > 0)
  {
    ACE_Guard<ACE_Thread_Mutex> lock(input_mutex_);
    receive(buffer, bytes_read, peer);
  }
  return 0;
}

int
TAO::DCPS::ReliableMulticast::detail::EventHandler::handle_output(
  ACE_HANDLE fd
  )
{
  ACE_UNUSED_ARG(fd);

  ACE_Guard<ACE_Thread_Mutex> lock(output_mutex_);
  if (!output_queue_.empty())
  {
    Queue::value_type& item = output_queue_.front();
    ssize_t bytes_sent = socket_.ACE_SOCK_Dgram::send(
      static_cast<const void*>(item.first.c_str()),
      item.first.size(),
      item.second
      );

    if (size_t(bytes_sent) == item.first.size())
    {
      output_queue_.pop();
    }
    else if (bytes_sent > 0)
    {
      item.first = item.first.substr(bytes_sent);
    }
  }
  if (output_queue_.empty())
  {
    reactor()->remove_handler(this, ACE_Event_Handler::WRITE_MASK);
  }
  return 0;
}

int
TAO::DCPS::ReliableMulticast::detail::EventHandler::handle_close(
  ACE_HANDLE fd,
  ACE_Reactor_Mask mask
  )
{
  ACE_UNUSED_ARG(fd);

  if (
    mask == ACE_Event_Handler::WRITE_MASK ||
    mask == ACE_Event_Handler::TIMER_MASK
    )
  {
    return 0;
  }
  if (socket_.get_handle() != ACE_INVALID_HANDLE)
  {
    reactor()->remove_handler(
      this,
      ACE_Event_Handler::ALL_EVENTS_MASK | ACE_Event_Handler::DONT_CALL
      );
    socket_.close();
  }
  return 0;
}
