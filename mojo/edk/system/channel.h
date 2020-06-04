// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_CHANNEL_H_
#define MOJO_EDK_SYSTEM_CHANNEL_H_

#include <stdint.h>

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/channel_endpoint.h"
#include "mojo/edk/system/channel_endpoint_id.h"
#include "mojo/edk/system/message_in_transit.h"
#include "mojo/edk/system/message_pipe.h"
#include "mojo/edk/system/raw_channel.h"
#include "mojo/edk/system/system_impl_export.h"
#include "mojo/public/c/system/types.h"

namespace mojo {

namespace embedder {
class PlatformSupport;
}

namespace system {

class ChannelEndpoint;

// This class is mostly thread-safe. It must be created on an I/O thread.
// |Init()| must be called on that same thread before it becomes thread-safe (in
// particular, before references are given to any other thread) and |Shutdown()|
// must be called on that same thread before destruction. Its public methods are
// otherwise thread-safe. (Many private methods are restricted to the creation
// thread.) It may be destroyed on any thread, in the sense that the last
// reference to it may be released on any thread, with the proviso that
// |Shutdown()| must have been called first (so the pattern is that a "main"
// reference is kept on its creation thread and is released after |Shutdown()|
// is called, but other threads may have temporarily "dangling" references).
//
// Note the lock order (in order of allowable acquisition): |MessagePipe|,
// |ChannelEndpoint|, |Channel|. Thus |Channel| may not call into
// |ChannelEndpoint| with |Channel|'s lock held.
class MOJO_SYSTEM_IMPL_EXPORT Channel
    : public base::RefCountedThreadSafe<Channel>,
      public RawChannel::Delegate {
 public:
  // |platform_support| (typically owned by |Core|) must remain alive until
  // after |Shutdown()| is called.
  explicit Channel(embedder::PlatformSupport* platform_support);

  // This must be called on the creation thread before any other methods are
  // called, and before references to this object are given to any other
  // threads. |raw_channel| should be uninitialized. Returns true on success. On
  // failure, no other methods should be called (including |Shutdown()|).
  bool Init(scoped_ptr<RawChannel> raw_channel);

  // This must be called on the creation thread before destruction (which can
  // happen on any thread).
  void Shutdown();

  // Signals that |Shutdown()| will be called soon (this may be called from any
  // thread, unlike |Shutdown()|). Warnings will be issued if, e.g., messages
  // are written after this is called; other warnings may be suppressed. (This
  // may be called multiple times, or not at all.)
  void WillShutdownSoon();

  // Attaches the given endpoint to this channel. This assigns it a local ID,
  // which it returns. The first endpoint attached will always have
  // |kBootstrapChannelEndpointId| as its local ID. (For bootstrapping, this
  // occurs on both sides, so one should use |kBootstrapChannelEndpointId| for
  // the remote ID for the first message pipe across a channel.) Returns
  // |kInvalidChannelEndpointId| on failure.
  // TODO(vtl): This should be combined with "run", and it should take a
  // |ChannelEndpoint| instead.
  // TODO(vtl): Maybe limit the number of attached message pipes.
  ChannelEndpointId AttachEndpoint(scoped_refptr<ChannelEndpoint> endpoint);

  // Runs the given endpoint (which must have been attached to this |Channel|,
  // and not detached), assigning it the specified |remote_id|.
  void RunEndpoint(scoped_refptr<ChannelEndpoint> endpoint,
                   ChannelEndpointId remote_id);

  // Tells the other side of the channel to run a message pipe endpoint (which
  // must already be attached); |local_id| and |remote_id| are relative to this
  // channel (i.e., |local_id| is the other side's remote ID and |remote_id| is
  // its local ID).
  // TODO(vtl): Maybe we should just have a flag argument to
  // |RunMessagePipeEndpoint()| that tells it to do this.
  void RunRemoteMessagePipeEndpoint(ChannelEndpointId local_id,
                                    ChannelEndpointId remote_id);

  // This forwards |message| verbatim to |raw_channel_|.
  bool WriteMessage(scoped_ptr<MessageInTransit> message);

  // See |RawChannel::IsWriteBufferEmpty()|.
  // TODO(vtl): Maybe we shouldn't expose this, and instead have a
  // |FlushWriteBufferAndShutdown()| or something like that.
  bool IsWriteBufferEmpty();

  // This removes the given endpoint from this channel (|local_id| and
  // |remote_id| are specified as an optimization; the latter should be
  // |kInvalidChannelEndpointId| if the endpoint is not yet running). Note: If
  // this is called, the |Channel| will *not* call
  // |ChannelEndpoint::DetachFromChannel()|.
  void DetachEndpoint(ChannelEndpoint* endpoint,
                      ChannelEndpointId local_id,
                      ChannelEndpointId remote_id);

  // See |RawChannel::GetSerializedPlatformHandleSize()|.
  size_t GetSerializedPlatformHandleSize() const;

  embedder::PlatformSupport* platform_support() const {
    return platform_support_;
  }

 private:
  friend class base::RefCountedThreadSafe<Channel>;
  virtual ~Channel();

  // |RawChannel::Delegate| implementation (only called on the creation thread):
  virtual void OnReadMessage(
      const MessageInTransit::View& message_view,
      embedder::ScopedPlatformHandleVectorPtr platform_handles) override;
  virtual void OnError(Error error) override;

  // Helpers for |OnReadMessage| (only called on the creation thread):
  void OnReadMessageForDownstream(
      const MessageInTransit::View& message_view,
      embedder::ScopedPlatformHandleVectorPtr platform_handles);
  void OnReadMessageForChannel(
      const MessageInTransit::View& message_view,
      embedder::ScopedPlatformHandleVectorPtr platform_handles);

  // Handles "run message pipe endpoint" messages.
  bool OnRunMessagePipeEndpoint(ChannelEndpointId local_id,
                                ChannelEndpointId remote_id);
  // Handles "remove message pipe endpoint" messages.
  bool OnRemoveMessagePipeEndpoint(ChannelEndpointId local_id,
                                   ChannelEndpointId remote_id);
  // Handles "remove message pipe endpoint ack" messages.
  bool OnRemoveMessagePipeEndpointAck(ChannelEndpointId local_id);

  // Handles errors (e.g., invalid messages) from the remote side. Callable from
  // any thread.
  void HandleRemoteError(const base::StringPiece& error_message);
  // Handles internal errors/failures from the local side. Callable from any
  // thread.
  void HandleLocalError(const base::StringPiece& error_message);

  // Helper to send channel control messages. Returns true on success. Should be
  // called *without* |lock_| held. Callable from any thread.
  bool SendControlMessage(MessageInTransit::Subtype subtype,
                          ChannelEndpointId source_id,
                          ChannelEndpointId destination_id);

  base::ThreadChecker creation_thread_checker_;

  embedder::PlatformSupport* const platform_support_;

  // Note: |MessagePipe|s MUST NOT be used under |lock_|. I.e., |lock_| can only
  // be acquired after |MessagePipe::lock_|, never before. Thus to call into a
  // |MessagePipe|, a reference to the |MessagePipe| should be acquired from
  // |local_id_to_endpoint_map_| under |lock_| and then the lock released.
  base::Lock lock_;  // Protects the members below.

  scoped_ptr<RawChannel> raw_channel_;
  bool is_running_;
  // Set when |WillShutdownSoon()| is called.
  bool is_shutting_down_;

  typedef base::hash_map<ChannelEndpointId, scoped_refptr<ChannelEndpoint>>
      IdToEndpointMap;
  // Map from local IDs to endpoints. If the endpoint is null, this means that
  // we're just waiting for the remove ack before removing the entry.
  IdToEndpointMap local_id_to_endpoint_map_;
  // The next local ID to try (when allocating new local IDs). Note: It should
  // be checked for existence before use.
  ChannelEndpointId next_local_id_;

  DISALLOW_COPY_AND_ASSIGN(Channel);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_CHANNEL_H_
