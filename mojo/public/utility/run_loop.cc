// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/utility/run_loop.h"

#include <assert.h>

#include <algorithm>
#include <vector>

#include "mojo/public/utility/run_loop_handler.h"
#include "mojo/public/utility/thread_local.h"

namespace mojo {
namespace utility {
namespace {

ThreadLocalPointer<RunLoop>* tls_run_loop = NULL;

const MojoTimeTicks kInvalidTimeTicks = static_cast<MojoTimeTicks>(0);

}  // namespace

// State needed for one iteration of WaitMany().
struct RunLoop::WaitState {
  WaitState() : deadline(MOJO_DEADLINE_INDEFINITE) {}

  std::vector<Handle> handles;
  std::vector<MojoWaitFlags> wait_flags;
  MojoDeadline deadline;
};

struct RunLoop::RunState {
  RunState() : should_quit(false) {}

  bool should_quit;
};

RunLoop::RunLoop() : run_state_(NULL), next_handler_id_(0) {
  assert(tls_run_loop);
  assert(!tls_run_loop->Get());
  tls_run_loop->Set(this);
}

RunLoop::~RunLoop() {
  assert(tls_run_loop->Get() == this);
  tls_run_loop->Set(NULL);
}

// static
void RunLoop::SetUp() {
  assert(!tls_run_loop);
  tls_run_loop = new ThreadLocalPointer<RunLoop>;
}

// static
void RunLoop::TearDown() {
  assert(!current());
  assert(tls_run_loop);
  delete tls_run_loop;
  tls_run_loop = NULL;
}

// static
RunLoop* RunLoop::current() {
  assert(tls_run_loop);
  return tls_run_loop->Get();
}

void RunLoop::AddHandler(RunLoopHandler* handler,
                         const Handle& handle,
                         MojoWaitFlags wait_flags,
                         MojoDeadline deadline) {
  assert(current() == this);
  assert(handler);
  assert(handle.is_valid());
  // Assume it's an error if someone tries to reregister an existing handle.
  assert(0u == handler_data_.count(handle));
  HandlerData handler_data;
  handler_data.handler = handler;
  handler_data.wait_flags = wait_flags;
  handler_data.deadline = (deadline == MOJO_DEADLINE_INDEFINITE) ?
      kInvalidTimeTicks :
      GetTimeTicksNow() + static_cast<MojoTimeTicks>(deadline);
  handler_data.id = next_handler_id_++;
  handler_data_[handle] = handler_data;
}

void RunLoop::RemoveHandler(const Handle& handle) {
  assert(current() == this);
  handler_data_.erase(handle);
}

bool RunLoop::HasHandler(const Handle& handle) const {
  return handler_data_.find(handle) != handler_data_.end();
}

void RunLoop::Run() {
  assert(current() == this);
  // We don't currently support nesting.
  assert(!run_state_);
  RunState* old_state = run_state_;
  RunState run_state;
  run_state_ = &run_state;
  while (!run_state.should_quit)
    Wait();
  run_state_ = old_state;
}

void RunLoop::Quit() {
  assert(current() == this);
  if (run_state_)
    run_state_->should_quit = true;
}

void RunLoop::Wait() {
  const WaitState wait_state = GetWaitState();
  if (wait_state.handles.empty()) {
    Quit();
    return;
  }

  const MojoResult result =
      WaitMany(wait_state.handles, wait_state.wait_flags, wait_state.deadline);
  if (result >= 0) {
    const size_t index = static_cast<size_t>(result);
    assert(handler_data_.find(wait_state.handles[index]) !=
           handler_data_.end());
    handler_data_[wait_state.handles[index]].handler->OnHandleReady(
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
        assert(false);
    }
  }

  NotifyDeadlineExceeded();
}

void RunLoop::NotifyDeadlineExceeded() {
  // Make a copy in case someone tries to add/remove new handlers as part of
  // notifying.
  const HandleToHandlerData cloned_handlers(handler_data_);
  const MojoTimeTicks now(GetTimeTicksNow());
  for (HandleToHandlerData::const_iterator i = cloned_handlers.begin();
       i != cloned_handlers.end(); ++i) {
    // Since we're iterating over a clone of the handlers, verify the handler is
    // still valid before notifying.
    if (i->second.deadline != kInvalidTimeTicks &&
        i->second.deadline < now &&
        handler_data_.find(i->first) != handler_data_.end() &&
        handler_data_[i->first].id == i->second.id) {
      handler_data_.erase(i->first);
      i->second.handler->OnHandleError(i->first, MOJO_RESULT_DEADLINE_EXCEEDED);
    }
  }
}

void RunLoop::RemoveFirstInvalidHandle(const WaitState& wait_state) {
  for (size_t i = 0; i < wait_state.handles.size(); ++i) {
    const MojoResult result =
        mojo::Wait(wait_state.handles[i], wait_state.wait_flags[i],
                   static_cast<MojoDeadline>(0));
    if (result == MOJO_RESULT_INVALID_ARGUMENT ||
        result == MOJO_RESULT_FAILED_PRECONDITION) {
      // Remove the handle first, this way if OnHandleError() tries to remove
      // the handle our iterator isn't invalidated.
      assert(handler_data_.find(wait_state.handles[i]) != handler_data_.end());
      RunLoopHandler* handler =
          handler_data_[wait_state.handles[i]].handler;
      handler_data_.erase(wait_state.handles[i]);
      handler->OnHandleError(wait_state.handles[i], result);
      return;
    } else {
      assert(MOJO_RESULT_DEADLINE_EXCEEDED == result);
    }
  }
}

RunLoop::WaitState RunLoop::GetWaitState() const {
  WaitState wait_state;
  MojoTimeTicks min_time = kInvalidTimeTicks;
  for (HandleToHandlerData::const_iterator i = handler_data_.begin();
       i != handler_data_.end(); ++i) {
    wait_state.handles.push_back(i->first);
    wait_state.wait_flags.push_back(i->second.wait_flags);
    if (i->second.deadline != kInvalidTimeTicks &&
        (min_time == kInvalidTimeTicks || i->second.deadline < min_time)) {
      min_time = i->second.deadline;
    }
  }
  if (min_time != kInvalidTimeTicks) {
    const MojoTimeTicks now = GetTimeTicksNow();
    if (min_time < now)
      wait_state.deadline = static_cast<MojoDeadline>(0);
    else
      wait_state.deadline = static_cast<MojoDeadline>(min_time - now);
  }
  return wait_state;
}

}  // namespace utility
}  // namespace mojo
