// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/ports/node.h"

#include <string.h>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "mojo/edk/system/ports/node_delegate.h"

namespace mojo {
namespace edk {
namespace ports {

namespace {

int DebugError(const char* message, int error_code) {
  CHECK(false) << "Oops: " << message;
  return error_code;
}

#define OOPS(x) DebugError(#x, x)

bool CanAcceptMoreMessages(const Port* port) {
  // Have we already doled out the last message (i.e., do we expect to NOT
  // receive further messages)?
  uint64_t next_sequence_num = port->message_queue.next_sequence_num();
  if (port->peer_closed || port->remove_proxy_on_last_message) {
    if (port->last_sequence_num_to_receive == next_sequence_num - 1)
      return false;
  }
  return true;
}

}  // namespace

Node::Node(const NodeName& name, NodeDelegate* delegate)
    : name_(name),
      delegate_(delegate) {
}

Node::~Node() {
  if (!ports_.empty())
    DLOG(WARNING) << "Unclean shutdown for node " << name_;
}

bool Node::CanShutdownCleanly(bool allow_local_ports) {
  base::AutoLock ports_lock(ports_lock_);

  if (!allow_local_ports) {
#if !defined(NDEBUG)
    for (auto entry : ports_) {
      DVLOG(2) << "Port " << entry.first << " referencing node "
               << entry.second->peer_node_name << " is blocking shutdown of "
               << "node " << name_ << " (state=" << entry.second->state << ")";
    }
#endif
    return ports_.empty();
  }

  // NOTE: This is not efficient, though it probably doesn't need to be since
  // relatively few ports should be open during shutdown and shutdown doesn't
  // need to be blazingly fast.
  bool can_shutdown = true;
  for (auto entry : ports_) {
    base::AutoLock lock(entry.second->lock);
    if (entry.second->peer_node_name != name_ &&
        entry.second->state != Port::kReceiving) {
      can_shutdown = false;
#if !defined(NDEBUG)
      DVLOG(2) << "Port " << entry.first << " referencing node "
               << entry.second->peer_node_name << " is blocking shutdown of "
               << "node " << name_ << " (state=" << entry.second->state << ")";
#else
      // Exit early when not debugging.
      break;
#endif
    }
  }

  return can_shutdown;
}

int Node::GetPort(const PortName& port_name, PortRef* port_ref) {
  scoped_refptr<Port> port = GetPort(port_name);
  if (!port)
    return ERROR_PORT_UNKNOWN;

  *port_ref = PortRef(port_name, std::move(port));
  return OK;
}

int Node::CreateUninitializedPort(PortRef* port_ref) {
  PortName port_name;
  delegate_->GenerateRandomPortName(&port_name);

  scoped_refptr<Port> port = make_scoped_refptr(new Port(kInitialSequenceNum,
                                                         kInitialSequenceNum));
  int rv = AddPortWithName(port_name, port);
  if (rv != OK)
    return rv;

  *port_ref = PortRef(port_name, std::move(port));
  return OK;
}

int Node::InitializePort(const PortRef& port_ref,
                         const NodeName& peer_node_name,
                         const PortName& peer_port_name) {
  Port* port = port_ref.port();

  {
    base::AutoLock lock(port->lock);
    if (port->state != Port::kUninitialized)
      return ERROR_PORT_STATE_UNEXPECTED;

    port->state = Port::kReceiving;
    port->peer_node_name = peer_node_name;
    port->peer_port_name = peer_port_name;
  }

  delegate_->PortStatusChanged(port_ref);

  return OK;
}

int Node::CreatePortPair(PortRef* port0_ref, PortRef* port1_ref) {
  int rv;

  rv = CreateUninitializedPort(port0_ref);
  if (rv != OK)
    return rv;

  rv = CreateUninitializedPort(port1_ref);
  if (rv != OK)
    return rv;

  rv = InitializePort(*port0_ref, name_, port1_ref->name());
  if (rv != OK)
    return rv;

  rv = InitializePort(*port1_ref, name_, port0_ref->name());
  if (rv != OK)
    return rv;

  return OK;
}

int Node::SetUserData(const PortRef& port_ref,
                      const scoped_refptr<UserData>& user_data) {
  Port* port = port_ref.port();

  base::AutoLock lock(port->lock);
  if (port->state == Port::kClosed)
    return ERROR_PORT_STATE_UNEXPECTED;

  port->user_data = std::move(user_data);

  return OK;
}

int Node::GetUserData(const PortRef& port_ref,
                      scoped_refptr<UserData>* user_data) {
  Port* port = port_ref.port();

  base::AutoLock lock(port->lock);
  if (port->state == Port::kClosed)
    return ERROR_PORT_STATE_UNEXPECTED;

  *user_data = port->user_data;

  return OK;
}

int Node::ClosePort(const PortRef& port_ref) {
  std::deque<PortName> referenced_port_names;

  ObserveClosureEventData data;

  NodeName peer_node_name;
  PortName peer_port_name;
  Port* port = port_ref.port();
  {
    // We may need to erase the port, which requires ports_lock_ to be held,
    // but ports_lock_ must be acquired before any individual port locks.
    base::AutoLock ports_lock(ports_lock_);

    base::AutoLock lock(port->lock);
    if (port->state == Port::kUninitialized) {
      // If the port was not yet initialized, there's nothing interesting to do.
      ErasePort_Locked(port_ref.name());
      return OK;
    }

    if (port->state != Port::kReceiving)
      return ERROR_PORT_STATE_UNEXPECTED;

    port->state = Port::kClosed;

    // We pass along the sequence number of the last message sent from this
    // port to allow the peer to have the opportunity to consume all inbound
    // messages before notifying the embedder that this port is closed.
    data.last_sequence_num = port->next_sequence_num_to_send - 1;

    peer_node_name = port->peer_node_name;
    peer_port_name = port->peer_port_name;

    // If the port being closed still has unread messages, then we need to take
    // care to close those ports so as to avoid leaking memory.
    port->message_queue.GetReferencedPorts(&referenced_port_names);
  }

  DVLOG(2) << "Sending ObserveClosure from " << port_ref.name() << "@" << name_
           << " to " << peer_port_name << "@" << peer_node_name;

  ErasePort(port_ref.name());

  delegate_->ForwardMessage(
      peer_node_name,
      NewInternalMessage(peer_port_name, EventType::kObserveClosure, data));

  for (const auto& name : referenced_port_names) {
    PortRef ref;
    if (GetPort(name, &ref) == OK)
      ClosePort(ref);
  }
  return OK;
}

int Node::GetStatus(const PortRef& port_ref, PortStatus* port_status) {
  Port* port = port_ref.port();

  base::AutoLock lock(port->lock);

  if (port->state != Port::kReceiving)
    return ERROR_PORT_STATE_UNEXPECTED;

  port_status->has_messages = port->message_queue.HasNextMessage();
  port_status->receiving_messages = CanAcceptMoreMessages(port);
  port_status->peer_closed = port->peer_closed;
  return OK;
}

int Node::GetMessage(const PortRef& port_ref, ScopedMessage* message) {
  return GetMessageIf(port_ref, nullptr, message);
}

int Node::GetMessageIf(const PortRef& port_ref,
                       std::function<bool(const Message&)> selector,
                       ScopedMessage* message) {
  *message = nullptr;

  DVLOG(2) << "GetMessageIf for " << port_ref.name() << "@" << name_;

  Port* port = port_ref.port();
  {
    base::AutoLock lock(port->lock);

    // This could also be treated like the port being unknown since the
    // embedder should no longer be referring to a port that has been sent.
    if (port->state != Port::kReceiving)
      return ERROR_PORT_STATE_UNEXPECTED;

    // Let the embedder get messages until there are no more before reporting
    // that the peer closed its end.
    if (!CanAcceptMoreMessages(port))
      return ERROR_PORT_PEER_CLOSED;

    port->message_queue.GetNextMessageIf(selector, message);
  }

  // Allow referenced ports to trigger PortStatusChanged calls.
  if (*message) {
    for (size_t i = 0; i < (*message)->num_ports(); ++i) {
      const PortName& new_port_name = (*message)->ports()[i];
      scoped_refptr<Port> new_port = GetPort(new_port_name);

      DCHECK(new_port) << "Port " << new_port_name << "@" << name_
                       << " does not exist!";

      base::AutoLock lock(new_port->lock);

      DCHECK(new_port->state == Port::kReceiving);
      new_port->message_queue.set_signalable(true);
    }
  }

  return OK;
}

int Node::SendMessage(const PortRef& port_ref, ScopedMessage* message) {
  ScopedMessage& m = *message;
  for (size_t i = 0; i < m->num_ports(); ++i) {
    if (m->ports()[i] == port_ref.name())
      return ERROR_PORT_CANNOT_SEND_SELF;
  }

  Port* port = port_ref.port();
  {
    // We must acquire |ports_lock_| before grabbing any port locks, because
    // WillSendMessage_Locked may need to lock multiple ports out of order.
    base::AutoLock ports_lock(ports_lock_);
    base::AutoLock lock(port->lock);

    if (port->state != Port::kReceiving)
      return ERROR_PORT_STATE_UNEXPECTED;

    if (port->peer_closed)
      return ERROR_PORT_PEER_CLOSED;

    int rv = WillSendMessage_Locked(port, port_ref.name(), m.get());
    if (rv != OK)
      return rv;

    // Beyond this point there's no sense in returning anything but OK. Even if
    // message forwarding or acceptance fails, there's nothing the embedder can
    // do to recover. Assume that failure beyond this point must be treated as a
    // transport failure.

    if (port->peer_node_name != name_) {
      delegate_->ForwardMessage(port->peer_node_name, std::move(m));
      return OK;
    }
  }

  int rv = AcceptMessage(std::move(m));
  if (rv != OK) {
    // See comment above for why we don't return an error in this case.
    DVLOG(2) << "AcceptMessage failed: " << rv;
  }

  return OK;
}

int Node::AcceptMessage(ScopedMessage message) {
  const EventHeader* header = GetEventHeader(*message);
  switch (header->type) {
    case EventType::kUser:
      return OnUserMessage(std::move(message));
    case EventType::kPortAccepted:
      return OnPortAccepted(header->port_name);
    case EventType::kObserveProxy:
      return OnObserveProxy(
          header->port_name,
          *GetEventData<ObserveProxyEventData>(*message));
    case EventType::kObserveProxyAck:
      return OnObserveProxyAck(
          header->port_name,
          GetEventData<ObserveProxyAckEventData>(*message)->last_sequence_num);
    case EventType::kObserveClosure:
      return OnObserveClosure(
          header->port_name,
          GetEventData<ObserveClosureEventData>(*message)->last_sequence_num);
    case EventType::kMergePort:
      return OnMergePort(header->port_name,
                         *GetEventData<MergePortEventData>(*message));
  }
  return OOPS(ERROR_NOT_IMPLEMENTED);
}

int Node::MergePorts(const PortRef& port_ref,
                     const NodeName& destination_node_name,
                     const PortName& destination_port_name) {
  Port* port = port_ref.port();
  {
    // |ports_lock_| must be held for WillSendPort_Locked below.
    base::AutoLock ports_lock(ports_lock_);
    base::AutoLock lock(port->lock);

    DVLOG(1) << "Sending MergePort from " << port_ref.name() << "@" << name_
             << " to " << destination_port_name << "@" << destination_node_name;

    // Send the port-to-merge over to the destination node so it can be merged
    // into the port cycle atomically there.
    MergePortEventData data;
    data.new_port_name = port_ref.name();
    WillSendPort_Locked(port, destination_node_name, &data.new_port_name,
                        &data.new_port_descriptor);
    delegate_->ForwardMessage(
        destination_node_name,
        NewInternalMessage(destination_port_name,
                           EventType::kMergePort, data));
  }
  return OK;
}

int Node::LostConnectionToNode(const NodeName& node_name) {
  // We can no longer send events to the given node. We also can't expect any
  // PortAccepted events.

  DVLOG(1) << "Observing lost connection from node " << name_
           << " to node " << node_name;

  std::vector<PortRef> ports_to_notify;

  {
    base::AutoLock ports_lock(ports_lock_);

    for (auto iter = ports_.begin(); iter != ports_.end(); ) {
      scoped_refptr<Port>& port = iter->second;

      bool remove_port = false;
      {
        base::AutoLock port_lock(port->lock);

        if (port->peer_node_name == node_name) {
          // We can no longer send messages to this port's peer. We assume we
          // will not receive any more messages from this port's peer as well.
          if (!port->peer_closed) {
            port->peer_closed = true;
            port->last_sequence_num_to_receive =
                port->message_queue.next_sequence_num() - 1;

            if (port->state == Port::kReceiving)
              ports_to_notify.push_back(PortRef(iter->first, port));
          }

          // We do not expect to forward any further messages, and we do not
          // expect to receive a Port{Accepted,Rejected} event.
          if (port->state != Port::kReceiving)
            remove_port = true;
        }
      }

      if (remove_port) {
        DVLOG(2) << "Deleted port " << iter->first << "@" << name_;
        iter = ports_.erase(iter);
      } else {
        ++iter;
      }
    }
  }

  for (size_t i = 0; i < ports_to_notify.size(); ++i)
    delegate_->PortStatusChanged(ports_to_notify[i]);

  return OK;
}

int Node::OnUserMessage(ScopedMessage message) {
  PortName port_name = GetEventHeader(*message)->port_name;
  const auto* event = GetEventData<UserEventData>(*message);

#if !defined(NDEBUG)
  std::ostringstream ports_buf;
  for (size_t i = 0; i < message->num_ports(); ++i) {
    if (i > 0)
      ports_buf << ",";
    ports_buf << message->ports()[i];
  }

  DVLOG(2) << "AcceptMessage " << event->sequence_num
             << " [ports=" << ports_buf.str() << "] at "
             << port_name << "@" << name_;
#endif

  scoped_refptr<Port> port = GetPort(port_name);

  // Even if this port does not exist, cannot receive anymore messages or is
  // buffering or proxying messages, we still need these ports to be bound to
  // this node. When the message is forwarded, these ports will get transferred
  // following the usual method. If the message cannot be accepted, then the
  // newly bound ports will simply be closed.

  for (size_t i = 0; i < message->num_ports(); ++i) {
    int rv = AcceptPort(message->ports()[i], GetPortDescriptors(event)[i]);
    if (rv != OK)
      return rv;
  }

  bool has_next_message = false;
  bool message_accepted = false;

  if (port) {
    // We may want to forward messages once the port lock is held, so we must
    // acquire |ports_lock_| first.
    base::AutoLock ports_lock(ports_lock_);
    base::AutoLock lock(port->lock);

    // Reject spurious messages if we've already received the last expected
    // message.
    if (CanAcceptMoreMessages(port.get())) {
      message_accepted = true;
      port->message_queue.AcceptMessage(std::move(message), &has_next_message);

      if (port->state == Port::kBuffering) {
        has_next_message = false;
      } else if (port->state == Port::kProxying) {
        has_next_message = false;

        // Forward messages. We forward messages in sequential order here so
        // that we maintain the message queue's notion of next sequence number.
        // That's useful for the proxy removal process as we can tell when this
        // port has seen all of the messages it is expected to see.
        int rv = ForwardMessages_Locked(port.get(), port_name);
        if (rv != OK)
          return rv;

        MaybeRemoveProxy_Locked(port.get(), port_name);
      }
    }
  }

  if (!message_accepted) {
    DVLOG(2) << "Message not accepted!\n";
    // Close all newly accepted ports as they are effectively orphaned.
    for (size_t i = 0; i < message->num_ports(); ++i) {
      PortRef port_ref;
      if (GetPort(message->ports()[i], &port_ref) == OK) {
        ClosePort(port_ref);
      } else {
        DLOG(WARNING) << "Cannot close non-existent port!\n";
      }
    }
  } else if (has_next_message) {
    PortRef port_ref(port_name, port);
    delegate_->PortStatusChanged(port_ref);
  }

  return OK;
}

int Node::OnPortAccepted(const PortName& port_name) {
  scoped_refptr<Port> port = GetPort(port_name);
  if (!port)
    return OOPS(ERROR_PORT_UNKNOWN);

  {
    // We must hold |ports_lock_| before grabbing the port lock because
    // ForwardMessages_Locked requires it to be held.
    base::AutoLock ports_lock(ports_lock_);
    base::AutoLock lock(port->lock);

    DVLOG(2) << "PortAccepted at " << port_name << "@" << name_
             << " pointing to "
             << port->peer_port_name << "@" << port->peer_node_name;

    return BeginProxying_Locked(port.get(), port_name);
  }
}

int Node::OnObserveProxy(const PortName& port_name,
                         const ObserveProxyEventData& event) {
  // The port may have already been closed locally, in which case the
  // ObserveClosure message will contain the last_sequence_num field.
  // We can then silently ignore this message.
  scoped_refptr<Port> port = GetPort(port_name);
  if (!port) {
    DVLOG(1) << "ObserveProxy: " << port_name << "@" << name_ << " not found";
    return OK;
  }

  DVLOG(2) << "ObserveProxy at " << port_name << "@" << name_ << ", proxy at "
           << event.proxy_port_name << "@"
           << event.proxy_node_name << " pointing to "
           << event.proxy_to_port_name << "@"
           << event.proxy_to_node_name;

  {
    base::AutoLock lock(port->lock);

    if (port->peer_node_name == event.proxy_node_name &&
        port->peer_port_name == event.proxy_port_name) {
      if (port->state == Port::kReceiving) {
        port->peer_node_name = event.proxy_to_node_name;
        port->peer_port_name = event.proxy_to_port_name;

        ObserveProxyAckEventData ack;
        ack.last_sequence_num = port->next_sequence_num_to_send - 1;

        delegate_->ForwardMessage(
            event.proxy_node_name,
            NewInternalMessage(event.proxy_port_name,
                               EventType::kObserveProxyAck,
                               ack));
      } else {
        // As a proxy ourselves, we don't know how to honor the ObserveProxy
        // event or to populate the last_sequence_num field of ObserveProxyAck.
        // Afterall, another port could be sending messages to our peer now
        // that we've sent out our own ObserveProxy event.  Instead, we will
        // send an ObserveProxyAck indicating that the ObserveProxy event
        // should be re-sent (last_sequence_num set to kInvalidSequenceNum).
        // However, this has to be done after we are removed as a proxy.
        // Otherwise, we might just find ourselves back here again, which
        // would be akin to a busy loop.

        DVLOG(2) << "Delaying ObserveProxyAck to "
                 << event.proxy_port_name << "@" << event.proxy_node_name;

        ObserveProxyAckEventData ack;
        ack.last_sequence_num = kInvalidSequenceNum;

        port->send_on_proxy_removal.reset(
            new std::pair<NodeName, ScopedMessage>(
                event.proxy_node_name,
                NewInternalMessage(event.proxy_port_name,
                                   EventType::kObserveProxyAck,
                                   ack)));
      }
    } else {
      // Forward this event along to our peer. Eventually, it should find the
      // port referring to the proxy.
      delegate_->ForwardMessage(
          port->peer_node_name,
          NewInternalMessage(port->peer_port_name,
                             EventType::kObserveProxy,
                             event));
    }
  }
  return OK;
}

int Node::OnObserveProxyAck(const PortName& port_name,
                            uint64_t last_sequence_num) {
  DVLOG(2) << "ObserveProxyAck at " << port_name << "@" << name_
           << " (last_sequence_num=" << last_sequence_num << ")";

  scoped_refptr<Port> port = GetPort(port_name);
  if (!port)
    return ERROR_PORT_UNKNOWN;  // The port may have observed closure first, so
                                // this is not an "Oops".

  {
    // We must acquire |ports_lock_| before the port lock because it must be
    // held for MaybeRemoveProxy_Locked.
    base::AutoLock ports_lock(ports_lock_);

    base::AutoLock lock(port->lock);

    if (port->state != Port::kProxying)
      return OOPS(ERROR_PORT_STATE_UNEXPECTED);

    if (last_sequence_num == kInvalidSequenceNum) {
      // Send again.
      InitiateProxyRemoval_Locked(port.get(), port_name);
      return OK;
    }

    // We can now remove this port once we have received and forwarded the last
    // message addressed to this port.
    port->remove_proxy_on_last_message = true;
    port->last_sequence_num_to_receive = last_sequence_num;

    MaybeRemoveProxy_Locked(port.get(), port_name);
  }
  return OK;
}

int Node::OnObserveClosure(const PortName& port_name,
                           uint64_t last_sequence_num) {
  // OK if the port doesn't exist, as it may have been closed already.
  scoped_refptr<Port> port = GetPort(port_name);
  if (!port)
    return OK;

  // This message tells the port that it should no longer expect more messages
  // beyond last_sequence_num. This message is forwarded along until we reach
  // the receiving end, and this message serves as an equivalent to
  // ObserveProxyAck.

  bool notify_delegate = false;
  {
    // We must acquire |ports_lock_| before the port lock because it must be
    // held for MaybeRemoveProxy_Locked.
    base::AutoLock ports_lock(ports_lock_);

    base::AutoLock lock(port->lock);

    port->peer_closed = true;
    port->last_sequence_num_to_receive = last_sequence_num;

    DVLOG(2) << "ObserveClosure at " << port_name << "@" << name_
             << " (state=" << port->state << ") pointing to "
             << port->peer_port_name << "@" << port->peer_node_name
             << " (last_sequence_num=" << last_sequence_num << ")";

    // We always forward ObserveClosure, even beyond the receiving port which
    // cares about it. This ensures that any dead-end proxies beyond that port
    // are notified to remove themselves.

    ObserveClosureEventData forwarded_data;

    if (port->state == Port::kReceiving) {
      notify_delegate = true;

      // When forwarding along the other half of the port cycle, this will only
      // reach dead-end proxies. Tell them we've sent our last message so they
      // can go away.
      //
      // TODO: Repurposing ObserveClosure for this has the desired result but
      // may be semantically confusing since the forwarding port is not actually
      // closed. Consider replacing this with a new event type.
      forwarded_data.last_sequence_num = port->next_sequence_num_to_send - 1;
    } else {
      // We haven't yet reached the receiving peer of the closed port, so
      // forward the message along as-is.
      forwarded_data.last_sequence_num = last_sequence_num;

      // See about removing the port if it is a proxy as our peer won't be able
      // to participate in proxy removal.
      port->remove_proxy_on_last_message = true;
      if (port->state == Port::kProxying)
        MaybeRemoveProxy_Locked(port.get(), port_name);
    }

    DVLOG(2) << "Forwarding ObserveClosure from "
             << port_name << "@" << name_ << " to peer "
             << port->peer_port_name << "@" << port->peer_node_name
             << " (last_sequence_num=" << forwarded_data.last_sequence_num
             << ")";

    delegate_->ForwardMessage(
        port->peer_node_name,
        NewInternalMessage(port->peer_port_name,
                           EventType::kObserveClosure, forwarded_data));
  }
  if (notify_delegate) {
    PortRef port_ref(port_name, port);
    delegate_->PortStatusChanged(port_ref);
  }
  return OK;
}

int Node::OnMergePort(const PortName& port_name,
                      const MergePortEventData& event) {
  scoped_refptr<Port> port = GetPort(port_name);
  if (!port)
    return ERROR_PORT_UNKNOWN;

  DVLOG(1) << "MergePort at " << port_name << "@" << name_ << " (state="
           << port->state << ") merging with proxy " << event.new_port_name
           << "@" << name_ << " pointing to "
           << event.new_port_descriptor.peer_port_name << "@"
           << event.new_port_descriptor.peer_node_name << " referred by "
           << event.new_port_descriptor.referring_port_name << "@"
           << event.new_port_descriptor.referring_node_name;

  bool close_target_port = false;
  bool close_new_port = false;

  // Accept the new port. This is now the receiving end of the other port cycle
  // to be merged with ours.
  int rv = AcceptPort(event.new_port_name, event.new_port_descriptor);
  if (rv != OK) {
    close_target_port = true;
  } else {
    // BeginProxying_Locked may call MaybeRemoveProxy_Locked, which in turn
    // needs to hold |ports_lock_|. We also acquire multiple port locks within.
    base::AutoLock ports_lock(ports_lock_);
    base::AutoLock lock(port->lock);

    if (port->state != Port::kReceiving) {
      close_new_port = true;
    } else {
      scoped_refptr<Port> new_port = GetPort_Locked(event.new_port_name);
      base::AutoLock new_port_lock(new_port->lock);
      DCHECK(new_port->state == Port::kReceiving);

      // Both ports are locked. Now all we have to do is swap their peer
      // information and set them up as proxies.

      std::swap(port->peer_node_name, new_port->peer_node_name);
      std::swap(port->peer_port_name, new_port->peer_port_name);
      std::swap(port->peer_closed, new_port->peer_closed);

      port->state = Port::kBuffering;
      if (port->peer_closed)
        port->remove_proxy_on_last_message = true;

      new_port->state = Port::kBuffering;
      if (new_port->peer_closed)
        new_port->remove_proxy_on_last_message = true;

      int rv1 = BeginProxying_Locked(port.get(), port_name);
      int rv2 = BeginProxying_Locked(new_port.get(), event.new_port_name);

      if (rv1 == OK && rv2 == OK)
        return OK;

      // If either proxy failed to initialize (e.g. had undeliverable messages
      // or ended up in a bad state somehow), we keep the system in a consistent
      // state by undoing the peer swap and closing both merge ports.

      std::swap(port->peer_node_name, new_port->peer_node_name);
      std::swap(port->peer_port_name, new_port->peer_port_name);
      port->state = Port::kReceiving;
      new_port->state = Port::kReceiving;
      close_new_port = true;
      close_target_port = true;
    }
  }

  if (close_target_port) {
    PortRef target_port;
    rv = GetPort(port_name, &target_port);
    DCHECK(rv == OK);

    ClosePort(target_port);
  }

  if (close_new_port) {
    PortRef new_port;
    rv = GetPort(event.new_port_name, &new_port);
    DCHECK(rv == OK);

    ClosePort(new_port);
  }

  return ERROR_PORT_STATE_UNEXPECTED;
}

int Node::AddPortWithName(const PortName& port_name,
                          const scoped_refptr<Port>& port) {
  base::AutoLock lock(ports_lock_);

  if (!ports_.insert(std::make_pair(port_name, port)).second)
    return OOPS(ERROR_PORT_EXISTS);  // Suggests a bad UUID generator.

  DVLOG(2) << "Created port " << port_name << "@" << name_;
  return OK;
}

void Node::ErasePort(const PortName& port_name) {
  base::AutoLock lock(ports_lock_);
  return ErasePort_Locked(port_name);
}

void Node::ErasePort_Locked(const PortName& port_name) {
  ports_lock_.AssertAcquired();
  ports_.erase(port_name);
  DVLOG(2) << "Deleted port " << port_name << "@" << name_;
}

scoped_refptr<Port> Node::GetPort(const PortName& port_name) {
  base::AutoLock lock(ports_lock_);
  return GetPort_Locked(port_name);
}

scoped_refptr<Port> Node::GetPort_Locked(const PortName& port_name) {
  ports_lock_.AssertAcquired();
  auto iter = ports_.find(port_name);
  if (iter == ports_.end())
    return nullptr;

  return iter->second;
}

void Node::WillSendPort_Locked(Port* port,
                               const NodeName& to_node_name,
                               PortName* port_name,
                               PortDescriptor* port_descriptor) {
  ports_lock_.AssertAcquired();
  port->lock.AssertAcquired();

  PortName local_port_name = *port_name;

  PortName new_port_name;
  delegate_->GenerateRandomPortName(&new_port_name);

  // Make sure we don't send messages to the new peer until after we know it
  // exists. In the meantime, just buffer messages locally.
  DCHECK(port->state == Port::kReceiving);
  port->state = Port::kBuffering;

  // If we already know our peer is closed, we already know this proxy can
  // be removed once it receives and forwards its last expected message.
  if (port->peer_closed)
    port->remove_proxy_on_last_message = true;

  *port_name = new_port_name;

  port_descriptor->peer_node_name = port->peer_node_name;
  port_descriptor->peer_port_name = port->peer_port_name;
  port_descriptor->referring_node_name = name_;
  port_descriptor->referring_port_name = local_port_name;
  port_descriptor->next_sequence_num_to_send = port->next_sequence_num_to_send;
  port_descriptor->next_sequence_num_to_receive =
      port->message_queue.next_sequence_num();
  port_descriptor->last_sequence_num_to_receive =
      port->last_sequence_num_to_receive;
  port_descriptor->peer_closed = port->peer_closed;

  // Configure the local port to point to the new port.
  port->peer_node_name = to_node_name;
  port->peer_port_name = new_port_name;
}

int Node::AcceptPort(const PortName& port_name,
                     const PortDescriptor& port_descriptor) {
  scoped_refptr<Port> port = make_scoped_refptr(
      new Port(port_descriptor.next_sequence_num_to_send,
               port_descriptor.next_sequence_num_to_receive));
  port->state = Port::kReceiving;
  port->peer_node_name = port_descriptor.peer_node_name;
  port->peer_port_name = port_descriptor.peer_port_name;
  port->last_sequence_num_to_receive =
      port_descriptor.last_sequence_num_to_receive;
  port->peer_closed = port_descriptor.peer_closed;

  DVLOG(2) << "Accepting port " << port_name << " [peer_closed="
           << port->peer_closed << "; last_sequence_num_to_receive="
           << port->last_sequence_num_to_receive << "]";

  // A newly accepted port is not signalable until the message referencing the
  // new port finds its way to the consumer (see GetMessageIf).
  port->message_queue.set_signalable(false);

  int rv = AddPortWithName(port_name, port);
  if (rv != OK)
    return rv;

  // Allow referring port to forward messages.
  delegate_->ForwardMessage(
      port_descriptor.referring_node_name,
      NewInternalMessage(port_descriptor.referring_port_name,
                         EventType::kPortAccepted));
  return OK;
}

int Node::WillSendMessage_Locked(Port* port,
                                 const PortName& port_name,
                                 Message* message) {
  ports_lock_.AssertAcquired();
  port->lock.AssertAcquired();

  DCHECK(message);

  // Messages may already have a sequence number if they're being forwarded
  // by a proxy. Otherwise, use the next outgoing sequence number.
  uint64_t* sequence_num =
      &GetMutableEventData<UserEventData>(message)->sequence_num;
  if (*sequence_num == 0)
    *sequence_num = port->next_sequence_num_to_send++;

#if !defined(NDEBUG)
  std::ostringstream ports_buf;
  for (size_t i = 0; i < message->num_ports(); ++i) {
    if (i > 0)
      ports_buf << ",";
    ports_buf << message->ports()[i];
  }
#endif

  if (message->num_ports() > 0) {
    // Note: Another thread could be trying to send the same ports, so we need
    // to ensure that they are ours to send before we mutate their state.

    std::vector<scoped_refptr<Port>> ports;
    ports.resize(message->num_ports());

    {
      for (size_t i = 0; i < message->num_ports(); ++i) {
        ports[i] = GetPort_Locked(message->ports()[i]);
        ports[i]->lock.Acquire();

        int error = OK;
        if (ports[i]->state != Port::kReceiving)
          error = ERROR_PORT_STATE_UNEXPECTED;
        else if (message->ports()[i] == port->peer_port_name)
          error = ERROR_PORT_CANNOT_SEND_PEER;

        if (error != OK) {
          // Oops, we cannot send this port.
          for (size_t j = 0; j <= i; ++j)
            ports[i]->lock.Release();
          // Backpedal on the sequence number.
          port->next_sequence_num_to_send--;
          return error;
        }
      }
    }

    PortDescriptor* port_descriptors =
        GetMutablePortDescriptors(GetMutableEventData<UserEventData>(message));

    for (size_t i = 0; i < message->num_ports(); ++i) {
      WillSendPort_Locked(ports[i].get(),
                          port->peer_node_name,
                          message->mutable_ports() + i,
                          port_descriptors + i);
    }

    for (size_t i = 0; i < message->num_ports(); ++i)
      ports[i]->lock.Release();
  }

#if !defined(NDEBUG)
  DVLOG(2) << "Sending message "
           << GetEventData<UserEventData>(*message)->sequence_num
           << " [ports=" << ports_buf.str() << "]"
           << " from " << port_name << "@" << name_
           << " to " << port->peer_port_name << "@" << port->peer_node_name;
#endif

  GetMutableEventHeader(message)->port_name = port->peer_port_name;
  return OK;
}

int Node::BeginProxying_Locked(Port* port, const PortName& port_name) {
  ports_lock_.AssertAcquired();
  port->lock.AssertAcquired();

  if (port->state != Port::kBuffering)
    return OOPS(ERROR_PORT_STATE_UNEXPECTED);

  port->state = Port::kProxying;

  int rv = ForwardMessages_Locked(port, port_name);
  if (rv != OK)
    return rv;

  // We may have observed closure while buffering. In that case, we can advance
  // to removing the proxy without sending out an ObserveProxy message. We
  // already know the last expected message, etc.

  if (port->remove_proxy_on_last_message) {
    MaybeRemoveProxy_Locked(port, port_name);

    // Make sure we propagate closure to our current peer.
    ObserveClosureEventData data;
    data.last_sequence_num = port->last_sequence_num_to_receive;
    delegate_->ForwardMessage(
        port->peer_node_name,
        NewInternalMessage(port->peer_port_name,
                           EventType::kObserveClosure, data));
  } else {
    InitiateProxyRemoval_Locked(port, port_name);
  }

  return OK;
}

int Node::ForwardMessages_Locked(Port* port, const PortName &port_name) {
  ports_lock_.AssertAcquired();
  port->lock.AssertAcquired();

  for (;;) {
    ScopedMessage message;
    port->message_queue.GetNextMessageIf(nullptr, &message);
    if (!message)
      break;

    int rv = WillSendMessage_Locked(port, port_name, message.get());
    if (rv != OK)
      return rv;

    delegate_->ForwardMessage(port->peer_node_name, std::move(message));
  }
  return OK;
}

void Node::InitiateProxyRemoval_Locked(Port* port,
                                       const PortName& port_name) {
  port->lock.AssertAcquired();

  // To remove this node, we start by notifying the connected graph that we are
  // a proxy. This allows whatever port is referencing this node to skip it.
  // Eventually, this node will receive ObserveProxyAck (or ObserveClosure if
  // the peer was closed in the meantime).

  ObserveProxyEventData data;
  data.proxy_node_name = name_;
  data.proxy_port_name = port_name;
  data.proxy_to_node_name = port->peer_node_name;
  data.proxy_to_port_name = port->peer_port_name;

  delegate_->ForwardMessage(
      port->peer_node_name,
      NewInternalMessage(port->peer_port_name, EventType::kObserveProxy, data));
}

void Node::MaybeRemoveProxy_Locked(Port* port,
                                   const PortName& port_name) {
  // |ports_lock_| must be held so we can potentilaly ErasePort_Locked().
  ports_lock_.AssertAcquired();
  port->lock.AssertAcquired();

  DCHECK(port->state == Port::kProxying);

  // Make sure we have seen ObserveProxyAck before removing the port.
  if (!port->remove_proxy_on_last_message)
    return;

  if (!CanAcceptMoreMessages(port)) {
    // This proxy port is done. We can now remove it!
    ErasePort_Locked(port_name);

    if (port->send_on_proxy_removal) {
      NodeName to_node = port->send_on_proxy_removal->first;
      ScopedMessage& message = port->send_on_proxy_removal->second;

      delegate_->ForwardMessage(to_node, std::move(message));
    }
  } else {
    DVLOG(2) << "Cannot remove port " << port_name << "@" << name_
             << " now; waiting for more messages";
  }
}

ScopedMessage Node::NewInternalMessage_Helper(const PortName& port_name,
                                              const EventType& type,
                                              const void* data,
                                              size_t num_data_bytes) {
  ScopedMessage message;
  delegate_->AllocMessage(sizeof(EventHeader) + num_data_bytes, &message);

  EventHeader* header = GetMutableEventHeader(message.get());
  header->port_name = port_name;
  header->type = type;
  header->padding = 0;

  if (num_data_bytes)
    memcpy(header + 1, data, num_data_bytes);

  return message;
}

}  // namespace ports
}  // namespace edk
}  // namespace mojo
