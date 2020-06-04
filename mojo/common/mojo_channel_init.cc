// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/mojo_channel_init.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "mojo/embedder/embedder.h"

namespace mojo {
namespace common {

MojoChannelInit::MojoChannelInit()
    : channel_info_(NULL),
      weak_factory_(this) {
}

MojoChannelInit::~MojoChannelInit() {
  bootstrap_message_pipe_.reset();
  if (channel_info_) {
    io_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&mojo::embedder::DestroyChannelOnIOThread, channel_info_));
  }
}

void MojoChannelInit::Init(
    base::PlatformFile file,
    scoped_refptr<base::TaskRunner> io_thread_task_runner) {
  DCHECK(!io_thread_task_runner_.get());  // Should only init once.
  io_thread_task_runner_ = io_thread_task_runner;
  bootstrap_message_pipe_ = mojo::embedder::CreateChannel(
      mojo::embedder::ScopedPlatformHandle(
          mojo::embedder::PlatformHandle(file)),
      io_thread_task_runner,
      base::Bind(&MojoChannelInit::OnCreatedChannel, weak_factory_.GetWeakPtr(),
                 io_thread_task_runner),
      base::MessageLoop::current()->message_loop_proxy()).Pass();
}

// static
void MojoChannelInit::OnCreatedChannel(
    base::WeakPtr<MojoChannelInit> host,
    scoped_refptr<base::TaskRunner> io_thread,
    embedder::ChannelInfo* channel) {
  // By the time we get here |host| may have been destroyed. If so, shutdown the
  // channel.
  if (!host.get()) {
    io_thread->PostTask(
        FROM_HERE,
        base::Bind(&mojo::embedder::DestroyChannelOnIOThread, channel));
    return;
  }
  host->channel_info_ = channel;
}

}  // namespace common
}  // namespace mojo
