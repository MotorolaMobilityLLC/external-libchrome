# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'mojo_variables.gypi',
  ],
  'targets': [
    {
      'target_name': 'mojo_edk_tests',
      'type': 'none',
      'dependencies': [
        # NOTE: If adding a new dependency here, please consider whether it
        # should also be added to the list of Mojo-related dependencies of
        # build/all.gyp:All on iOS, as All cannot depend on the mojo_base
        # target on iOS due to the presence of the js targets, which cause v8
        # to be built.
        'mojo_message_pipe_perftests',
        'mojo_public_application_unittests',
        'mojo_public_bindings_unittests',
        'mojo_public_environment_unittests',
        'mojo_public_system_perftests',
        'mojo_public_system_unittests',
        'mojo_public_utility_unittests',
        'mojo_system_unittests',
        'mojo_js_unittests',
        'mojo_js_integration_tests',
      ],
    },
    # TODO(vtl): Reorganize the mojo_public_*_unittests.
    {
      # GN version: //mojo/edk/test:mojo_public_bindings_unittests
      'target_name': 'mojo_public_bindings_unittests',
      'type': 'executable',
      'dependencies': [
        '../testing/gtest.gyp:gtest',
        'mojo_edk.gyp:mojo_run_all_unittests',
        'mojo_public.gyp:mojo_cpp_bindings',
        'mojo_public.gyp:mojo_environment_standalone',
        'mojo_public.gyp:mojo_public_bindings_test_utils',
        'mojo_public.gyp:mojo_public_test_interfaces',
        'mojo_public.gyp:mojo_public_test_utils',
        'mojo_public.gyp:mojo_utility',
      ],
      'sources': [
        'public/cpp/bindings/tests/array_unittest.cc',
        'public/cpp/bindings/tests/bounds_checker_unittest.cc',
        'public/cpp/bindings/tests/buffer_unittest.cc',
        'public/cpp/bindings/tests/connector_unittest.cc',
        'public/cpp/bindings/tests/container_test_util.cc',
        'public/cpp/bindings/tests/equals_unittest.cc',
        'public/cpp/bindings/tests/handle_passing_unittest.cc',
        'public/cpp/bindings/tests/interface_ptr_unittest.cc',
        'public/cpp/bindings/tests/map_unittest.cc',
        'public/cpp/bindings/tests/request_response_unittest.cc',
        'public/cpp/bindings/tests/router_unittest.cc',
        'public/cpp/bindings/tests/sample_service_unittest.cc',
        'public/cpp/bindings/tests/serialization_warning_unittest.cc',
        'public/cpp/bindings/tests/string_unittest.cc',
        'public/cpp/bindings/tests/struct_unittest.cc',
        'public/cpp/bindings/tests/type_conversion_unittest.cc',
        'public/cpp/bindings/tests/validation_unittest.cc',
      ],
    },
    {
      # GN version: //mojo/edk/test:mojo_public_environment_unittests
      'target_name': 'mojo_public_environment_unittests',
      'type': 'executable',
      'dependencies': [
        '../testing/gtest.gyp:gtest',
        'mojo_edk.gyp:mojo_run_all_unittests',
        'mojo_public.gyp:mojo_environment_standalone',
        'mojo_public.gyp:mojo_public_test_utils',
        'mojo_public.gyp:mojo_utility',
      ],
      'include_dirs': [ '..' ],
      'sources': [
        'public/cpp/environment/tests/async_wait_unittest.cc',
        'public/cpp/environment/tests/async_waiter_unittest.cc',
        'public/cpp/environment/tests/logger_unittest.cc',
        'public/cpp/environment/tests/logging_unittest.cc',
      ],
    },
    {
      # GN version: //mojo/edk/test:mojo_public_application_unittests
      'target_name': 'mojo_public_application_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_edk.gyp:mojo_run_all_unittests',
        'mojo_public.gyp:mojo_application_standalone',
        'mojo_public.gyp:mojo_utility',
        'mojo_public.gyp:mojo_environment_standalone',
      ],
      'sources': [
        'public/cpp/application/tests/service_registry_unittest.cc',
      ],
    },
    {
      # GN version: //mojo/public/cpp/system/tests:mojo_public_system_unittests
      # and         //mojo/public/c/system/tests
      'target_name': 'mojo_public_system_unittests',
      'type': 'executable',
      'dependencies': [
        '../testing/gtest.gyp:gtest',
        'mojo_edk.gyp:mojo_run_all_unittests',
        'mojo_public.gyp:mojo_public_test_utils',
      ],
      'include_dirs': [ '..' ],
      'sources': [
        '<@(mojo_public_system_unittest_sources)',
      ],
    },
    {
      # GN version: //mojo/public/cpp/application/tests:mojo_public_utility_unittests
      'target_name': 'mojo_public_utility_unittests',
      'type': 'executable',
      'dependencies': [
        '../testing/gtest.gyp:gtest',
        'mojo_edk.gyp:mojo_run_all_unittests',
        'mojo_public.gyp:mojo_public_test_utils',
        'mojo_public.gyp:mojo_utility',
      ],
      'include_dirs': [ '..' ],
      'sources': [
        'public/cpp/utility/tests/mutex_unittest.cc',
        'public/cpp/utility/tests/run_loop_unittest.cc',
        'public/cpp/utility/tests/thread_unittest.cc',
      ],
      'conditions': [
        # See crbug.com/342893:
        ['OS=="win"', {
          'sources!': [
            'public/cpp/utility/tests/mutex_unittest.cc',
            'public/cpp/utility/tests/thread_unittest.cc',
          ],
        }],
      ],
    },
    {
      # GN version: //mojo/edk/test:mojo_public_system_perftests
      'target_name': 'mojo_public_system_perftests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_edk.gyp:mojo_run_all_perftests',
        'mojo_public.gyp:mojo_public_test_utils',
        'mojo_public.gyp:mojo_utility',
      ],
      'sources': [
        'public/c/system/tests/core_perftest.cc',
      ],
    },
    {
      # GN version: //mojo/edk/system:mojo_system_unittests
      'target_name': 'mojo_system_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_edk.gyp:mojo_common_test_support',
        'mojo_edk.gyp:mojo_system_impl',
      ],
      'sources': [
        'edk/embedder/embedder_unittest.cc',
        'edk/embedder/platform_channel_pair_posix_unittest.cc',
        'edk/embedder/simple_platform_shared_buffer_unittest.cc',
        'edk/system/awakable_list_unittest.cc',
        'edk/system/channel_endpoint_id_unittest.cc',
        'edk/system/channel_manager_unittest.cc',
        'edk/system/channel_unittest.cc',
        'edk/system/core_unittest.cc',
        'edk/system/core_test_base.cc',
        'edk/system/core_test_base.h',
        'edk/system/data_pipe_unittest.cc',
        'edk/system/dispatcher_unittest.cc',
        'edk/system/local_data_pipe_unittest.cc',
        'edk/system/memory_unittest.cc',
        'edk/system/message_pipe_dispatcher_unittest.cc',
        'edk/system/message_pipe_test_utils.h',
        'edk/system/message_pipe_test_utils.cc',
        'edk/system/message_pipe_unittest.cc',
        'edk/system/multiprocess_message_pipe_unittest.cc',
        'edk/system/options_validation_unittest.cc',
        'edk/system/platform_handle_dispatcher_unittest.cc',
        'edk/system/raw_channel_unittest.cc',
        'edk/system/remote_message_pipe_unittest.cc',
        'edk/system/run_all_unittests.cc',
        'edk/system/shared_buffer_dispatcher_unittest.cc',
        'edk/system/simple_dispatcher_unittest.cc',
        'edk/system/test_utils.cc',
        'edk/system/test_utils.h',
        'edk/system/waiter_test_utils.cc',
        'edk/system/waiter_test_utils.h',
        'edk/system/waiter_unittest.cc',
        'edk/test/multiprocess_test_helper_unittest.cc',
      ],
      'conditions': [
        ['OS=="ios"', {
          'sources!': [
            'edk/embedder/embedder_unittest.cc',
            'edk/system/multiprocess_message_pipe_unittest.cc',
            'edk/test/multiprocess_test_helper_unittest.cc',
          ],
        }],
      ],
    },
    {
      # GN version: //mojo/edk/system:mojo_message_pipe_perftests
      'target_name': 'mojo_message_pipe_perftests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../base/base.gyp:test_support_perf',
        '../testing/gtest.gyp:gtest',
        'mojo_edk.gyp:mojo_common_test_support',
        'mojo_edk.gyp:mojo_system_impl',
      ],
      'sources': [
        'edk/system/message_pipe_perftest.cc',
        'edk/system/message_pipe_test_utils.h',
        'edk/system/message_pipe_test_utils.cc',
        'edk/system/test_utils.cc',
        'edk/system/test_utils.h',
      ],
    },
    {
      # GN version: //mojo/edk/js/test:js_unittests
      'target_name': 'mojo_js_unittests',
      'type': 'executable',
      'dependencies': [
        '../gin/gin.gyp:gin_test',
        'mojo_edk.gyp:mojo_common_test_support',
        'mojo_edk.gyp:mojo_run_all_unittests',
        'mojo_edk.gyp:mojo_js_lib',
        'mojo_public.gyp:mojo_environment_standalone',
        'mojo_public.gyp:mojo_public_test_interfaces',
        'mojo_public.gyp:mojo_utility',
      ],
      'sources': [
        'edk/js/handle_unittest.cc',
        'edk/js/test/run_js_tests.cc',
      ],
    },
    {
      # GN version: //mojo/edk/js/test:js_integration_tests
      'target_name': 'mojo_js_integration_tests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../gin/gin.gyp:gin_test',
        'mojo_public.gyp:mojo_environment_standalone',
        'mojo_public.gyp:mojo_public_test_interfaces',
        'mojo_public.gyp:mojo_utility',
        'mojo_edk.gyp:mojo_js_lib',
        'mojo_edk.gyp:mojo_run_all_unittests',
        'mojo_js_to_cpp_bindings',
      ],
      'sources': [
        'edk/js/test/run_js_integration_tests.cc',
        'edk/js/tests/js_to_cpp_tests',
      ],
    },
    {
      'target_name': 'mojo_js_to_cpp_bindings',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'edk/js/tests/js_to_cpp.mojom',
        ],
      },
      'includes': [ 'mojom_bindings_generator_explicit.gypi' ],
    },
  ],
}
