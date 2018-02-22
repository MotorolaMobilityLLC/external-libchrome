// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_dump_manager.h"

#include <inttypes.h>
#include <stdio.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/allocator/buildflags.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/debug/stack_trace.h"
#include "base/debug/thread_heap_usage_tracker.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/heap_profiler.h"
#include "base/trace_event/heap_profiler_allocation_context_tracker.h"
#include "base/trace_event/heap_profiler_event_filter.h"
#include "base/trace_event/heap_profiler_serialization_state.h"
#include "base/trace_event/heap_profiler_stack_frame_deduplicator.h"
#include "base/trace_event/heap_profiler_type_name_deduplicator.h"
#include "base/trace_event/malloc_dump_provider.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/memory_dump_scheduler.h"
#include "base/trace_event/memory_infra_background_whitelist.h"
#include "base/trace_event/memory_peak_detector.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "build/build_config.h"

#if defined(OS_ANDROID)
#include "base/trace_event/java_heap_dump_provider_android.h"
#endif

namespace base {
namespace trace_event {

namespace {

const char* const kTraceEventArgNames[] = {"dumps"};
const unsigned char kTraceEventArgTypes[] = {TRACE_VALUE_TYPE_CONVERTABLE};

MemoryDumpManager* g_instance_for_testing = nullptr;

// Temporary (until peak detector and scheduler are moved outside of here)
// trampoline function to match the |request_dump_function| passed to Initialize
// to the callback expected by MemoryPeakDetector and MemoryDumpScheduler.
// TODO(primiano): remove this.
void DoGlobalDumpWithoutCallback(
    MemoryDumpManager::RequestGlobalDumpFunction global_dump_fn,
    MemoryDumpType dump_type,
    MemoryDumpLevelOfDetail level_of_detail) {
  global_dump_fn.Run(dump_type, level_of_detail);
}

// Proxy class which wraps a ConvertableToTraceFormat owned by the
// |heap_profiler_serialization_state| into a proxy object that can be added to
// the trace event log. This is to solve the problem that the
// HeapProfilerSerializationState is refcounted but the tracing subsystem wants
// a std::unique_ptr<ConvertableToTraceFormat>.
template <typename T>
struct SessionStateConvertableProxy : public ConvertableToTraceFormat {
  using GetterFunctPtr = T* (HeapProfilerSerializationState::*)() const;

  SessionStateConvertableProxy(scoped_refptr<HeapProfilerSerializationState>
                                   heap_profiler_serialization_state,
                               GetterFunctPtr getter_function)
      : heap_profiler_serialization_state(heap_profiler_serialization_state),
        getter_function(getter_function) {}

  void AppendAsTraceFormat(std::string* out) const override {
    return (heap_profiler_serialization_state.get()->*getter_function)()
        ->AppendAsTraceFormat(out);
  }

  void EstimateTraceMemoryOverhead(
      TraceEventMemoryOverhead* overhead) override {
    return (heap_profiler_serialization_state.get()->*getter_function)()
        ->EstimateTraceMemoryOverhead(overhead);
  }

  scoped_refptr<HeapProfilerSerializationState>
      heap_profiler_serialization_state;
  GetterFunctPtr const getter_function;
};

void NotifyHeapProfilingEnabledOnMDPThread(
    scoped_refptr<MemoryDumpProviderInfo> mdpinfo,
    bool profiling_enabled) {
  mdpinfo->dump_provider->OnHeapProfilingEnabled(profiling_enabled);
}

inline bool ShouldEnableMDPAllocatorHooks(HeapProfilingMode mode) {
  return (mode == kHeapProfilingModePseudo) ||
         (mode == kHeapProfilingModeNative) ||
         (mode == kHeapProfilingModeBackground);
}

#if BUILDFLAG(USE_ALLOCATOR_SHIM) && !defined(OS_NACL)
inline bool IsHeapProfilingModeEnabled(HeapProfilingMode mode) {
  return mode != kHeapProfilingModeDisabled &&
         mode != kHeapProfilingModeInvalid;
}

void EnableFilteringForPseudoStackProfiling() {
  if (AllocationContextTracker::capture_mode() !=
          AllocationContextTracker::CaptureMode::PSEUDO_STACK ||
      (TraceLog::GetInstance()->enabled_modes() & TraceLog::FILTERING_MODE)) {
    return;
  }
  // Create trace config with heap profiling filter.
  std::string filter_string = JoinString(
      {"*", TRACE_DISABLED_BY_DEFAULT("net"), TRACE_DISABLED_BY_DEFAULT("cc"),
       MemoryDumpManager::kTraceCategory},
      ",");
  TraceConfigCategoryFilter category_filter;
  category_filter.InitializeFromString(filter_string);

  TraceConfig::EventFilterConfig heap_profiler_filter_config(
      HeapProfilerEventFilter::kName);
  heap_profiler_filter_config.SetCategoryFilter(category_filter);

  TraceConfig::EventFilters filters;
  filters.push_back(heap_profiler_filter_config);
  TraceConfig filtering_trace_config;
  filtering_trace_config.SetEventFilters(filters);

  TraceLog::GetInstance()->SetEnabled(filtering_trace_config,
                                      TraceLog::FILTERING_MODE);
}
#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM) && !defined(OS_NACL)

}  // namespace

// static
const char* const MemoryDumpManager::kTraceCategory =
    TRACE_DISABLED_BY_DEFAULT("memory-infra");

// static
const int MemoryDumpManager::kMaxConsecutiveFailuresCount = 3;

// static
const uint64_t MemoryDumpManager::kInvalidTracingProcessId = 0;

// static
const char* const MemoryDumpManager::kSystemAllocatorPoolName =
#if defined(MALLOC_MEMORY_TRACING_SUPPORTED)
    MallocDumpProvider::kAllocatedObjects;
#else
    nullptr;
#endif

// static
MemoryDumpManager* MemoryDumpManager::GetInstance() {
  if (g_instance_for_testing)
    return g_instance_for_testing;

  return Singleton<MemoryDumpManager,
                   LeakySingletonTraits<MemoryDumpManager>>::get();
}

// static
std::unique_ptr<MemoryDumpManager>
MemoryDumpManager::CreateInstanceForTesting() {
  DCHECK(!g_instance_for_testing);
  std::unique_ptr<MemoryDumpManager> instance(new MemoryDumpManager());
  g_instance_for_testing = instance.get();
  return instance;
}

MemoryDumpManager::MemoryDumpManager()
    : is_coordinator_(false),
      tracing_process_id_(kInvalidTracingProcessId),
      dumper_registrations_ignored_for_testing_(false),
      heap_profiling_mode_(kHeapProfilingModeDisabled) {}

MemoryDumpManager::~MemoryDumpManager() {
  Thread* dump_thread = nullptr;
  {
    AutoLock lock(lock_);
    if (dump_thread_) {
      dump_thread = dump_thread_.get();
    }
  }
  if (dump_thread) {
    dump_thread->Stop();
  }
  AutoLock lock(lock_);
  dump_thread_.reset();
  g_instance_for_testing = nullptr;
}

// static
HeapProfilingMode MemoryDumpManager::GetHeapProfilingModeFromCommandLine() {
  if (!CommandLine::InitializedForCurrentProcess() ||
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableHeapProfiling)) {
    return kHeapProfilingModeDisabled;
  }
#if BUILDFLAG(USE_ALLOCATOR_SHIM) && !defined(OS_NACL)
  std::string profiling_mode =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kEnableHeapProfiling);
  if (profiling_mode == switches::kEnableHeapProfilingTaskProfiler)
    return kHeapProfilingModeTaskProfiler;
  if (profiling_mode == switches::kEnableHeapProfilingModePseudo)
    return kHeapProfilingModePseudo;
  if (profiling_mode == switches::kEnableHeapProfilingModeNative)
    return kHeapProfilingModeNative;
#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM) && !defined(OS_NACL)
  return kHeapProfilingModeInvalid;
}

void MemoryDumpManager::EnableHeapProfilingIfNeeded() {
#if BUILDFLAG(USE_ALLOCATOR_SHIM) && !defined(OS_NACL)
  HeapProfilingMode profiling_mode = GetHeapProfilingModeFromCommandLine();
  if (IsHeapProfilingModeEnabled(profiling_mode)) {
    EnableHeapProfiling(profiling_mode);
  } else {
    if (profiling_mode == kHeapProfilingModeInvalid) {
      // Heap profiling is misconfigured, disable it permanently.
      EnableHeapProfiling(kHeapProfilingModeDisabled);
    }
  }
#else
  // Heap profiling is unsupported, disable it permanently.
  EnableHeapProfiling(kHeapProfilingModeDisabled);
#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM) && !defined(OS_NACL)
}

bool MemoryDumpManager::EnableHeapProfiling(HeapProfilingMode profiling_mode) {
  AutoLock lock(lock_);
#if BUILDFLAG(USE_ALLOCATOR_SHIM) && !defined(OS_NACL)
  bool notify_mdps = true;

  if (heap_profiling_mode_ == kHeapProfilingModeInvalid)
    return false;  // Disabled permanently.

  if (IsHeapProfilingModeEnabled(heap_profiling_mode_) ==
      IsHeapProfilingModeEnabled(profiling_mode)) {
    if (profiling_mode == kHeapProfilingModeDisabled)
      heap_profiling_mode_ = kHeapProfilingModeInvalid;  // Disable permanently.
    return false;
  }

  switch (profiling_mode) {
    case kHeapProfilingModeTaskProfiler:
      if (!base::debug::ThreadHeapUsageTracker::IsHeapTrackingEnabled())
        base::debug::ThreadHeapUsageTracker::EnableHeapTracking();
      notify_mdps = false;
      break;

    case kHeapProfilingModeBackground:
      AllocationContextTracker::SetCaptureMode(
          AllocationContextTracker::CaptureMode::MIXED_STACK);
      break;

    case kHeapProfilingModePseudo:
      AllocationContextTracker::SetCaptureMode(
          AllocationContextTracker::CaptureMode::PSEUDO_STACK);
      EnableFilteringForPseudoStackProfiling();
      break;

    case kHeapProfilingModeNative:
      // If we don't have frame pointers then native tracing falls-back to
      // using base::debug::StackTrace, which may be slow.
      AllocationContextTracker::SetCaptureMode(
          AllocationContextTracker::CaptureMode::NATIVE_STACK);
      break;

    case kHeapProfilingModeDisabled:
      if (heap_profiling_mode_ == kHeapProfilingModeTaskProfiler) {
        LOG(ERROR) << "ThreadHeapUsageTracker cannot be disabled.";
        return false;
      }
      if (heap_profiling_mode_ == kHeapProfilingModePseudo)
        TraceLog::GetInstance()->SetDisabled(TraceLog::FILTERING_MODE);
      AllocationContextTracker::SetCaptureMode(
          AllocationContextTracker::CaptureMode::DISABLED);
      heap_profiling_mode_ = kHeapProfilingModeInvalid;  // Disable permanently.
      break;

    default:
      NOTREACHED() << "Incorrect heap profiling mode " << profiling_mode;
      return false;
  }

  if (heap_profiling_mode_ != kHeapProfilingModeInvalid)
    heap_profiling_mode_ = profiling_mode;

  // In case tracing was already enabled, setup the serialization state before
  // notifying mdps.
  InitializeHeapProfilerStateIfNeededLocked();
  if (notify_mdps) {
    bool enabled = IsHeapProfilingModeEnabled(heap_profiling_mode_);
    for (const auto& mdpinfo : dump_providers_)
      NotifyHeapProfilingEnabledLocked(mdpinfo, enabled);
  }
  return true;
#else
  heap_profiling_mode_ = kHeapProfilingModeInvalid;
  return false;
#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM) && !defined(OS_NACL)
}

HeapProfilingMode MemoryDumpManager::GetHeapProfilingMode() {
  AutoLock lock(lock_);
  return heap_profiling_mode_;
}

void MemoryDumpManager::Initialize(
    RequestGlobalDumpFunction request_dump_function,
    bool is_coordinator) {
  {
    AutoLock lock(lock_);
    DCHECK(!request_dump_function.is_null());
    DCHECK(!can_request_global_dumps());
    request_dump_function_ = request_dump_function;
    is_coordinator_ = is_coordinator;
  }
  EnableHeapProfilingIfNeeded();

// Enable the core dump providers.
#if defined(MALLOC_MEMORY_TRACING_SUPPORTED)
  base::trace_event::MemoryDumpProvider::Options options;
  options.supports_heap_profiling = true;
  RegisterDumpProvider(MallocDumpProvider::GetInstance(), "Malloc", nullptr,
                       options);
#endif

#if defined(OS_ANDROID)
  RegisterDumpProvider(JavaHeapDumpProvider::GetInstance(), "JavaHeap",
                       nullptr);
#endif

  TRACE_EVENT_WARMUP_CATEGORY(kTraceCategory);
}

void MemoryDumpManager::RegisterDumpProvider(
    MemoryDumpProvider* mdp,
    const char* name,
    scoped_refptr<SingleThreadTaskRunner> task_runner,
    MemoryDumpProvider::Options options) {
  options.dumps_on_single_thread_task_runner = true;
  RegisterDumpProviderInternal(mdp, name, std::move(task_runner), options);
}

void MemoryDumpManager::RegisterDumpProvider(
    MemoryDumpProvider* mdp,
    const char* name,
    scoped_refptr<SingleThreadTaskRunner> task_runner) {
  // Set |dumps_on_single_thread_task_runner| to true because all providers
  // without task runner are run on dump thread.
  MemoryDumpProvider::Options options;
  options.dumps_on_single_thread_task_runner = true;
  RegisterDumpProviderInternal(mdp, name, std::move(task_runner), options);
}

void MemoryDumpManager::RegisterDumpProviderWithSequencedTaskRunner(
    MemoryDumpProvider* mdp,
    const char* name,
    scoped_refptr<SequencedTaskRunner> task_runner,
    MemoryDumpProvider::Options options) {
  DCHECK(task_runner);
  options.dumps_on_single_thread_task_runner = false;
  RegisterDumpProviderInternal(mdp, name, std::move(task_runner), options);
}

void MemoryDumpManager::RegisterDumpProviderInternal(
    MemoryDumpProvider* mdp,
    const char* name,
    scoped_refptr<SequencedTaskRunner> task_runner,
    const MemoryDumpProvider::Options& options) {
  if (dumper_registrations_ignored_for_testing_)
    return;

  // A handful of MDPs are required to compute the summary struct these are
  // 'whitelisted for summary mode'. These MDPs are a subset of those which
  // have small enough performance overhead that it is resonable to run them
  // in the background while the user is doing other things. Those MDPs are
  // 'whitelisted for background mode'.
  bool whitelisted_for_background_mode = IsMemoryDumpProviderWhitelisted(name);
  bool whitelisted_for_summary_mode =
      IsMemoryDumpProviderWhitelistedForSummary(name);

  scoped_refptr<MemoryDumpProviderInfo> mdpinfo = new MemoryDumpProviderInfo(
      mdp, name, std::move(task_runner), options,
      whitelisted_for_background_mode, whitelisted_for_summary_mode);

  if (options.is_fast_polling_supported) {
    DCHECK(!mdpinfo->task_runner) << "MemoryDumpProviders capable of fast "
                                     "polling must NOT be thread bound.";
  }

  {
    AutoLock lock(lock_);
    bool already_registered = !dump_providers_.insert(mdpinfo).second;
    // This actually happens in some tests which don't have a clean tear-down
    // path for RenderThreadImpl::Init().
    if (already_registered)
      return;

    if (options.is_fast_polling_supported)
      MemoryPeakDetector::GetInstance()->NotifyMemoryDumpProvidersChanged();

    if (ShouldEnableMDPAllocatorHooks(heap_profiling_mode_))
      NotifyHeapProfilingEnabledLocked(mdpinfo, true);
  }
}

void MemoryDumpManager::UnregisterDumpProvider(MemoryDumpProvider* mdp) {
  UnregisterDumpProviderInternal(mdp, false /* delete_async */);
}

void MemoryDumpManager::UnregisterAndDeleteDumpProviderSoon(
    std::unique_ptr<MemoryDumpProvider> mdp) {
  UnregisterDumpProviderInternal(mdp.release(), true /* delete_async */);
}

void MemoryDumpManager::UnregisterDumpProviderInternal(
    MemoryDumpProvider* mdp,
    bool take_mdp_ownership_and_delete_async) {
  std::unique_ptr<MemoryDumpProvider> owned_mdp;
  if (take_mdp_ownership_and_delete_async)
    owned_mdp.reset(mdp);

  AutoLock lock(lock_);

  auto mdp_iter = dump_providers_.begin();
  for (; mdp_iter != dump_providers_.end(); ++mdp_iter) {
    if ((*mdp_iter)->dump_provider == mdp)
      break;
  }

  if (mdp_iter == dump_providers_.end())
    return;  // Not registered / already unregistered.

  if (take_mdp_ownership_and_delete_async) {
    // The MDP will be deleted whenever the MDPInfo struct will, that is either:
    // - At the end of this function, if no dump is in progress.
    // - In ContinueAsyncProcessDump() when MDPInfo is removed from
    //   |pending_dump_providers|.
    // - When the provider is removed from other clients (MemoryPeakDetector).
    DCHECK(!(*mdp_iter)->owned_dump_provider);
    (*mdp_iter)->owned_dump_provider = std::move(owned_mdp);
  } else {
    // If you hit this DCHECK, your dump provider has a bug.
    // Unregistration of a MemoryDumpProvider is safe only if:
    // - The MDP has specified a sequenced task runner affinity AND the
    //   unregistration happens on the same task runner. So that the MDP cannot
    //   unregister and be in the middle of a OnMemoryDump() at the same time.
    // - The MDP has NOT specified a task runner affinity and its ownership is
    //   transferred via UnregisterAndDeleteDumpProviderSoon().
    // In all the other cases, it is not possible to guarantee that the
    // unregistration will not race with OnMemoryDump() calls.
    DCHECK((*mdp_iter)->task_runner &&
           (*mdp_iter)->task_runner->RunsTasksInCurrentSequence())
        << "MemoryDumpProvider \"" << (*mdp_iter)->name << "\" attempted to "
        << "unregister itself in a racy way. Please file a crbug.";
  }

  if ((*mdp_iter)->options.is_fast_polling_supported) {
    DCHECK(take_mdp_ownership_and_delete_async);
    MemoryPeakDetector::GetInstance()->NotifyMemoryDumpProvidersChanged();
  }

  // The MDPInfo instance can still be referenced by the
  // |ProcessMemoryDumpAsyncState.pending_dump_providers|. For this reason
  // the MDPInfo is flagged as disabled. It will cause InvokeOnMemoryDump()
  // to just skip it, without actually invoking the |mdp|, which might be
  // destroyed by the caller soon after this method returns.
  (*mdp_iter)->disabled = true;
  dump_providers_.erase(mdp_iter);
}

void MemoryDumpManager::GetDumpProvidersForPolling(
    std::vector<scoped_refptr<MemoryDumpProviderInfo>>* providers) {
  DCHECK(providers->empty());
  AutoLock lock(lock_);
  for (const scoped_refptr<MemoryDumpProviderInfo>& mdp : dump_providers_) {
    if (mdp->options.is_fast_polling_supported)
      providers->push_back(mdp);
  }
}

bool MemoryDumpManager::IsDumpProviderRegisteredForTesting(
    MemoryDumpProvider* provider) {
  AutoLock lock(lock_);

  for (const auto& info : dump_providers_) {
    if (info->dump_provider == provider)
      return true;
  }
  return false;
}

scoped_refptr<base::SequencedTaskRunner>
MemoryDumpManager::GetOrCreateBgTaskRunnerLocked() {
  lock_.AssertAcquired();

  if (dump_thread_)
    return dump_thread_->task_runner();

  dump_thread_ = std::make_unique<Thread>("MemoryInfra");
  bool started = dump_thread_->Start();
  CHECK(started);

  return dump_thread_->task_runner();
}

void MemoryDumpManager::CreateProcessDump(
    const MemoryDumpRequestArgs& args,
    const ProcessMemoryDumpCallback& callback) {
  char guid_str[20];
  sprintf(guid_str, "0x%" PRIx64, args.dump_guid);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(kTraceCategory, "ProcessMemoryDump",
                                    TRACE_ID_LOCAL(args.dump_guid), "dump_guid",
                                    TRACE_STR_COPY(guid_str));

  // If argument filter is enabled then only background mode dumps should be
  // allowed. In case the trace config passed for background tracing session
  // missed the allowed modes argument, it crashes here instead of creating
  // unexpected dumps.
  if (TraceLog::GetInstance()
          ->GetCurrentTraceConfig()
          .IsArgumentFilterEnabled()) {
    CHECK_EQ(MemoryDumpLevelOfDetail::BACKGROUND, args.level_of_detail);
  }

  std::unique_ptr<ProcessMemoryDumpAsyncState> pmd_async_state;
  {
    AutoLock lock(lock_);

    // MDM could have been disabled by this point destroying
    // |heap_profiler_serialization_state|. If heap profiling is enabled we
    // require session state so if heap profiling is on and session state is
    // absent we fail the dump immediately. If heap profiler is enabled during
    // the dump, then the dump succeeds since the dump was requested before, and
    // the future process dumps will contain heap dumps.
    if (args.dump_type != MemoryDumpType::SUMMARY_ONLY &&
        ShouldEnableMDPAllocatorHooks(heap_profiling_mode_) &&
        !heap_profiler_serialization_state_) {
      callback.Run(false /* success */, args.dump_guid, nullptr);
      return;
    }

    pmd_async_state.reset(new ProcessMemoryDumpAsyncState(
        args, dump_providers_, heap_profiler_serialization_state_, callback,
        GetOrCreateBgTaskRunnerLocked()));

    // If enabled, holds back the peak detector resetting its estimation window.
    MemoryPeakDetector::GetInstance()->Throttle();
  }

  // Start the process dump. This involves task runner hops as specified by the
  // MemoryDumpProvider(s) in RegisterDumpProvider()).
  ContinueAsyncProcessDump(pmd_async_state.release());
}

// Invokes OnMemoryDump() on all MDPs that are next in the pending list and run
// on the current sequenced task runner. If the next MDP does not run in current
// sequenced task runner, then switches to that task runner and continues. All
// OnMemoryDump() invocations are linearized. |lock_| is used in these functions
// purely to ensure consistency w.r.t. (un)registrations of |dump_providers_|.
void MemoryDumpManager::ContinueAsyncProcessDump(
    ProcessMemoryDumpAsyncState* owned_pmd_async_state) {
  HEAP_PROFILER_SCOPED_IGNORE;
  // Initalizes the ThreadLocalEventBuffer to guarantee that the TRACE_EVENTs
  // in the PostTask below don't end up registering their own dump providers
  // (for discounting trace memory overhead) while holding the |lock_|.
  TraceLog::GetInstance()->InitializeThreadLocalEventBufferIfSupported();

  // In theory |owned_pmd_async_state| should be a unique_ptr. The only reason
  // why it isn't is because of the corner case logic of |did_post_task|
  // above, which needs to take back the ownership of the |pmd_async_state| when
  // the PostTask() fails.
  // Unfortunately, PostTask() destroys the unique_ptr arguments upon failure
  // to prevent accidental leaks. Using a unique_ptr would prevent us to to
  // skip the hop and move on. Hence the manual naked -> unique ptr juggling.
  auto pmd_async_state = WrapUnique(owned_pmd_async_state);
  owned_pmd_async_state = nullptr;

  while (!pmd_async_state->pending_dump_providers.empty()) {
    // Read MemoryDumpProviderInfo thread safety considerations in
    // memory_dump_manager.h when accessing |mdpinfo| fields.
    MemoryDumpProviderInfo* mdpinfo =
        pmd_async_state->pending_dump_providers.back().get();

    if (!IsDumpProviderAllowedToDump(pmd_async_state->req_args, *mdpinfo)) {
      pmd_async_state->pending_dump_providers.pop_back();
      continue;
    }

    // If the dump provider did not specify a task runner affinity, dump on
    // |dump_thread_|.
    scoped_refptr<SequencedTaskRunner> task_runner = mdpinfo->task_runner;
    if (!task_runner) {
      DCHECK(mdpinfo->options.dumps_on_single_thread_task_runner);
      task_runner = pmd_async_state->dump_thread_task_runner;
      DCHECK(task_runner);
    }

    // If |RunsTasksInCurrentSequence()| is true then no PostTask is
    // required since we are on the right SequencedTaskRunner.
    if (task_runner->RunsTasksInCurrentSequence()) {
      InvokeOnMemoryDump(mdpinfo, pmd_async_state->process_memory_dump.get());
      pmd_async_state->pending_dump_providers.pop_back();
      continue;
    }

    bool did_post_task = task_runner->PostTask(
        FROM_HERE,
        BindOnce(&MemoryDumpManager::ContinueAsyncProcessDump, Unretained(this),
                 Unretained(pmd_async_state.get())));

    if (did_post_task) {
      // Ownership is tranferred to the posted task.
      ignore_result(pmd_async_state.release());
      return;
    }

    // PostTask usually fails only if the process or thread is shut down. So,
    // the dump provider is disabled here. But, don't disable unbound dump
    // providers, since the |dump_thread_| is controlled by MDM.
    if (mdpinfo->task_runner) {
      // A locked access is required to R/W |disabled| (for the
      // UnregisterAndDeleteDumpProviderSoon() case).
      AutoLock lock(lock_);
      mdpinfo->disabled = true;
    }

    // PostTask failed. Ignore the dump provider and continue.
    pmd_async_state->pending_dump_providers.pop_back();
  }

  FinishAsyncProcessDump(std::move(pmd_async_state));
}

bool MemoryDumpManager::IsDumpProviderAllowedToDump(
    const MemoryDumpRequestArgs& req_args,
    const MemoryDumpProviderInfo& mdpinfo) const {
  // If we are in background tracing, we should invoke only the whitelisted
  // providers. Ignore other providers and continue.
  if (req_args.level_of_detail == MemoryDumpLevelOfDetail::BACKGROUND &&
      !mdpinfo.whitelisted_for_background_mode) {
    return false;
  }

  // If we are in summary mode, we only need to invoke the providers
  // whitelisted for summary mode.
  if (req_args.dump_type == MemoryDumpType::SUMMARY_ONLY &&
      !mdpinfo.whitelisted_for_summary_mode) {
    return false;
  }

  return true;
}

// This function is called on the right task runner for current MDP. It is
// either the task runner specified by MDP or |dump_thread_task_runner| if the
// MDP did not specify task runner. Invokes the dump provider's OnMemoryDump()
// (unless disabled).
void MemoryDumpManager::InvokeOnMemoryDump(MemoryDumpProviderInfo* mdpinfo,
                                           ProcessMemoryDump* pmd) {
  HEAP_PROFILER_SCOPED_IGNORE;
  DCHECK(!mdpinfo->task_runner ||
         mdpinfo->task_runner->RunsTasksInCurrentSequence());

  TRACE_EVENT1(kTraceCategory, "MemoryDumpManager::InvokeOnMemoryDump",
               "dump_provider.name", mdpinfo->name);

  // Do not add any other TRACE_EVENT macro (or function that might have them)
  // below this point. Under some rare circunstances, they can re-initialize
  // and invalide the current ThreadLocalEventBuffer MDP, making the
  // |should_dump| check below susceptible to TOCTTOU bugs
  // (https://crbug.com/763365).

  bool is_thread_bound;
  {
    // A locked access is required to R/W |disabled| (for the
    // UnregisterAndDeleteDumpProviderSoon() case).
    AutoLock lock(lock_);

    // Unregister the dump provider if it failed too many times consecutively.
    if (!mdpinfo->disabled &&
        mdpinfo->consecutive_failures >= kMaxConsecutiveFailuresCount) {
      mdpinfo->disabled = true;
      DLOG(ERROR) << "Disabling MemoryDumpProvider \"" << mdpinfo->name
                  << "\". Dump failed multiple times consecutively.";
    }
    if (mdpinfo->disabled)
      return;

    is_thread_bound = mdpinfo->task_runner != nullptr;
  }  // AutoLock lock(lock_);

  // Invoke the dump provider.

  // A stack allocated string with dump provider name is useful to debug
  // crashes while invoking dump after a |dump_provider| is not unregistered
  // in safe way.
  char provider_name_for_debugging[16];
  strncpy(provider_name_for_debugging, mdpinfo->name,
          sizeof(provider_name_for_debugging) - 1);
  provider_name_for_debugging[sizeof(provider_name_for_debugging) - 1] = '\0';
  base::debug::Alias(provider_name_for_debugging);

  ANNOTATE_BENIGN_RACE(&mdpinfo->disabled, "best-effort race detection");
  CHECK(!is_thread_bound ||
        !*(static_cast<volatile bool*>(&mdpinfo->disabled)));
  bool dump_successful =
      mdpinfo->dump_provider->OnMemoryDump(pmd->dump_args(), pmd);
  mdpinfo->consecutive_failures =
      dump_successful ? 0 : mdpinfo->consecutive_failures + 1;
}

void MemoryDumpManager::FinishAsyncProcessDump(
    std::unique_ptr<ProcessMemoryDumpAsyncState> pmd_async_state) {
  HEAP_PROFILER_SCOPED_IGNORE;
  DCHECK(pmd_async_state->pending_dump_providers.empty());
  const uint64_t dump_guid = pmd_async_state->req_args.dump_guid;
  if (!pmd_async_state->callback_task_runner->BelongsToCurrentThread()) {
    scoped_refptr<SingleThreadTaskRunner> callback_task_runner =
        pmd_async_state->callback_task_runner;
    callback_task_runner->PostTask(
        FROM_HERE, BindOnce(&MemoryDumpManager::FinishAsyncProcessDump,
                            Unretained(this), std::move(pmd_async_state)));
    return;
  }

  TRACE_EVENT0(kTraceCategory, "MemoryDumpManager::FinishAsyncProcessDump");

  // In the general case (allocators and edges) the serialization into the trace
  // buffer is handled by the memory-infra service (see tracing_observer.cc).
  // This special case below deals only with serialization of the heap profiler
  // and is temporary given the upcoming work on the out-of-process heap
  // profiler.
  const auto& args = pmd_async_state->req_args;
  if (!pmd_async_state->process_memory_dump->heap_dumps().empty()) {
    std::unique_ptr<TracedValue> traced_value = std::make_unique<TracedValue>();
    pmd_async_state->process_memory_dump->SerializeHeapProfilerDumpsInto(
        traced_value.get());

    traced_value->SetString("level_of_detail",
                            base::trace_event::MemoryDumpLevelOfDetailToString(
                                args.level_of_detail));
    std::unique_ptr<base::trace_event::ConvertableToTraceFormat> event_value(
        std::move(traced_value));
    TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_PROCESS_ID(
        TRACE_EVENT_PHASE_MEMORY_DUMP,
        base::trace_event::TraceLog::GetCategoryGroupEnabled(
            base::trace_event::MemoryDumpManager::kTraceCategory),
        base::trace_event::MemoryDumpTypeToString(args.dump_type),
        trace_event_internal::kGlobalScope, args.dump_guid,
        base::kNullProcessId, 1 /* num_args */, kTraceEventArgNames,
        kTraceEventArgTypes, nullptr /* arg_values */, &event_value,
        TRACE_EVENT_FLAG_HAS_ID);
  }

  if (!pmd_async_state->callback.is_null()) {
    pmd_async_state->callback.Run(
        true /* success */, dump_guid,
        std::move(pmd_async_state->process_memory_dump));
    pmd_async_state->callback.Reset();
  }

  TRACE_EVENT_NESTABLE_ASYNC_END0(kTraceCategory, "ProcessMemoryDump",
                                  TRACE_ID_LOCAL(dump_guid));
}

void MemoryDumpManager::SetupForTracing(
    const TraceConfig::MemoryDumpConfig& memory_dump_config) {
  AutoLock lock(lock_);
  heap_profiler_serialization_state_ = new HeapProfilerSerializationState();
  heap_profiler_serialization_state_
      ->set_heap_profiler_breakdown_threshold_bytes(
          memory_dump_config.heap_profiler_options.breakdown_threshold_bytes);
  InitializeHeapProfilerStateIfNeededLocked();

  // At this point we must have the ability to request global dumps.
  DCHECK(can_request_global_dumps());

  MemoryDumpScheduler::Config periodic_config;
  bool peak_detector_configured = false;
  for (const auto& trigger : memory_dump_config.triggers) {
    if (trigger.trigger_type == MemoryDumpType::PERIODIC_INTERVAL) {
      if (periodic_config.triggers.empty()) {
        periodic_config.callback =
            BindRepeating(&DoGlobalDumpWithoutCallback, request_dump_function_,
                          MemoryDumpType::PERIODIC_INTERVAL);
      }
      periodic_config.triggers.push_back(
          {trigger.level_of_detail, trigger.min_time_between_dumps_ms});
    } else if (trigger.trigger_type == MemoryDumpType::PEAK_MEMORY_USAGE) {
      // At most one peak trigger is allowed.
      CHECK(!peak_detector_configured);
      peak_detector_configured = true;
      MemoryPeakDetector::GetInstance()->Setup(
          BindRepeating(&MemoryDumpManager::GetDumpProvidersForPolling,
                        Unretained(this)),
          GetOrCreateBgTaskRunnerLocked(),
          BindRepeating(&DoGlobalDumpWithoutCallback, request_dump_function_,
                        MemoryDumpType::PEAK_MEMORY_USAGE,
                        trigger.level_of_detail));

      MemoryPeakDetector::Config peak_config;
      peak_config.polling_interval_ms = 10;
      peak_config.min_time_between_peaks_ms = trigger.min_time_between_dumps_ms;
      peak_config.enable_verbose_poll_tracing =
          trigger.level_of_detail == MemoryDumpLevelOfDetail::DETAILED;
      MemoryPeakDetector::GetInstance()->Start(peak_config);

      // When peak detection is enabled, trigger a dump straight away as it
      // gives a good reference point for analyzing the trace.
      if (is_coordinator_) {
        GetOrCreateBgTaskRunnerLocked()->PostTask(
            FROM_HERE,
            BindRepeating(&DoGlobalDumpWithoutCallback, request_dump_function_,
                          MemoryDumpType::PEAK_MEMORY_USAGE,
                          trigger.level_of_detail));
      }
    }
  }

  // Only coordinator process triggers periodic memory dumps.
  if (is_coordinator_ && !periodic_config.triggers.empty()) {
    MemoryDumpScheduler::GetInstance()->Start(periodic_config,
                                              GetOrCreateBgTaskRunnerLocked());
  }
}

void MemoryDumpManager::TeardownForTracing() {
  // There might be a memory dump in progress while this happens. Therefore,
  // ensure that the MDM state which depends on the tracing enabled / disabled
  // state is always accessed by the dumping methods holding the |lock_|.
  AutoLock lock(lock_);

  MemoryDumpScheduler::GetInstance()->Stop();
  MemoryPeakDetector::GetInstance()->TearDown();
  heap_profiler_serialization_state_ = nullptr;
}

void MemoryDumpManager::InitializeHeapProfilerStateIfNeededLocked() {
  lock_.AssertAcquired();
  if (!ShouldEnableMDPAllocatorHooks(heap_profiling_mode_) ||
      !heap_profiler_serialization_state_ ||
      heap_profiler_serialization_state_->is_initialized()) {
    return;
  }
  // If heap profiling is enabled, the stack frame deduplicator and type name
  // deduplicator will be in use. Add a metadata events to write the frames
  // and type IDs.
  heap_profiler_serialization_state_->SetStackFrameDeduplicator(
      WrapUnique(new StackFrameDeduplicator));
  heap_profiler_serialization_state_->SetTypeNameDeduplicator(
      WrapUnique(new TypeNameDeduplicator));

  TRACE_EVENT_API_ADD_METADATA_EVENT(
      TraceLog::GetCategoryGroupEnabled("__metadata"), "stackFrames",
      "stackFrames",
      std::make_unique<SessionStateConvertableProxy<StackFrameDeduplicator>>(
          heap_profiler_serialization_state_,
          &HeapProfilerSerializationState::stack_frame_deduplicator));

  TRACE_EVENT_API_ADD_METADATA_EVENT(
      TraceLog::GetCategoryGroupEnabled("__metadata"), "typeNames", "typeNames",
      std::make_unique<SessionStateConvertableProxy<TypeNameDeduplicator>>(
          heap_profiler_serialization_state_,
          &HeapProfilerSerializationState::type_name_deduplicator));
}

void MemoryDumpManager::NotifyHeapProfilingEnabledLocked(
    scoped_refptr<MemoryDumpProviderInfo> mdpinfo,
    bool enabled) {
  lock_.AssertAcquired();
  if (!mdpinfo->options.supports_heap_profiling)
    return;

  const auto& task_runner = mdpinfo->task_runner
                                ? mdpinfo->task_runner
                                : GetOrCreateBgTaskRunnerLocked();
  // TODO(ssid): Post tasks only for MDPs that support heap profiling.
  task_runner->PostTask(
      FROM_HERE,
      BindOnce(&NotifyHeapProfilingEnabledOnMDPThread, mdpinfo, enabled));
}

MemoryDumpManager::ProcessMemoryDumpAsyncState::ProcessMemoryDumpAsyncState(
    MemoryDumpRequestArgs req_args,
    const MemoryDumpProviderInfo::OrderedSet& dump_providers,
    scoped_refptr<HeapProfilerSerializationState>
        heap_profiler_serialization_state_in,
    ProcessMemoryDumpCallback callback,
    scoped_refptr<SequencedTaskRunner> dump_thread_task_runner)
    : req_args(req_args),
      heap_profiler_serialization_state(
          std::move(heap_profiler_serialization_state_in)),
      callback(callback),
      callback_task_runner(ThreadTaskRunnerHandle::Get()),
      dump_thread_task_runner(std::move(dump_thread_task_runner)) {
  pending_dump_providers.reserve(dump_providers.size());
  pending_dump_providers.assign(dump_providers.rbegin(), dump_providers.rend());
  MemoryDumpArgs args = {req_args.level_of_detail, req_args.dump_guid};
  process_memory_dump =
      MakeUnique<ProcessMemoryDump>(heap_profiler_serialization_state, args);
}

MemoryDumpManager::ProcessMemoryDumpAsyncState::~ProcessMemoryDumpAsyncState() =
    default;

}  // namespace trace_event
}  // namespace base
