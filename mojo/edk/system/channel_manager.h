// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_CHANNEL_MANAGER_H_
#define MOJO_EDK_SYSTEM_CHANNEL_MANAGER_H_

#include <stdint.h>

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/task_runner.h"
#include "mojo/edk/system/channel.h"
#include "mojo/edk/system/channel_info.h"

namespace mojo {
namespace system {

// IDs for |Channel|s managed by a |ChannelManager|. (IDs should be thought of
// as specific to a given |ChannelManager|.) 0 is never a valid ID.
//
// Note: We currently just use the pointer of the |Channel| casted to a
// |uintptr_t|, but we reserve the right to change this.
typedef uintptr_t ChannelId;

// This class manages and "owns" |Channel|s (which typically connect to other
// processes) for a given process. This class is thread-safe.
class MOJO_SYSTEM_IMPL_EXPORT ChannelManager {
 public:
  ChannelManager();
  ~ChannelManager();

  // Adds |channel| to the set of |Channel|s managed by this |ChannelManager|;
  // |channel_thread_task_runner| should be the task runner for |channel|'s
  // creation (a.k.a. I/O) thread. |channel| should either already be
  // initialized. It should not be managed by any |ChannelManager| yet. Returns
  // the ID for the added channel.
  ChannelId AddChannel(
      scoped_refptr<Channel> channel,
      scoped_refptr<base::TaskRunner> channel_thread_task_runner);

  // Informs the channel manager (and thus channel) that it will be shutdown
  // soon (by calling |ShutdownChannel()|). Calling this is optional (and may in
  // fact be called multiple times) but it will suppress certain warnings (e.g.,
  // for the channel being broken) and enable others (if messages are written to
  // the channel).
  void WillShutdownChannel(ChannelId channel_id);

  // Shuts down the channel specified by the given ID. It is up to the caller to
  // guarantee that this is only called once per channel (that was added using
  // |AddChannel()|). If called from the chanel's creation thread (i.e.,
  // |base::MessageLoopProxy::current()| is the channel thread's |TaskRunner|),
  // this will complete synchronously.
  void ShutdownChannel(ChannelId channel_id);

 private:
  // Gets the ID for a given channel.
  //
  // Note: This is currently a static method and thus may be called under
  // |lock_|. If this is ever made non-static (i.e., made specific to a given
  // |ChannelManager|), those call sites may have to changed.
  static ChannelId GetChannelId(const Channel* channel) {
    return reinterpret_cast<ChannelId>(channel);
  }

  // Gets the |ChannelInfo| for the channel specified by the given ID. (This
  // should *not* be called under lock.)
  ChannelInfo GetChannelInfo(ChannelId channel_id);

  // Note: |Channel| methods should not be called under |lock_|.
  base::Lock lock_;  // Protects the members below.

  base::hash_map<ChannelId, ChannelInfo> channel_infos_;

  DISALLOW_COPY_AND_ASSIGN(ChannelManager);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_CHANNEL_MANAGER_H_
