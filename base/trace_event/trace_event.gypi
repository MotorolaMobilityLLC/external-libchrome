# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'trace_event_sources' : [
      'trace_event/common/trace_event_common.h',
      'trace_event/heap_profiler_allocation_context.cc',
      'trace_event/heap_profiler_allocation_context.h',
      'trace_event/heap_profiler_allocation_context_tracker.cc',
      'trace_event/heap_profiler_allocation_context_tracker.h',
      'trace_event/heap_profiler_allocation_register.cc',
      'trace_event/heap_profiler_allocation_register_posix.cc',
      'trace_event/heap_profiler_allocation_register_win.cc',
      'trace_event/heap_profiler_allocation_register.h',
      'trace_event/heap_profiler_heap_dump_writer.cc',
      'trace_event/heap_profiler_heap_dump_writer.h',
      'trace_event/heap_profiler_stack_frame_deduplicator.cc',
      'trace_event/heap_profiler_stack_frame_deduplicator.h',
      'trace_event/heap_profiler_type_name_deduplicator.cc',
      'trace_event/heap_profiler_type_name_deduplicator.h',
      'trace_event/java_heap_dump_provider_android.cc',
      'trace_event/java_heap_dump_provider_android.h',
      'trace_event/memory_allocator_dump.cc',
      'trace_event/memory_allocator_dump.h',
      'trace_event/memory_allocator_dump_guid.cc',
      'trace_event/memory_allocator_dump_guid.h',
      'trace_event/memory_dump_manager.cc',
      'trace_event/memory_dump_manager.h',
      'trace_event/memory_dump_provider.h',
      'trace_event/memory_dump_request_args.cc',
      'trace_event/memory_dump_request_args.h',
      'trace_event/memory_dump_session_state.cc',
      'trace_event/memory_dump_session_state.h',
      'trace_event/process_memory_dump.cc',
      'trace_event/process_memory_dump.h',
      'trace_event/process_memory_maps.cc',
      'trace_event/process_memory_maps.h',
      'trace_event/process_memory_totals.cc',
      'trace_event/process_memory_totals.h',
      'trace_event/trace_buffer.cc',
      'trace_event/trace_buffer.h',
      'trace_event/trace_config.cc',
      'trace_event/trace_config.h',
      'trace_event/trace_event.h',
      'trace_event/trace_event_android.cc',
      'trace_event/trace_event_argument.cc',
      'trace_event/trace_event_argument.h',
      'trace_event/trace_event_etw_export_win.cc',
      'trace_event/trace_event_etw_export_win.h',
      'trace_event/trace_event_impl.cc',
      'trace_event/trace_event_impl.h',
      'trace_event/trace_event_memory_overhead.cc',
      'trace_event/trace_event_memory_overhead.h',
      'trace_event/trace_event_synthetic_delay.cc',
      'trace_event/trace_event_synthetic_delay.h',
      'trace_event/trace_event_system_stats_monitor.cc',
      'trace_event/trace_event_system_stats_monitor.h',
      'trace_event/trace_log.cc',
      'trace_event/trace_log.h',
      'trace_event/trace_log_constants.cc',
      'trace_event/trace_sampling_thread.cc',
      'trace_event/trace_sampling_thread.h',
      'trace_event/tracing_agent.cc',
      'trace_event/tracing_agent.h',
      'trace_event/winheap_dump_provider_win.cc',
      'trace_event/winheap_dump_provider_win.h',
    ],
    'trace_event_test_sources' : [
      'trace_event/heap_profiler_allocation_context_tracker_unittest.cc',
      'trace_event/heap_profiler_allocation_register_unittest.cc',
      'trace_event/heap_profiler_heap_dump_writer_unittest.cc',
      'trace_event/heap_profiler_stack_frame_deduplicator_unittest.cc',
      'trace_event/heap_profiler_type_name_deduplicator_unittest.cc',
      'trace_event/java_heap_dump_provider_android_unittest.cc',
      'trace_event/memory_allocator_dump_unittest.cc',
      'trace_event/memory_dump_manager_unittest.cc',
      'trace_event/process_memory_dump_unittest.cc',
      'trace_event/trace_config_memory_test_util.h',
      'trace_event/trace_config_unittest.cc',
      'trace_event/trace_event_argument_unittest.cc',
      'trace_event/trace_event_synthetic_delay_unittest.cc',
      'trace_event/trace_event_system_stats_monitor_unittest.cc',
      'trace_event/trace_event_unittest.cc',
      'trace_event/winheap_dump_provider_win_unittest.cc',
    ],
    'conditions': [
      ['OS == "linux" or OS=="android" or OS=="mac"', {
        'trace_event_sources': [
          'trace_event/malloc_dump_provider.cc',
          'trace_event/malloc_dump_provider.h',
        ],
      }],
      ['OS == "android"', {
        'trace_event_test_sources' : [
          'trace_event/trace_event_android_unittest.cc',
        ],
      }],
    ],
  },
}
