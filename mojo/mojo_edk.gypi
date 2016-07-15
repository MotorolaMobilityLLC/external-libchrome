# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'mojo_edk_ports_sources': [
      'edk/system/ports/event.cc',
      'edk/system/ports/event.h',
      'edk/system/ports/message.cc',
      'edk/system/ports/message.h',
      'edk/system/ports/message_queue.cc',
      'edk/system/ports/message_queue.h',
      'edk/system/ports/name.cc',
      'edk/system/ports/name.h',
      'edk/system/ports/node.cc',
      'edk/system/ports/node.h',
      'edk/system/ports/node_delegate.h',
      'edk/system/ports/port.cc',
      'edk/system/ports/port.h',
      'edk/system/ports/port_ref.cc',
      'edk/system/ports/user_data.h',
      'edk/system/ports_message.cc',
      'edk/system/ports_message.h',
    ],
    'mojo_edk_system_impl_sources': [
      'edk/embedder/configuration.h',
      'edk/embedder/embedder.cc',
      'edk/embedder/embedder.h',
      'edk/embedder/embedder_internal.h',
      'edk/embedder/entrypoints.cc',
      'edk/embedder/named_platform_channel_pair_win.cc',
      'edk/embedder/named_platform_channel_pair.h',
      'edk/embedder/platform_channel_pair.cc',
      'edk/embedder/platform_channel_pair.h',
      'edk/embedder/platform_channel_pair_posix.cc',
      'edk/embedder/platform_channel_pair_win.cc',
      'edk/embedder/platform_handle.cc',
      'edk/embedder/platform_handle.h',
      'edk/embedder/platform_handle_utils.h',
      'edk/embedder/platform_handle_utils_posix.cc',
      'edk/embedder/platform_handle_utils_win.cc',
      'edk/embedder/platform_handle_vector.h',
      'edk/embedder/platform_shared_buffer.cc',
      'edk/embedder/platform_shared_buffer.h',
      'edk/embedder/scoped_ipc_support.cc',
      'edk/embedder/scoped_ipc_support.h',
      'edk/embedder/scoped_platform_handle.h',
      'edk/system/awakable.h',
      'edk/system/awakable_list.cc',
      'edk/system/awakable_list.h',
      'edk/system/atomic_flag.h',
      'edk/system/broker.h',
      'edk/system/broker_host.h',
      'edk/system/channel.cc',
      'edk/system/channel.h',
      'edk/system/channel_win.cc',
      'edk/system/configuration.cc',
      'edk/system/configuration.h',
      'edk/system/core.cc',
      'edk/system/core.h',
      'edk/system/data_pipe_consumer_dispatcher.cc',
      'edk/system/data_pipe_consumer_dispatcher.h',
      'edk/system/data_pipe_control_message.cc',
      'edk/system/data_pipe_control_message.h',
      'edk/system/data_pipe_producer_dispatcher.cc',
      'edk/system/data_pipe_producer_dispatcher.h',
      'edk/system/dispatcher.cc',
      'edk/system/dispatcher.h',
      'edk/system/handle_signals_state.h',
      'edk/system/handle_table.cc',
      'edk/system/handle_table.h',
      'edk/system/mapping_table.cc',
      'edk/system/mapping_table.h',
      'edk/system/message_for_transit.cc',
      'edk/system/message_for_transit.h',
      'edk/system/message_pipe_dispatcher.cc',
      'edk/system/message_pipe_dispatcher.h',
      'edk/system/node_channel.cc',
      'edk/system/node_channel.h',
      'edk/system/node_controller.cc',
      'edk/system/node_controller.h',
      'edk/system/options_validation.h',
      'edk/system/platform_handle_dispatcher.cc',
      'edk/system/platform_handle_dispatcher.h',
      'edk/system/remote_message_pipe_bootstrap.h',
      'edk/system/request_context.cc',
      'edk/system/request_context.h',
      'edk/system/shared_buffer_dispatcher.cc',
      'edk/system/shared_buffer_dispatcher.h',
      'edk/system/wait_set_dispatcher.cc',
      'edk/system/wait_set_dispatcher.h',
      'edk/system/waiter.cc',
      'edk/system/waiter.h',
      'edk/system/watcher.cc',
      'edk/system/watcher.h',
      'edk/system/watcher_set.cc',
      'edk/system/watcher_set.h',
      # Test-only code:
      # TODO(vtl): It's a little unfortunate that these end up in the same
      # component as non-test-only code. In the static build, this code
      # should hopefully be dead-stripped.
      'edk/embedder/test_embedder.cc',
      'edk/embedder/test_embedder.h',
    ],
    'mojo_edk_system_impl_non_nacl_sources': [
      'edk/embedder/platform_channel_utils_posix.cc',
      'edk/embedder/platform_channel_utils_posix.h',
      'edk/system/broker_host_posix.cc',
      'edk/system/broker_posix.cc',
      'edk/system/channel_posix.cc',
      'edk/system/remote_message_pipe_bootstrap.cc',
    ],
    'mojo_edk_system_impl_nacl_nonsfi_sources': [
      'edk/embedder/platform_channel_utils_posix.cc',
      'edk/embedder/platform_channel_utils_posix.h',
      'edk/system/broker_posix.cc',
      'edk/system/channel_posix.cc',
    ],
  },
}
