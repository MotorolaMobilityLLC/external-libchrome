// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_sync_channel.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "ipc/ipc_channel_factory.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_sync_message.h"
#include "mojo/public/cpp/bindings/sync_event_watcher.h"

using base::WaitableEvent;

namespace IPC {

namespace {

// A generic callback used when watching handles synchronously. Sets |*signal|
// to true.
void OnEventReady(bool* signal) {
  *signal = true;
}

base::LazyInstance<std::unique_ptr<base::WaitableEvent>>::Leaky
    g_pump_messages_event = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// When we're blocked in a Send(), we need to process incoming synchronous
// messages right away because it could be blocking our reply (either
// directly from the same object we're calling, or indirectly through one or
// more other channels).  That means that in SyncContext's OnMessageReceived,
// we need to process sync message right away if we're blocked.  However a
// simple check isn't sufficient, because the listener thread can be in the
// process of calling Send.
// To work around this, when SyncChannel filters a sync message, it sets
// an event that the listener thread waits on during its Send() call.  This
// allows us to dispatch incoming sync messages when blocked.  The race
// condition is handled because if Send is in the process of being called, it
// will check the event.  In case the listener thread isn't sending a message,
// we queue a task on the listener thread to dispatch the received messages.
// The messages are stored in this queue object that's shared among all
// SyncChannel objects on the same thread (since one object can receive a
// sync message while another one is blocked).

class SyncChannel::ReceivedSyncMsgQueue :
    public base::RefCountedThreadSafe<ReceivedSyncMsgQueue> {
 public:
  // SyncChannel::WaitForReplyWithNestedMessageLoop may be re-entered, i.e. we
  // may nest waiting message loops arbitrarily deep on the SyncChannel's
  // thread. Every such operation has a corresponding WaitableEvent to be
  // watched which, when signalled for IPC completion, breaks out of the loop.
  // A reference to the innermost (i.e. topmost) watcher is held in
  // |ReceivedSyncMsgQueue::top_send_done_event_watcher_|.
  //
  // NestedSendDoneWatcher provides a simple scoper which is used by
  // WaitForReplyWithNestedMessageLoop to begin watching a new local "send done"
  // event, preserving the previous topmost state on the local stack until the
  // new inner loop is broken. If yet another subsequent nested loop is started
  // therein the process is repeated again in the new inner stack frame, and so
  // on.
  //
  // When this object is destroyed on stack unwind, the previous topmost state
  // is swapped back into |ReceivedSyncMsgQueue::top_send_done_event_watcher_|,
  // and its watch is resumed immediately.
  class NestedSendDoneWatcher {
   public:
    NestedSendDoneWatcher(SyncChannel::SyncContext* context,
                          base::RunLoop* run_loop,
                          scoped_refptr<base::SequencedTaskRunner> task_runner)
        : sync_msg_queue_(context->received_sync_msgs()),
          outer_state_(sync_msg_queue_->top_send_done_event_watcher_),
          event_(context->GetSendDoneEvent()),
          callback_(
              base::BindOnce(&SyncChannel::SyncContext::OnSendDoneEventSignaled,
                             context,
                             run_loop)),
          task_runner_(std::move(task_runner)) {
      sync_msg_queue_->top_send_done_event_watcher_ = this;
      if (outer_state_)
        outer_state_->StopWatching();
      StartWatching();
    }

    ~NestedSendDoneWatcher() {
      sync_msg_queue_->top_send_done_event_watcher_ = outer_state_;
      if (outer_state_)
        outer_state_->StartWatching();
    }

   private:
    void Run(WaitableEvent* event) {
      DCHECK(callback_);
      std::move(callback_).Run(event);
    }

    void StartWatching() {
      watcher_.StartWatching(
          event_,
          base::BindOnce(&NestedSendDoneWatcher::Run, base::Unretained(this)),
          task_runner_);
    }

    void StopWatching() { watcher_.StopWatching(); }

    ReceivedSyncMsgQueue* const sync_msg_queue_;
    NestedSendDoneWatcher* const outer_state_;

    base::WaitableEvent* const event_;
    base::WaitableEventWatcher::EventCallback callback_;
    base::WaitableEventWatcher watcher_;
    scoped_refptr<base::SequencedTaskRunner> task_runner_;

    DISALLOW_COPY_AND_ASSIGN(NestedSendDoneWatcher);
  };

  // Returns the ReceivedSyncMsgQueue instance for this thread, creating one
  // if necessary.  Call RemoveContext on the same thread when done.
  static ReceivedSyncMsgQueue* AddContext() {
    // We want one ReceivedSyncMsgQueue per listener thread (i.e. since multiple
    // SyncChannel objects can block the same thread).
    ReceivedSyncMsgQueue* rv = lazy_tls_ptr_.Pointer()->Get();
    if (!rv) {
      rv = new ReceivedSyncMsgQueue();
      ReceivedSyncMsgQueue::lazy_tls_ptr_.Pointer()->Set(rv);
    }
    rv->listener_count_++;
    return rv;
  }

  // Prevents messages from being dispatched immediately when the dispatch event
  // is signaled. Instead, |*dispatch_flag| will be set.
  void BlockDispatch(bool* dispatch_flag) { dispatch_flag_ = dispatch_flag; }

  // Allows messages to be dispatched immediately when the dispatch event is
  // signaled.
  void UnblockDispatch() { dispatch_flag_ = nullptr; }

  // Called on IPC thread when a synchronous message or reply arrives.
  void QueueMessage(const Message& msg, SyncChannel::SyncContext* context) {
    bool was_task_pending;
    {
      base::AutoLock auto_lock(message_lock_);

      was_task_pending = task_pending_;
      task_pending_ = true;

      // We set the event in case the listener thread is blocked (or is about
      // to). In case it's not, the PostTask dispatches the messages.
      message_queue_.push_back(QueuedMessage(new Message(msg), context));
      message_queue_version_++;
    }

    dispatch_event_.Signal();
    if (!was_task_pending) {
      listener_task_runner_->PostTask(
          FROM_HERE, base::Bind(&ReceivedSyncMsgQueue::DispatchMessagesTask,
                                this, base::RetainedRef(context)));
    }
  }

  void QueueReply(const Message &msg, SyncChannel::SyncContext* context) {
    received_replies_.push_back(QueuedMessage(new Message(msg), context));
  }

  // Called on the listener's thread to process any queues synchronous
  // messages.
  void DispatchMessagesTask(SyncContext* context) {
    {
      base::AutoLock auto_lock(message_lock_);
      task_pending_ = false;
    }
    context->DispatchMessages();
  }

  // Dispatches any queued incoming sync messages. If |dispatching_context| is
  // not null, messages which target a restricted dispatch channel will only be
  // dispatched if |dispatching_context| belongs to the same restricted dispatch
  // group as that channel. If |dispatching_context| is null, all queued
  // messages are dispatched.
  void DispatchMessages(SyncContext* dispatching_context) {
    bool first_time = true;
    uint32_t expected_version = 0;
    SyncMessageQueue::iterator it;
    while (true) {
      Message* message = nullptr;
      scoped_refptr<SyncChannel::SyncContext> context;
      {
        base::AutoLock auto_lock(message_lock_);
        if (first_time || message_queue_version_ != expected_version) {
          it = message_queue_.begin();
          first_time = false;
        }
        for (; it != message_queue_.end(); it++) {
          int message_group = it->context->restrict_dispatch_group();
          if (message_group == kRestrictDispatchGroup_None ||
              (dispatching_context &&
               message_group ==
                   dispatching_context->restrict_dispatch_group())) {
            message = it->message;
            context = it->context;
            it = message_queue_.erase(it);
            message_queue_version_++;
            expected_version = message_queue_version_;
            break;
          }
        }
      }

      if (message == nullptr)
        break;
      context->OnDispatchMessage(*message);
      delete message;
    }
  }

  // SyncChannel calls this in its destructor.
  void RemoveContext(SyncContext* context) {
    base::AutoLock auto_lock(message_lock_);

    SyncMessageQueue::iterator iter = message_queue_.begin();
    while (iter != message_queue_.end()) {
      if (iter->context.get() == context) {
        delete iter->message;
        iter = message_queue_.erase(iter);
        message_queue_version_++;
      } else {
        iter++;
      }
    }

    if (--listener_count_ == 0) {
      DCHECK(lazy_tls_ptr_.Pointer()->Get());
      lazy_tls_ptr_.Pointer()->Set(nullptr);
      sync_dispatch_watcher_.reset();
    }
  }

  base::WaitableEvent* dispatch_event() { return &dispatch_event_; }
  base::SingleThreadTaskRunner* listener_task_runner() {
    return listener_task_runner_.get();
  }

  // Holds a pointer to the per-thread ReceivedSyncMsgQueue object.
  static base::LazyInstance<base::ThreadLocalPointer<ReceivedSyncMsgQueue>>::
      DestructorAtExit lazy_tls_ptr_;

  // Called on the ipc thread to check if we can unblock any current Send()
  // calls based on a queued reply.
  void DispatchReplies() {
    for (size_t i = 0; i < received_replies_.size(); ++i) {
      Message* message = received_replies_[i].message;
      if (received_replies_[i].context->TryToUnblockListener(message)) {
        delete message;
        received_replies_.erase(received_replies_.begin() + i);
        return;
      }
    }
  }

 private:
  friend class base::RefCountedThreadSafe<ReceivedSyncMsgQueue>;

  // See the comment in SyncChannel::SyncChannel for why this event is created
  // as manual reset.
  ReceivedSyncMsgQueue()
      : message_queue_version_(0),
        dispatch_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                        base::WaitableEvent::InitialState::NOT_SIGNALED),
        listener_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        sync_dispatch_watcher_(std::make_unique<mojo::SyncEventWatcher>(
            &dispatch_event_,
            base::Bind(&ReceivedSyncMsgQueue::OnDispatchEventReady,
                       base::Unretained(this)))) {
    sync_dispatch_watcher_->AllowWokenUpBySyncWatchOnSameThread();
  }

  ~ReceivedSyncMsgQueue() = default;

  void OnDispatchEventReady() {
    if (dispatch_flag_) {
      *dispatch_flag_ = true;
      return;
    }

    // We were woken up during a sync wait, but no specific SyncChannel is
    // currently waiting. i.e., some other Mojo interface on this thread is
    // waiting for a response. Since we don't support anything analogous to
    // restricted dispatch on Mojo interfaces, in this case it's safe to
    // dispatch sync messages for any context.
    DispatchMessages(nullptr);
  }

  // Holds information about a queued synchronous message or reply.
  struct QueuedMessage {
    QueuedMessage(Message* m, SyncContext* c) : message(m), context(c) { }
    Message* message;
    scoped_refptr<SyncChannel::SyncContext> context;
  };

  typedef std::list<QueuedMessage> SyncMessageQueue;
  SyncMessageQueue message_queue_;

  // Used to signal DispatchMessages to rescan
  uint32_t message_queue_version_ = 0;

  std::vector<QueuedMessage> received_replies_;

  // Signaled when we get a synchronous message that we must respond to, as the
  // sender needs its reply before it can reply to our original synchronous
  // message.
  base::WaitableEvent dispatch_event_;
  scoped_refptr<base::SingleThreadTaskRunner> listener_task_runner_;
  base::Lock message_lock_;
  bool task_pending_ = false;
  int listener_count_ = 0;

  // The current NestedSendDoneWatcher for this thread, if we're currently
  // in a SyncChannel::WaitForReplyWithNestedMessageLoop. See
  // NestedSendDoneWatcher comments for more details.
  NestedSendDoneWatcher* top_send_done_event_watcher_ = nullptr;

  // If not null, the address of a flag to set when the dispatch event signals,
  // in lieu of actually dispatching messages. This is used by
  // SyncChannel::WaitForReply to restrict the scope of queued messages we're
  // allowed to process while it's waiting.
  bool* dispatch_flag_ = nullptr;

  // Watches |dispatch_event_| during all sync handle watches on this thread.
  std::unique_ptr<mojo::SyncEventWatcher> sync_dispatch_watcher_;
};

base::LazyInstance<base::ThreadLocalPointer<
    SyncChannel::ReceivedSyncMsgQueue>>::DestructorAtExit
    SyncChannel::ReceivedSyncMsgQueue::lazy_tls_ptr_ =
        LAZY_INSTANCE_INITIALIZER;

SyncChannel::SyncContext::SyncContext(
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& listener_task_runner,
    WaitableEvent* shutdown_event)
    : ChannelProxy::Context(listener, ipc_task_runner, listener_task_runner),
      received_sync_msgs_(ReceivedSyncMsgQueue::AddContext()),
      shutdown_event_(shutdown_event),
      restrict_dispatch_group_(kRestrictDispatchGroup_None) {}

void SyncChannel::SyncContext::OnSendDoneEventSignaled(
    base::RunLoop* nested_loop,
    base::WaitableEvent* event) {
  DCHECK_EQ(GetSendDoneEvent(), event);
  nested_loop->Quit();
}

SyncChannel::SyncContext::~SyncContext() {
  while (!deserializers_.empty())
    Pop();
}

// Adds information about an outgoing sync message to the context so that
// we know how to deserialize the reply. Returns |true| if the message was added
// to the context or |false| if it was rejected (e.g. due to shutdown.)
bool SyncChannel::SyncContext::Push(SyncMessage* sync_msg) {
  // Create the tracking information for this message. This object is stored
  // by value since all members are pointers that are cheap to copy. These
  // pointers are cleaned up in the Pop() function.
  //
  // The event is created as manual reset because in between Signal and
  // OnObjectSignalled, another Send can happen which would stop the watcher
  // from being called.  The event would get watched later, when the nested
  // Send completes, so the event will need to remain set.
  base::AutoLock auto_lock(deserializers_lock_);
  if (reject_new_deserializers_)
    return false;
  PendingSyncMsg pending(
      SyncMessage::GetMessageId(*sync_msg), sync_msg->GetReplyDeserializer(),
      new base::WaitableEvent(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED));
  deserializers_.push_back(pending);
  return true;
}

bool SyncChannel::SyncContext::Pop() {
  bool result;
  {
    base::AutoLock auto_lock(deserializers_lock_);
    PendingSyncMsg msg = deserializers_.back();
    delete msg.deserializer;
    delete msg.done_event;
    msg.done_event = nullptr;
    deserializers_.pop_back();
    result = msg.send_result;
  }

  // We got a reply to a synchronous Send() call that's blocking the listener
  // thread.  However, further down the call stack there could be another
  // blocking Send() call, whose reply we received after we made this last
  // Send() call.  So check if we have any queued replies available that
  // can now unblock the listener thread.
  ipc_task_runner()->PostTask(
      FROM_HERE, base::Bind(&ReceivedSyncMsgQueue::DispatchReplies,
                            received_sync_msgs_));

  return result;
}

base::WaitableEvent* SyncChannel::SyncContext::GetSendDoneEvent() {
  base::AutoLock auto_lock(deserializers_lock_);
  return deserializers_.back().done_event;
}

base::WaitableEvent* SyncChannel::SyncContext::GetDispatchEvent() {
  return received_sync_msgs_->dispatch_event();
}

void SyncChannel::SyncContext::DispatchMessages() {
  received_sync_msgs_->DispatchMessages(this);
}

bool SyncChannel::SyncContext::TryToUnblockListener(const Message* msg) {
  base::AutoLock auto_lock(deserializers_lock_);
  if (deserializers_.empty() ||
      !SyncMessage::IsMessageReplyTo(*msg, deserializers_.back().id)) {
    return false;
  }

  if (!msg->is_reply_error()) {
    bool send_result = deserializers_.back().deserializer->
        SerializeOutputParameters(*msg);
    deserializers_.back().send_result = send_result;
    DVLOG_IF(1, !send_result) << "Couldn't deserialize reply message";
  } else {
    DVLOG(1) << "Received error reply";
  }

  base::WaitableEvent* done_event = deserializers_.back().done_event;
  TRACE_EVENT_FLOW_BEGIN0(
      TRACE_DISABLED_BY_DEFAULT("ipc.flow"),
      "SyncChannel::SyncContext::TryToUnblockListener", done_event);

  done_event->Signal();

  return true;
}

void SyncChannel::SyncContext::Clear() {
  CancelPendingSends();
  received_sync_msgs_->RemoveContext(this);
  Context::Clear();
}

bool SyncChannel::SyncContext::OnMessageReceived(const Message& msg) {
  // Give the filters a chance at processing this message.
  if (TryFilters(msg))
    return true;

  if (TryToUnblockListener(&msg))
    return true;

  if (msg.is_reply()) {
    received_sync_msgs_->QueueReply(msg, this);
    return true;
  }

  if (msg.should_unblock()) {
    received_sync_msgs_->QueueMessage(msg, this);
    return true;
  }

  return Context::OnMessageReceivedNoFilter(msg);
}

void SyncChannel::SyncContext::OnChannelError() {
  CancelPendingSends();
  shutdown_watcher_.StopWatching();
  Context::OnChannelError();
}

void SyncChannel::SyncContext::OnChannelOpened() {
  shutdown_watcher_.StartWatching(
      shutdown_event_,
      base::Bind(&SyncChannel::SyncContext::OnShutdownEventSignaled,
                 base::Unretained(this)),
      ipc_task_runner());
  Context::OnChannelOpened();
}

void SyncChannel::SyncContext::OnChannelClosed() {
  CancelPendingSends();
  shutdown_watcher_.StopWatching();
  Context::OnChannelClosed();
}

void SyncChannel::SyncContext::CancelPendingSends() {
  base::AutoLock auto_lock(deserializers_lock_);
  reject_new_deserializers_ = true;
  PendingSyncMessageQueue::iterator iter;
  DVLOG(1) << "Canceling pending sends";
  for (iter = deserializers_.begin(); iter != deserializers_.end(); iter++) {
    TRACE_EVENT_FLOW_BEGIN0(TRACE_DISABLED_BY_DEFAULT("ipc.flow"),
                            "SyncChannel::SyncContext::CancelPendingSends",
                            iter->done_event);
    iter->done_event->Signal();
  }
}

void SyncChannel::SyncContext::OnShutdownEventSignaled(WaitableEvent* event) {
  DCHECK_EQ(event, shutdown_event_);

  // Process shut down before we can get a reply to a synchronous message.
  // Cancel pending Send calls, which will end up setting the send done event.
  CancelPendingSends();
}

// static
std::unique_ptr<SyncChannel> SyncChannel::Create(
    const IPC::ChannelHandle& channel_handle,
    Channel::Mode mode,
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& listener_task_runner,
    bool create_pipe_now,
    base::WaitableEvent* shutdown_event) {
  std::unique_ptr<SyncChannel> channel =
      Create(listener, ipc_task_runner, listener_task_runner, shutdown_event);
  channel->Init(channel_handle, mode, create_pipe_now);
  return channel;
}

// static
std::unique_ptr<SyncChannel> SyncChannel::Create(
    std::unique_ptr<ChannelFactory> factory,
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& listener_task_runner,
    bool create_pipe_now,
    base::WaitableEvent* shutdown_event) {
  std::unique_ptr<SyncChannel> channel =
      Create(listener, ipc_task_runner, listener_task_runner, shutdown_event);
  channel->Init(std::move(factory), create_pipe_now);
  return channel;
}

// static
std::unique_ptr<SyncChannel> SyncChannel::Create(
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& listener_task_runner,
    WaitableEvent* shutdown_event) {
  return base::WrapUnique(new SyncChannel(
      listener, ipc_task_runner, listener_task_runner, shutdown_event));
}

SyncChannel::SyncChannel(
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& listener_task_runner,
    WaitableEvent* shutdown_event)
    : ChannelProxy(new SyncContext(listener,
                                   ipc_task_runner,
                                   listener_task_runner,
                                   shutdown_event)),
      sync_handle_registry_(mojo::SyncHandleRegistry::current()) {
  // The current (listener) thread must be distinct from the IPC thread, or else
  // sending synchronous messages will deadlock.
  DCHECK_NE(ipc_task_runner.get(), base::ThreadTaskRunnerHandle::Get().get());
  StartWatching();
}

SyncChannel::~SyncChannel() = default;

void SyncChannel::SetRestrictDispatchChannelGroup(int group) {
  sync_context()->set_restrict_dispatch_group(group);
}

scoped_refptr<SyncMessageFilter> SyncChannel::CreateSyncMessageFilter() {
  scoped_refptr<SyncMessageFilter> filter = new SyncMessageFilter(
      sync_context()->shutdown_event());
  AddFilter(filter.get());
  if (!did_init())
    pre_init_sync_message_filters_.push_back(filter);
  return filter;
}

bool SyncChannel::Send(Message* message) {
#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  std::string name;
  Logging::GetInstance()->GetMessageText(
      message->type(), &name, message, nullptr);
  TRACE_EVENT1("ipc", "SyncChannel::Send", "name", name);
#else
  TRACE_EVENT2("ipc", "SyncChannel::Send",
               "class", IPC_MESSAGE_ID_CLASS(message->type()),
               "line", IPC_MESSAGE_ID_LINE(message->type()));
#endif
  if (!message->is_sync()) {
    ChannelProxy::SendInternal(message);
    return true;
  }

  SyncMessage* sync_msg = static_cast<SyncMessage*>(message);
  bool pump_messages = sync_msg->ShouldPumpMessages();

  // *this* might get deleted in WaitForReply.
  scoped_refptr<SyncContext> context(sync_context());
  if (!context->Push(sync_msg)) {
    DVLOG(1) << "Channel is shutting down. Dropping sync message.";
    delete message;
    return false;
  }

  ChannelProxy::SendInternal(message);

  // Wait for reply, or for any other incoming synchronous messages.
  // |this| might get deleted, so only call static functions at this point.
  scoped_refptr<mojo::SyncHandleRegistry> registry = sync_handle_registry_;
  WaitForReply(registry.get(), context.get(), pump_messages);

  TRACE_EVENT_FLOW_END0(TRACE_DISABLED_BY_DEFAULT("ipc.flow"),
                        "SyncChannel::Send", context->GetSendDoneEvent());

  return context->Pop();
}

void SyncChannel::WaitForReply(mojo::SyncHandleRegistry* registry,
                               SyncContext* context,
                               bool pump_messages) {
  context->DispatchMessages();

  base::WaitableEvent* pump_messages_event = nullptr;
  if (pump_messages) {
    if (!g_pump_messages_event.Get()) {
      g_pump_messages_event.Get() = std::make_unique<base::WaitableEvent>(
          base::WaitableEvent::ResetPolicy::MANUAL,
          base::WaitableEvent::InitialState::SIGNALED);
    }
    pump_messages_event = g_pump_messages_event.Get().get();
  }

  while (true) {
    bool dispatch = false;
    bool send_done = false;
    bool should_pump_messages = false;
    base::Closure on_send_done_callback = base::Bind(&OnEventReady, &send_done);
    registry->RegisterEvent(context->GetSendDoneEvent(), on_send_done_callback);

    base::Closure on_pump_messages_callback;
    if (pump_messages_event) {
      on_pump_messages_callback =
          base::Bind(&OnEventReady, &should_pump_messages);
      registry->RegisterEvent(pump_messages_event, on_pump_messages_callback);
    }

    const bool* stop_flags[] = { &dispatch, &send_done, &should_pump_messages };
    context->received_sync_msgs()->BlockDispatch(&dispatch);
    registry->Wait(stop_flags, 3);
    context->received_sync_msgs()->UnblockDispatch();

    registry->UnregisterEvent(context->GetSendDoneEvent(),
                              on_send_done_callback);
    if (pump_messages_event)
      registry->UnregisterEvent(pump_messages_event, on_pump_messages_callback);

    if (dispatch) {
      // We're waiting for a reply, but we received a blocking synchronous call.
      // We must process it to avoid potential deadlocks.
      context->GetDispatchEvent()->Reset();
      context->DispatchMessages();
      continue;
    }

    if (should_pump_messages)
      WaitForReplyWithNestedMessageLoop(context);  // Run a nested run loop.

    break;
  }
}

void SyncChannel::WaitForReplyWithNestedMessageLoop(SyncContext* context) {
  base::RunLoop nested_loop(base::RunLoop::Type::kNestableTasksAllowed);
  ReceivedSyncMsgQueue::NestedSendDoneWatcher watcher(
      context, &nested_loop, context->listener_task_runner());
  nested_loop.Run();
}

void SyncChannel::OnDispatchEventSignaled(base::WaitableEvent* event) {
  DCHECK_EQ(sync_context()->GetDispatchEvent(), event);
  sync_context()->GetDispatchEvent()->Reset();

  StartWatching();

  // NOTE: May delete |this|.
  sync_context()->DispatchMessages();
}

void SyncChannel::StartWatching() {
  // |dispatch_watcher_| watches the event asynchronously, only dispatching
  // messages once the listener thread is unblocked and pumping its task queue.
  // The ReceivedSyncMsgQueue also watches this event and may dispatch
  // immediately if woken up by a message which it's allowed to dispatch.
  dispatch_watcher_.StartWatching(
      sync_context()->GetDispatchEvent(),
      base::BindOnce(&SyncChannel::OnDispatchEventSignaled,
                     base::Unretained(this)),
      sync_context()->listener_task_runner());
}

void SyncChannel::OnChannelInit() {
  pre_init_sync_message_filters_.clear();
}

}  // namespace IPC
