// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_ROUTED_RAW_CHANNEL_H_
#define MOJO_EDK_SYSTEM_ROUTED_RAW_CHANNEL_H_

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "mojo/edk/embedder/platform_handle_vector.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/broker.h"
#include "mojo/edk/system/raw_channel.h"
#include "mojo/edk/system/system_impl_export.h"

namespace mojo {
namespace edk {
class RawChannel;

// This class wraps a RawChannel and adds routing on top of it.
// Non-transferable MessagePipeDispatchers call here, indirectly through the
// Broker interface, to associate with their pipe_id.
class RoutedRawChannel : public RawChannel::Delegate {
 public:
  RoutedRawChannel(
      ScopedPlatformHandle handle,
      const base::Callback<void(RoutedRawChannel*)>& destruct_callback);

  // Connect the given |pipe| with the pipe_id route. Only non-transferable
  // message pipes can call this, and they can only call it once.
  void AddRoute(uint64_t pipe_id, MessagePipeDispatcher* pipe);

  // Called when the MessagePipeDispatcher is going away.
  void RemoveRoute(uint64_t pipe_id, MessagePipeDispatcher* pipe);

  RawChannel* channel() { return channel_; }

 private:
  friend class base::DeleteHelper<RoutedRawChannel>;
  ~RoutedRawChannel() override;

  // RawChannel::Delegate implementation:
  void OnReadMessage(
        const MessageInTransit::View& message_view,
        ScopedPlatformHandleVectorPtr platform_handles) override;
  void OnError(Error error) override;

  RawChannel* channel_;

  base::Lock lock_;  // Guards access to below.
  base::hash_map<uint64_t, MessagePipeDispatcher*> routes_;

  // If we got messages before the route was added (due to race conditions
  // between different channels), this is used to buffer them.
  struct PendingMessage {
    PendingMessage();
    ~PendingMessage();
    std::vector<char> message;
    ScopedPlatformHandleVectorPtr handles;
  };
  std::vector<scoped_ptr<PendingMessage>> pending_messages_;

  // If we got a ROUTE_CLOSED message for a route before it registered with us,
  // we need to hold on to this information so that we can tell it that the
  // connetion is closed when it does connect.
  base::hash_set<uint64_t> close_routes_;

  base::Callback<void(RoutedRawChannel*)> destruct_callback_;

  DISALLOW_COPY_AND_ASSIGN(RoutedRawChannel);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_ROUTED_RAW_CHANNEL_H_
