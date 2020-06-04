{
  'targets': [
    {
      'target_name': 'mojo_system',
      'type': 'shared_library',
      'defines': [
        'MOJO_SYSTEM_IMPLEMENTATION',
      ],
      'include_dirs': [
        '..',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'sources': [
        'public/c/system/async_waiter.h',
        'public/c/system/core.h',
        'public/c/system/macros.h',
        'public/c/system/system_export.h',
        'public/cpp/system/core.h',
        'public/cpp/system/macros.h',
        'public/system/core_private.cc',
        'public/system/core_private.h',
      ],
      'conditions': [
        ['OS=="mac"', {
          'xcode_settings': {
            # Make it a run-path dependent library.
            'DYLIB_INSTALL_NAME_BASE': '@rpath',
          },
          'direct_dependent_settings': {
            'xcode_settings': {
              # Look for run-path dependent libraries in the loader's directory.
              'LD_RUNPATH_SEARCH_PATHS': [ '@loader_path/.', ],
            },
          },
        }],
      ],
    },
    {
      'target_name': 'mojo_gles2',
      'type': 'shared_library',
      'defines': [
        'MOJO_GLES2_IMPLEMENTATION',
        'GLES2_USE_MOJO',
      ],
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../third_party/khronos/khronos.gyp:khronos_headers'
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
        'defines': [
          'GLES2_USE_MOJO',
        ],
      },
      'sources': [
        'public/gles2/gles2.h',
        'public/gles2/gles2_export.h',
        'public/gles2/gles2_private.cc',
        'public/gles2/gles2_private.h',
      ],
      'conditions': [
        ['OS=="mac"', {
          'xcode_settings': {
            # Make it a run-path dependent library.
            'DYLIB_INSTALL_NAME_BASE': '@rpath',
          },
          'direct_dependent_settings': {
            'xcode_settings': {
              # Look for run-path dependent libraries in the loader's directory.
              'LD_RUNPATH_SEARCH_PATHS': [ '@loader_path/.', ],
            },
          },
        }],
      ],
    },
    {
      'target_name': 'mojo_test_support',
      'type': 'shared_library',
      'defines': [
        'MOJO_TEST_SUPPORT_IMPLEMENTATION',
      ],
      'include_dirs': [
        '..',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'sources': [
        'public/tests/test_support.h',
        'public/tests/test_support_private.cc',
        'public/tests/test_support_private.h',
        'public/tests/test_support_export.h',
      ],
      'conditions': [
        ['OS=="mac"', {
          'xcode_settings': {
            # Make it a run-path dependent library.
            'DYLIB_INSTALL_NAME_BASE': '@rpath',
          },
          'direct_dependent_settings': {
            'xcode_settings': {
              # Look for run-path dependent libraries in the loader's directory.
              'LD_RUNPATH_SEARCH_PATHS': [ '@loader_path/.', ],
            },
          },
        }],
      ],
    },
    {
      'target_name': 'mojo_public_test_utils',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_system',
        'mojo_test_support',
      ],
      'sources': [
        'public/tests/test_utils.cc',
        'public/tests/test_utils.h',
      ],
    },
    # TODO(vtl): Reorganize the mojo_public_*_unittests.
    {
      'target_name': 'mojo_public_bindings_unittests',
      'type': 'executable',
      'dependencies': [
        '../testing/gtest.gyp:gtest',
        'mojo_bindings',
        'mojo_environment_standalone',
        'mojo_public_test_utils',
        'mojo_run_all_unittests',
        'mojo_sample_service',
        'mojo_system',
        'mojo_utility',
      ],
      'sources': [
        'public/bindings/tests/array_unittest.cc',
        'public/bindings/tests/buffer_unittest.cc',
        'public/bindings/tests/connector_unittest.cc',
        'public/bindings/tests/handle_passing_unittest.cc',
        'public/bindings/tests/math_calculator.mojom',
        'public/bindings/tests/remote_ptr_unittest.cc',
        'public/bindings/tests/request_response_unittest.cc',
        'public/bindings/tests/router_unittest.cc',
        'public/bindings/tests/sample_factory.mojom',
        'public/bindings/tests/sample_interfaces.mojom',
        'public/bindings/tests/sample_service_unittest.cc',
        'public/bindings/tests/test_structs.mojom',
        'public/bindings/tests/type_conversion_unittest.cc',
      ],
      'variables': {
        'mojom_base_output_dir': 'mojo',
      },
      'includes': [ 'public/bindings/mojom_bindings_generator.gypi' ],
    },
    {
      'target_name': 'mojo_public_environment_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_environment_standalone',
        'mojo_public_test_utils',
        'mojo_run_all_unittests',
        'mojo_system',
        'mojo_utility',
      ],
      'sources': [
        'public/environment/tests/async_waiter_unittest.cc',
      ],
    },
    {
      'target_name': 'mojo_public_system_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_bindings',
        'mojo_public_test_utils',
        'mojo_run_all_unittests',
        'mojo_system',
      ],
      'sources': [
        'public/c/tests/system/core_unittest.cc',
        'public/c/tests/system/core_unittest_pure_c.c',
        'public/c/tests/system/macros_unittest.cc',
        'public/cpp/tests/system/core_unittest.cc',
        'public/cpp/tests/system/macros_unittest.cc',
      ],
    },
    {
      'target_name': 'mojo_public_utility_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_bindings',
        'mojo_public_test_utils',
        'mojo_run_all_unittests',
        'mojo_system',
        'mojo_utility',
      ],
      'sources': [
        'public/utility/tests/mutex_unittest.cc',
        'public/utility/tests/run_loop_unittest.cc',
        'public/utility/tests/thread_unittest.cc',
      ],
      'conditions': [
        # See crbug.com/342893:
        ['OS=="win"', {
          'sources!': [
            'public/utility/tests/mutex_unittest.cc',
            'public/utility/tests/thread_unittest.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'mojo_public_system_perftests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_public_test_utils',
        'mojo_run_all_perftests',
        'mojo_system',
        'mojo_utility',
      ],
      'sources': [
        'public/c/tests/system/core_perftest.cc',
      ],
    },
    {
      'target_name': 'mojo_bindings',
      'type': 'static_library',
      'include_dirs': [
        '..'
      ],
      'sources': [
        'public/bindings/allocation_scope.h',
        'public/bindings/array.h',
        'public/bindings/buffer.h',
        'public/bindings/callback.h',
        'public/bindings/error_handler.h',
        'public/bindings/interface.h',
        'public/bindings/js/constants.cc',
        'public/bindings/js/constants.h',
        'public/bindings/message.h',
        'public/bindings/passable.h',
        'public/bindings/remote_ptr.h',
        'public/bindings/sync_dispatcher.h',
        'public/bindings/type_converter.h',
        'public/bindings/lib/array.cc',
        'public/bindings/lib/array_internal.h',
        'public/bindings/lib/array_internal.cc',
        'public/bindings/lib/bindings_internal.h',
        'public/bindings/lib/bindings_serialization.cc',
        'public/bindings/lib/bindings_serialization.h',
        'public/bindings/lib/buffer.cc',
        'public/bindings/lib/callback_internal.h',
        'public/bindings/lib/connector.cc',
        'public/bindings/lib/connector.h',
        'public/bindings/lib/fixed_buffer.cc',
        'public/bindings/lib/fixed_buffer.h',
        'public/bindings/lib/interface.cc',
        'public/bindings/lib/message.cc',
        'public/bindings/lib/message_builder.cc',
        'public/bindings/lib/message_builder.h',
        'public/bindings/lib/message_internal.h',
        'public/bindings/lib/message_queue.cc',
        'public/bindings/lib/message_queue.h',
        'public/bindings/lib/router.cc',
        'public/bindings/lib/router.h',
        'public/bindings/lib/scratch_buffer.cc',
        'public/bindings/lib/scratch_buffer.h',
        'public/bindings/lib/shared_data.h',
        'public/bindings/lib/shared_ptr.h',
        'public/bindings/lib/sync_dispatcher.cc',
      ],
    },
    {
      'target_name': 'mojo_sample_service',
      'type': 'static_library',
      'sources': [
        'public/bindings/tests/sample_service.mojom',
        'public/bindings/tests/sample_import.mojom',
        'public/bindings/tests/sample_import2.mojom',
      ],
      'variables': {
        'mojom_base_output_dir': 'mojo',
      },
      'includes': [ 'public/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_bindings',
        'mojo_system',
      ],
      'dependencies': [
        'mojo_bindings',
        'mojo_system',
      ],
    },
    {
      'target_name': 'mojo_environment_standalone',
      'type': 'static_library',
      'sources': [
        'public/environment/buffer_tls.h',
        'public/environment/default_async_waiter.h',
        'public/environment/environment.h',
        'public/environment/lib/default_async_waiter.cc',
        'public/environment/lib/buffer_tls.cc',
        'public/environment/lib/buffer_tls_setup.h',
        'public/environment/lib/environment.cc',
      ],
      'include_dirs': [
        '..',
      ],
    },
    {
      'target_name': 'mojo_utility',
      'type': 'static_library',
      'sources': [
        'public/utility/mutex.h',
        'public/utility/run_loop.h',
        'public/utility/run_loop_handler.h',
        'public/utility/thread.h',
        'public/utility/lib/mutex.cc',
        'public/utility/lib/run_loop.cc',
        'public/utility/lib/thread.cc',
        'public/utility/lib/thread_local.h',
        'public/utility/lib/thread_local_posix.cc',
        'public/utility/lib/thread_local_win.cc',
      ],
      'conditions': [
        # See crbug.com/342893:
        ['OS=="win"', {
          'sources!': [
            'public/utility/mutex.h',
            'public/utility/thread.h',
            'public/utility/lib/mutex.cc',
            'public/utility/lib/thread.cc',
          ],
        }],
      ],
      'include_dirs': [
        '..',
      ],
    },
    {
      'target_name': 'mojo_shell_bindings',
      'type': 'static_library',
      'sources': [
        'public/shell/shell.mojom',
      ],
      'variables': {
        'mojom_base_output_dir': 'mojo',
      },
      'includes': [ 'public/bindings/mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_bindings',
        'mojo_system',
      ],
      'export_dependent_settings': [
        'mojo_bindings',
      ],
    },
    {
      'target_name': 'mojo_shell_client',
      'type': 'static_library',
      'sources': [
        'public/shell/lib/application.cc',
        'public/shell/lib/service.cc',
        'public/shell/application.h',
        'public/shell/service.h',
      ],
      'dependencies': [
        'mojo_shell_bindings',
      ],
      'export_dependent_settings': [
        'mojo_shell_bindings',
      ],
    },
  ],
}
