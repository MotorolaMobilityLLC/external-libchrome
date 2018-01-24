// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROFILER_STACK_SAMPLING_PROFILER_H_
#define BASE_PROFILER_STACK_SAMPLING_PROFILER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/atomicops.h"
#include "base/base_export.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"

namespace base {

class NativeStackSampler;
class NativeStackSamplerTestDelegate;

// StackSamplingProfiler periodically stops a thread to sample its stack, for
// the purpose of collecting information about which code paths are
// executing. This information is used in aggregate by UMA to identify hot
// and/or janky code paths.
//
// Sample StackSamplingProfiler usage:
//
//   // Create and customize params as desired.
//   base::StackStackSamplingProfiler::SamplingParams params;
//   // Any thread's ID may be passed as the target.
//   base::StackSamplingProfiler profiler(base::PlatformThread::CurrentId()),
//       params);
//
//   // Or, to process the profiles within Chrome rather than via UMA, use a
//   // custom completed callback:
//   base::StackStackSamplingProfiler::CompletedCallback
//       thread_safe_callback = ...;
//   base::StackSamplingProfiler profiler(base::PlatformThread::CurrentId()),
//       params, thread_safe_callback);
//
//   profiler.Start();
//   // ... work being done on the target thread here ...
//   profiler.Stop();  // optional, stops collection before complete per params
//
// The default SamplingParams causes stacks to be recorded in a single burst at
// a 10Hz interval for a total of 30 seconds. All of these parameters may be
// altered as desired.
//
// When all call stack profiles are complete, or the profiler is stopped, the
// completed callback is called from a thread created by the profiler with the
// collected profiles.
//
// The results of the profiling are passed to the completed callback and consist
// of a vector of CallStackProfiles. Each CallStackProfile corresponds to a
// burst as specified in SamplingParams and contains a set of Samples and
// Modules. One Sample corresponds to a single recorded stack, and the Modules
// record those modules associated with the recorded stack frames.
class BASE_EXPORT StackSamplingProfiler {
 public:
  // Module represents the module (DLL or exe) corresponding to a stack frame.
  struct BASE_EXPORT Module {
    Module();
    Module(uintptr_t base_address,
           const std::string& id,
           const FilePath& filename);
    ~Module();

    // Points to the base address of the module.
    uintptr_t base_address;

    // An opaque binary string that uniquely identifies a particular program
    // version with high probability. This is parsed from headers of the loaded
    // module.
    // For binaries generated by GNU tools:
    //   Contents of the .note.gnu.build-id field.
    // On Windows:
    //   GUID + AGE in the debug image headers of a module.
    std::string id;

    // The filename of the module.
    FilePath filename;
  };

  // Frame represents an individual sampled stack frame with module information.
  struct BASE_EXPORT Frame {
    // Identifies an unknown module.
    static const size_t kUnknownModuleIndex = static_cast<size_t>(-1);

    Frame(uintptr_t instruction_pointer, size_t module_index);
    ~Frame();

    // Default constructor to satisfy IPC macros. Do not use explicitly.
    Frame();

    // The sampled instruction pointer within the function.
    uintptr_t instruction_pointer;

    // Index of the module in CallStackProfile::modules. We don't represent
    // module state directly here to save space.
    size_t module_index;
  };

  // Sample represents a set of stack frames with some extra information.
  struct BASE_EXPORT Sample {
    Sample();
    Sample(const Sample& sample);
    ~Sample();

    // These constructors are used only during testing.
    Sample(const Frame& frame);
    Sample(const std::vector<Frame>& frames);

    // The entire stack frame when the sample is taken.
    std::vector<Frame> frames;

    // A bit-field indicating which process milestones have passed. This can be
    // used to tell where in the process lifetime the samples are taken. Just
    // as a "lifetime" can only move forward, these bits mark the milestones of
    // the processes life as they occur. Bits can be set but never reset. The
    // actual definition of the individual bits is left to the user of this
    // module.
    uint32_t process_milestones = 0;
  };

  // CallStackProfile represents a set of samples.
  struct BASE_EXPORT CallStackProfile {
    CallStackProfile();
    CallStackProfile(CallStackProfile&& other);
    ~CallStackProfile();

    CallStackProfile& operator=(CallStackProfile&& other);

    CallStackProfile CopyForTesting() const;

    std::vector<Module> modules;
    std::vector<Sample> samples;

    // Duration of this profile.
    TimeDelta profile_duration;

    // Time between samples.
    TimeDelta sampling_period;

   private:
    // Copying is possible but expensive so disallow it except for internal use
    // (i.e. CopyForTesting); use std::move instead.
    CallStackProfile(const CallStackProfile& other);

    DISALLOW_ASSIGN(CallStackProfile);
  };

  using CallStackProfiles = std::vector<CallStackProfile>;

  // Represents parameters that configure the sampling.
  struct BASE_EXPORT SamplingParams {
    // Time to delay before first samples are taken.
    TimeDelta initial_delay = TimeDelta::FromMilliseconds(0);

    // Number of sampling bursts to perform.
    int bursts = 1;

    // Interval between sampling bursts. This is the desired duration from the
    // start of one burst to the start of the next burst.
    TimeDelta burst_interval = TimeDelta::FromSeconds(10);

    // Number of samples to record per burst.
    int samples_per_burst = 300;

    // Interval between samples during a sampling burst. This is the desired
    // duration from the start of one sample to the start of the next sample.
    TimeDelta sampling_interval = TimeDelta::FromMilliseconds(100);
  };

  // Testing support. These methods are static beause they interact with the
  // sampling thread, a singleton used by all StackSamplingProfiler objects.
  // These methods can only be called by the same thread that started the
  // sampling.
  class BASE_EXPORT TestAPI {
   public:
    // Resets the internal state to that of a fresh start. This is necessary
    // so that tests don't inherit state from previous tests.
    static void Reset();

    // Resets internal annotations (like process phase) to initial values.
    static void ResetAnnotations();

    // Returns whether the sampling thread is currently running or not.
    static bool IsSamplingThreadRunning();

    // Disables inherent idle-shutdown behavior.
    static void DisableIdleShutdown();

    // Initiates an idle shutdown task, as though the idle timer had expired,
    // causing the thread to exit. There is no "idle" check so this must be
    // called only when all sampling tasks have completed. This blocks until
    // the task has been executed, though the actual stopping of the thread
    // still happens asynchronously. Watch IsSamplingThreadRunning() to know
    // when the thread has exited. If |simulate_intervening_start| is true then
    // this method will make it appear to the shutdown task that a new profiler
    // was started between when the idle-shutdown was initiated and when it
    // runs.
    static void PerformSamplingThreadIdleShutdown(
        bool simulate_intervening_start);
  };

  // The callback type used to collect completed profiles. The passed |profiles|
  // are move-only. Other threads, including the UI thread, may block on
  // callback completion so this should run as quickly as possible.
  //
  // After collection completion, the callback may instruct the profiler to do
  // additional collection(s) by returning a SamplingParams object to indicate
  // collection should be started again.
  //
  // IMPORTANT NOTE: The callback is invoked on a thread the profiler
  // constructs, rather than on the thread used to construct the profiler and
  // set the callback, and thus the callback must be callable on any thread. For
  // threads with message loops that create StackSamplingProfilers, posting a
  // task to the message loop with the moved (i.e. std::move) profiles is the
  // thread-safe callback implementation.
  using CompletedCallback =
      Callback<Optional<SamplingParams>(CallStackProfiles)>;

  // Creates a profiler for the CURRENT thread that sends completed profiles
  // to |callback|. An optional |test_delegate| can be supplied by tests.
  // The caller must ensure that this object gets destroyed before the current
  // thread exits.
  StackSamplingProfiler(
      const SamplingParams& params,
      const CompletedCallback& callback,
      NativeStackSamplerTestDelegate* test_delegate = nullptr);

  // Creates a profiler for ANOTHER thread that sends completed profiles to
  // |callback|. An optional |test_delegate| can be supplied by tests.
  //
  // IMPORTANT: The caller must ensure that the thread being sampled does not
  // exit before this object gets destructed or Bad Things(tm) may occur.
  StackSamplingProfiler(
      PlatformThreadId thread_id,
      const SamplingParams& params,
      const CompletedCallback& callback,
      NativeStackSamplerTestDelegate* test_delegate = nullptr);

  // Stops any profiling currently taking place before destroying the profiler.
  // This will block until the callback has been run if profiling has started
  // but not already finished.
  ~StackSamplingProfiler();

  // Initializes the profiler and starts sampling. Might block on a
  // WaitableEvent if this StackSamplingProfiler was previously started and
  // recently stopped, while the previous profiling phase winds down.
  void Start();

  // Stops the profiler and any ongoing sampling. This method will return
  // immediately with the callback being run asynchronously. At most one
  // more stack sample will be taken after this method returns. Calling this
  // function is optional; if not invoked profiling terminates when all the
  // profiling bursts specified in the SamplingParams are completed or the
  // profiler object is destroyed, whichever occurs first.
  void Stop();

  // Set the current system state that is recorded with each captured stack
  // frame. This is thread-safe so can be called from anywhere. The parameter
  // value should be from an enumeration of the appropriate type with values
  // ranging from 0 to 31, inclusive. This sets bits within Sample field of
  // |process_milestones|. The actual meanings of these bits are defined
  // (globally) by the caller(s).
  static void SetProcessMilestone(int milestone);

 private:
  friend class TestAPI;

  // SamplingThread is a separate thread used to suspend and sample stacks from
  // the target thread.
  class SamplingThread;

  // Adds annotations to a Sample.
  static void RecordAnnotations(Sample* sample);

  // This global variables holds the current system state and is recorded with
  // every captured sample, done on a separate thread which is why updates to
  // this must be atomic. A PostTask to move the the updates to that thread
  // would skew the timing and a lock could result in deadlock if the thread
  // making a change was also being profiled and got stopped.
  static subtle::Atomic32 process_milestones_;

  // The thread whose stack will be sampled.
  PlatformThreadId thread_id_;

  const SamplingParams params_;

  const CompletedCallback completed_callback_;

  // This starts "signaled", is reset when sampling begins, and is signaled
  // when that sampling is complete and the callback done.
  WaitableEvent profiling_inactive_;

  // Object that does the native sampling. This is created during construction
  // and later passed to the sampling thread when profiling is started.
  std::unique_ptr<NativeStackSampler> native_sampler_;

  // An ID uniquely identifying this profiler to the sampling thread. This
  // will be an internal "null" value when no collection has been started.
  int profiler_id_;

  // Stored until it can be passed to the NativeStackSampler created in Start().
  NativeStackSamplerTestDelegate* const test_delegate_;

  DISALLOW_COPY_AND_ASSIGN(StackSamplingProfiler);
};

// These operators permit types to be compared and used in a map of Samples, as
// done in tests and by the metrics provider code.
BASE_EXPORT bool operator==(const StackSamplingProfiler::Module& a,
                            const StackSamplingProfiler::Module& b);
BASE_EXPORT bool operator==(const StackSamplingProfiler::Sample& a,
                            const StackSamplingProfiler::Sample& b);
BASE_EXPORT bool operator!=(const StackSamplingProfiler::Sample& a,
                            const StackSamplingProfiler::Sample& b);
BASE_EXPORT bool operator<(const StackSamplingProfiler::Sample& a,
                           const StackSamplingProfiler::Sample& b);
BASE_EXPORT bool operator==(const StackSamplingProfiler::Frame& a,
                            const StackSamplingProfiler::Frame& b);
BASE_EXPORT bool operator<(const StackSamplingProfiler::Frame& a,
                           const StackSamplingProfiler::Frame& b);

}  // namespace base

#endif  // BASE_PROFILER_STACK_SAMPLING_PROFILER_H_
