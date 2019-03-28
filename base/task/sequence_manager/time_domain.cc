// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/time_domain.h"

#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/task/sequence_manager/task_queue_impl.h"
#include "base/task/sequence_manager/work_queue.h"

namespace base {
namespace sequence_manager {

TimeDomain::TimeDomain() : sequence_manager_(nullptr) {}

TimeDomain::~TimeDomain() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
}

void TimeDomain::OnRegisterWithSequenceManager(
    internal::SequenceManagerImpl* sequence_manager) {
  DCHECK(sequence_manager);
  DCHECK(!sequence_manager_);
  sequence_manager_ = sequence_manager;
}

SequenceManager* TimeDomain::sequence_manager() const {
  DCHECK(sequence_manager_);
  return sequence_manager_;
}

// TODO(kraynov): https://crbug.com/857101 Consider making an interface
// for SequenceManagerImpl which will expose SetNextDelayedDoWork and
// MaybeScheduleImmediateWork methods to make the functions below pure-virtual.

void TimeDomain::SetNextDelayedDoWork(LazyNow* lazy_now, TimeTicks run_time) {
  sequence_manager_->SetNextDelayedDoWork(lazy_now, run_time);
}

void TimeDomain::RequestDoWork() {
  sequence_manager_->MaybeScheduleImmediateWork(FROM_HERE);
}

void TimeDomain::UnregisterQueue(internal::TaskQueueImpl* queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(queue->GetTimeDomain(), this);
  LazyNow lazy_now(CreateLazyNow());
  SetNextWakeUpForQueue(queue, nullopt, &lazy_now);
}

void TimeDomain::SetNextWakeUpForQueue(
    internal::TaskQueueImpl* queue,
    Optional<internal::TaskQueueImpl::DelayedWakeUp> wake_up,
    LazyNow* lazy_now) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(queue->GetTimeDomain(), this);
  DCHECK(queue->IsQueueEnabled() || !wake_up);

  Optional<TimeTicks> previous_wake_up;
  if (!delayed_wake_up_queue_.empty())
    previous_wake_up = delayed_wake_up_queue_.Min().wake_up.time;

  if (wake_up) {
    // Insert a new wake-up into the heap.
    if (queue->heap_handle().IsValid()) {
      // O(log n)
      delayed_wake_up_queue_.ChangeKey(queue->heap_handle(),
                                       {wake_up.value(), queue});
    } else {
      // O(log n)
      delayed_wake_up_queue_.insert({wake_up.value(), queue});
    }
  } else {
    // Remove a wake-up from heap if present.
    if (queue->heap_handle().IsValid())
      delayed_wake_up_queue_.erase(queue->heap_handle());
  }

  Optional<TimeTicks> new_wake_up;
  if (!delayed_wake_up_queue_.empty())
    new_wake_up = delayed_wake_up_queue_.Min().wake_up.time;

  // TODO(kraynov): https://crbug.com/857101 Review the relationship with
  // SequenceManager's time. Right now it's not an issue since
  // VirtualTimeDomain doesn't invoke SequenceManager itself.

  if (new_wake_up) {
    if (new_wake_up != previous_wake_up) {
      // Update the wake-up.
      SetNextDelayedDoWork(lazy_now, new_wake_up.value());
    }
  } else {
    if (previous_wake_up) {
      // No new wake-up to be set, cancel the previous one.
      SetNextDelayedDoWork(lazy_now, TimeTicks::Max());
    }
  }
}

void TimeDomain::WakeUpReadyDelayedQueues(LazyNow* lazy_now) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  // Wake up any queues with pending delayed work.  Note std::multimap stores
  // the elements sorted by key, so the begin() iterator points to the earliest
  // queue to wake-up.
  while (!delayed_wake_up_queue_.empty() &&
         delayed_wake_up_queue_.Min().wake_up.time <= lazy_now->Now()) {
    internal::TaskQueueImpl* queue = delayed_wake_up_queue_.Min().queue;
    queue->WakeUpForDelayedWork(lazy_now);
  }
}

Optional<TimeTicks> TimeDomain::NextScheduledRunTime() const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (delayed_wake_up_queue_.empty())
    return nullopt;
  return delayed_wake_up_queue_.Min().wake_up.time;
}

void TimeDomain::AsValueInto(trace_event::TracedValue* state) const {
  state->BeginDictionary();
  state->SetString("name", GetName());
  state->SetInteger("registered_delay_count", delayed_wake_up_queue_.size());
  if (!delayed_wake_up_queue_.empty()) {
    TimeDelta delay = delayed_wake_up_queue_.Min().wake_up.time - Now();
    state->SetDouble("next_delay_ms", delay.InMillisecondsF());
  }
  AsValueIntoInternal(state);
  state->EndDictionary();
}

void TimeDomain::AsValueIntoInternal(trace_event::TracedValue* state) const {
  // Can be overriden to trace some additional state.
}

}  // namespace sequence_manager
}  // namespace base
