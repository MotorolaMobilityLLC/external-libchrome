// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/message_pump_mojo.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "base/time/time.h"
#include "mojo/common/message_pump_mojo_handler.h"
#include "mojo/common/time_helper.h"

namespace mojo {
namespace common {

// State needed for one iteration of WaitMany. The first handle and flags
// corresponds to that of the control pipe.
struct MessagePumpMojo::WaitState {
  std::vector<Handle> handles;
  std::vector<MojoWaitFlags> wait_flags;
};

struct MessagePumpMojo::RunState {
  RunState() : should_quit(false) {
    CreateMessagePipe(&read_handle, &write_handle);
  }

  base::TimeTicks delayed_work_time;

  // Used to wake up WaitForWork().
  ScopedMessagePipeHandle read_handle;
  ScopedMessagePipeHandle write_handle;

  bool should_quit;
};

MessagePumpMojo::MessagePumpMojo() : run_state_(NULL), next_handler_id_(0) {
}

MessagePumpMojo::~MessagePumpMojo() {
}

void MessagePumpMojo::AddHandler(MessagePumpMojoHandler* handler,
                                 const Handle& handle,
                                 MojoWaitFlags wait_flags,
                                 base::TimeTicks deadline) {
  DCHECK(handler);
  DCHECK(handle.is_valid());
  // Assume it's an error if someone tries to reregister an existing handle.
  DCHECK_EQ(0u, handlers_.count(handle));
  Handler handler_data;
  handler_data.handler = handler;
  handler_data.wait_flags = wait_flags;
  handler_data.deadline = deadline;
  handler_data.id = next_handler_id_++;
  handlers_[handle] = handler_data;
}

void MessagePumpMojo::RemoveHandler(const Handle& handle) {
  handlers_.erase(handle);
}

void MessagePumpMojo::Run(Delegate* delegate) {
  RunState* old_state = run_state_;
  RunState run_state;
  // TODO: better deal with error handling.
  CHECK(run_state.read_handle.is_valid());
  CHECK(run_state.write_handle.is_valid());
  run_state_ = &run_state;
  bool more_work_is_plausible = true;
  for (;;) {
    const bool block = !more_work_is_plausible;
    DoInternalWork(block);

    // There isn't a good way to know if there are more handles ready, we assume
    // not.
    more_work_is_plausible = false;

    if (run_state.should_quit)
      break;

    more_work_is_plausible |= delegate->DoWork();
    if (run_state.should_quit)
      break;

    more_work_is_plausible |= delegate->DoDelayedWork(
        &run_state.delayed_work_time);
    if (run_state.should_quit)
      break;

    if (more_work_is_plausible)
      continue;

    more_work_is_plausible = delegate->DoIdleWork();
    if (run_state.should_quit)
      break;
  }
  run_state_ = old_state;
}

void MessagePumpMojo::Quit() {
  if (run_state_)
    run_state_->should_quit = true;
}

void MessagePumpMojo::ScheduleWork() {
  SignalControlPipe();
}

void MessagePumpMojo::ScheduleDelayedWork(
    const base::TimeTicks& delayed_work_time) {
  if (!run_state_)
    return;
  run_state_->delayed_work_time = delayed_work_time;
  SignalControlPipe();
}

void MessagePumpMojo::DoInternalWork(bool block) {
  const MojoDeadline deadline = block ? GetDeadlineForWait() : 0;
  const WaitState wait_state = GetWaitState();
  const MojoResult result =
      WaitMany(wait_state.handles, wait_state.wait_flags, deadline);
  if (result == 0) {
    // Control pipe was written to.
    uint32_t num_bytes = 0;
    ReadMessageRaw(run_state_->read_handle.get(), NULL, &num_bytes, NULL, NULL,
                   MOJO_READ_MESSAGE_FLAG_MAY_DISCARD);
  } else if (result > 0) {
    const size_t index = static_cast<size_t>(result);
    DCHECK(handlers_.find(wait_state.handles[index]) != handlers_.end());
    handlers_[wait_state.handles[index]].handler->OnHandleReady(
        wait_state.handles[index]);
  } else {
    switch (result) {
      case MOJO_RESULT_INVALID_ARGUMENT:
      case MOJO_RESULT_FAILED_PRECONDITION:
        RemoveFirstInvalidHandle(wait_state);
        break;
      case MOJO_RESULT_DEADLINE_EXCEEDED:
        break;
      default:
        NOTREACHED();
    }
  }

  // Notify and remove any handlers whose time has expired. Make a copy in case
  // someone tries to add/remove new handlers from notification.
  const HandleToHandler cloned_handlers(handlers_);
  const base::TimeTicks now(internal::NowTicks());
  for (HandleToHandler::const_iterator i = cloned_handlers.begin();
       i != cloned_handlers.end(); ++i) {
    // Since we're iterating over a clone of the handlers, verify the handler is
    // still valid before notifying.
    if (!i->second.deadline.is_null() && i->second.deadline < now &&
        handlers_.find(i->first) != handlers_.end() &&
        handlers_[i->first].id == i->second.id) {
      i->second.handler->OnHandleError(i->first, MOJO_RESULT_DEADLINE_EXCEEDED);
    }
  }
}

void MessagePumpMojo::RemoveFirstInvalidHandle(const WaitState& wait_state) {
  // TODO(sky): deal with control pipe going bad.
  for (size_t i = 1; i < wait_state.handles.size(); ++i) {
    const MojoResult result =
        Wait(wait_state.handles[i], wait_state.wait_flags[i], 0);
    if (result == MOJO_RESULT_INVALID_ARGUMENT ||
        result == MOJO_RESULT_FAILED_PRECONDITION) {
      // Remove the handle first, this way if OnHandleError() tries to remove
      // the handle our iterator isn't invalidated.
      DCHECK(handlers_.find(wait_state.handles[i]) != handlers_.end());
      MessagePumpMojoHandler* handler =
          handlers_[wait_state.handles[i]].handler;
      handlers_.erase(wait_state.handles[i]);
      handler->OnHandleError(wait_state.handles[i], result);
      return;
    } else {
      DCHECK_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, result);
    }
  }
}

void MessagePumpMojo::SignalControlPipe() {
  if (!run_state_)
    return;

  // TODO(sky): deal with error?
  WriteMessageRaw(run_state_->write_handle.get(), NULL, 0, NULL, 0,
                  MOJO_WRITE_MESSAGE_FLAG_NONE);
}

MessagePumpMojo::WaitState MessagePumpMojo::GetWaitState() const {
  WaitState wait_state;
  wait_state.handles.push_back(run_state_->read_handle.get());
  wait_state.wait_flags.push_back(MOJO_WAIT_FLAG_READABLE);

  for (HandleToHandler::const_iterator i = handlers_.begin();
       i != handlers_.end(); ++i) {
    wait_state.handles.push_back(i->first);
    wait_state.wait_flags.push_back(i->second.wait_flags);
  }
  return wait_state;
}

MojoDeadline MessagePumpMojo::GetDeadlineForWait() const {
  base::TimeTicks min_time = run_state_->delayed_work_time;
  for (HandleToHandler::const_iterator i = handlers_.begin();
       i != handlers_.end(); ++i) {
    if (min_time.is_null() && i->second.deadline < min_time)
      min_time = i->second.deadline;
  }
  return min_time.is_null() ? MOJO_DEADLINE_INDEFINITE :
      std::max(static_cast<MojoDeadline>(0),
               static_cast<MojoDeadline>(
                   (min_time - internal::NowTicks()).InMicroseconds()));
}

}  // namespace common
}  // namespace mojo
