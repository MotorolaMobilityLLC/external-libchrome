// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_mojo.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace IPC {

// static
std::unique_ptr<Channel> Channel::CreateClient(
    const IPC::ChannelHandle& channel_handle,
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner) {
  if (channel_handle.mojo_handle.is_valid()) {
    return ChannelMojo::Create(
        mojo::ScopedMessagePipeHandle(channel_handle.mojo_handle),
        Channel::MODE_CLIENT, listener, ipc_task_runner);
  }
  return Channel::Create(channel_handle, Channel::MODE_CLIENT, listener);
}

// static
std::unique_ptr<Channel> Channel::CreateNamedServer(
    const IPC::ChannelHandle& channel_handle,
    Listener* listener) {
  return Channel::Create(channel_handle, Channel::MODE_NAMED_SERVER, listener);
}

// static
std::unique_ptr<Channel> Channel::CreateNamedClient(
    const IPC::ChannelHandle& channel_handle,
    Listener* listener) {
  return Channel::Create(channel_handle, Channel::MODE_NAMED_CLIENT, listener);
}

// static
std::unique_ptr<Channel> Channel::CreateServer(
    const IPC::ChannelHandle& channel_handle,
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner) {
  if (channel_handle.mojo_handle.is_valid()) {
    return ChannelMojo::Create(
        mojo::ScopedMessagePipeHandle(channel_handle.mojo_handle),
        Channel::MODE_SERVER, listener, ipc_task_runner);
  }
  return Channel::Create(channel_handle, Channel::MODE_SERVER, listener);
}

// static
void Channel::GenerateMojoChannelHandlePair(
    const std::string& name_postfix,
    IPC::ChannelHandle* handle0,
    IPC::ChannelHandle* handle1) {
  DCHECK_NE(handle0, handle1);
  mojo::MessagePipe message_pipe;
  *handle0 = ChannelHandle(message_pipe.handle0.release());
  *handle1 = ChannelHandle(message_pipe.handle1.release());
}

Channel::~Channel() {
}

Channel::AssociatedInterfaceSupport* Channel::GetAssociatedInterfaceSupport() {
  return nullptr;
}

void Channel::Pause() { NOTREACHED(); }

void Channel::Unpause(bool flush) { NOTREACHED(); }

void Channel::Flush() { NOTREACHED(); }

void Channel::OnSetAttachmentBrokerEndpoint() {
  CHECK(!did_start_connect_);
}

void Channel::WillConnect() {
  did_start_connect_ = true;
}

}  // namespace IPC

