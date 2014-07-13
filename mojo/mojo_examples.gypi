# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'mojo_sample_app',
      'type': 'loadable_module',
      'dependencies': [
        # TODO(darin): we should not be linking against these libraries!
        '../ui/events/events.gyp:events',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        'mojo_application',
        'mojo_cpp_bindings',
        'mojo_environment_standalone',
        'mojo_geometry_bindings',
        'mojo_gles2',
        'mojo_native_viewport_bindings',
        'mojo_utility',
        '<(mojo_system_for_loadable_module)',
      ],
      'sources': [
        'examples/sample_app/gles2_client_impl.cc',
        'examples/sample_app/gles2_client_impl.cc',
        'examples/sample_app/sample_app.cc',
        'examples/sample_app/spinning_cube.cc',
        'examples/sample_app/spinning_cube.h',
        'public/cpp/application/lib/mojo_main_standalone.cc',
      ],
    },
    {
      'target_name': 'package_mojo_sample_app',
      'variables': {
        'app_name': 'mojo_sample_app',
      },
      'includes': [ 'build/package_app.gypi' ],
    },
    {
      'target_name': 'mojo_compositor_app',
      'type': 'loadable_module',
      'dependencies': [
        '../base/base.gyp:base',
        '../cc/cc.gyp:cc',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        'mojo_application',
        'mojo_cc_support',
        'mojo_common_lib',
        'mojo_environment_chromium',
        'mojo_geometry_bindings',
        'mojo_geometry_lib',
        'mojo_gles2',
        'mojo_native_viewport_bindings',
        '<(mojo_system_for_loadable_module)',
      ],
      'sources': [
        'examples/compositor_app/compositor_app.cc',
        'examples/compositor_app/compositor_host.cc',
        'examples/compositor_app/compositor_host.h',
        'public/cpp/application/lib/mojo_main_chromium.cc',
      ],
    },
    {
      'target_name': 'package_mojo_compositor_app',
      'variables': {
        'app_name': 'mojo_compositor_app',
      },
      'includes': [ 'build/package_app.gypi' ],
    },
    {
      'target_name': 'mojo_wget',
      'type': 'loadable_module',
      'dependencies': [
        'mojo_application',
        'mojo_cpp_bindings',
        'mojo_environment_standalone',
        'mojo_network_bindings',
        'mojo_utility',
        '<(mojo_system_for_loadable_module)',
      ],
      'sources': [
        'examples/wget/wget.cc',
        'public/cpp/application/lib/mojo_main_standalone.cc',
      ],
    },
    {
      'target_name': 'package_mojo_wget',
      'variables': {
        'app_name': 'mojo_wget',
      },
      'includes': [ 'build/package_app.gypi' ],
    },
    {
      'target_name': 'mojo_html_viewer',
      'type': 'loadable_module',
      'dependencies': [
        '../net/net.gyp:net',
        '../skia/skia.gyp:skia',
        '../third_party/WebKit/public/blink.gyp:blink',
        '../ui/native_theme/native_theme.gyp:native_theme',
        '../url/url.gyp:url_lib',
        'mojo_application',
        'mojo_common_lib',
        'mojo_cpp_bindings',
        'mojo_environment_chromium',
        'mojo_navigation_bindings',
        'mojo_network_bindings',
        'mojo_launcher_bindings',
        'mojo_utility',
        'mojo_view_manager_lib',
        '<(mojo_system_for_loadable_module)',
      ],
      'include_dirs': [
        'third_party/WebKit'
      ],
      'sources': [
        'examples/html_viewer/blink_input_events_type_converters.cc',
        'examples/html_viewer/blink_input_events_type_converters.h',
        'examples/html_viewer/blink_platform_impl.cc',
        'examples/html_viewer/blink_platform_impl.h',
        'examples/html_viewer/html_viewer.cc',
        'examples/html_viewer/html_document_view.cc',
        'examples/html_viewer/html_document_view.h',
        'examples/html_viewer/webmimeregistry_impl.cc',
        'examples/html_viewer/webmimeregistry_impl.h',
        'examples/html_viewer/webstoragenamespace_impl.cc',
        'examples/html_viewer/webstoragenamespace_impl.h',
        'examples/html_viewer/webthemeengine_impl.cc',
        'examples/html_viewer/webthemeengine_impl.h',
        'examples/html_viewer/webthread_impl.cc',
        'examples/html_viewer/webthread_impl.h',
        'examples/html_viewer/weburlloader_impl.cc',
        'examples/html_viewer/weburlloader_impl.h',
        'public/cpp/application/lib/mojo_main_chromium.cc',
      ],
    },
    {
      'target_name': 'mojo_media_viewer_bindings',
      'type': 'static_library',
      'sources': [
        'examples/media_viewer/media_viewer.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_cpp_bindings',
      ],
    },
    {
      'target_name': 'mojo_png_viewer',
      'type': 'loadable_module',
      'dependencies': [
        '../skia/skia.gyp:skia',
        '../ui/gfx/gfx.gyp:gfx',
        'mojo_application',
        'mojo_cpp_bindings',
        'mojo_environment_chromium',
        'mojo_media_viewer_bindings',
        'mojo_navigation_bindings',
        'mojo_network_bindings',
        'mojo_launcher_bindings',
        'mojo_utility',
        'mojo_view_manager_lib',
        '<(mojo_system_for_loadable_module)',
      ],
      'sources': [
        'examples/png_viewer/png_viewer.cc',
        'public/cpp/application/lib/mojo_main_chromium.cc',
      ],
    },
    {
      'target_name': 'mojo_pepper_container_app',
      'type': 'loadable_module',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../gpu/gpu.gyp:command_buffer_common',
        '../ppapi/ppapi.gyp:ppapi_c',
        '../ppapi/ppapi_internal.gyp:ppapi_example_gles2_spinning_cube',
        '../ui/events/events.gyp:events_base',
        'mojo_application',
        'mojo_common_lib',
        'mojo_environment_chromium',
        'mojo_geometry_bindings',
        'mojo_gles2',
        'mojo_native_viewport_bindings',
        '<(mojo_system_for_loadable_module)',
      ],
      'defines': [
        # We don't really want to export. We could change how
        # ppapi_{shared,thunk}_export.h are defined to avoid this.
        'PPAPI_SHARED_IMPLEMENTATION',
        'PPAPI_THUNK_IMPLEMENTATION',
      ],
      'sources': [
        # Source files from ppapi/.
        # An alternative is to depend on
        # '../ppapi/ppapi_internal.gyp:ppapi_shared', but that target includes
        # a lot of things that we don't need.
        # TODO(yzshen): Consider extracting these files into a separate target
        # which mojo_pepper_container_app and ppapi_shared both depend on.
        '../ppapi/shared_impl/api_id.h',
        '../ppapi/shared_impl/callback_tracker.cc',
        '../ppapi/shared_impl/callback_tracker.h',
        '../ppapi/shared_impl/host_resource.cc',
        '../ppapi/shared_impl/host_resource.h',
        '../ppapi/shared_impl/id_assignment.cc',
        '../ppapi/shared_impl/id_assignment.h',
        '../ppapi/shared_impl/ppapi_globals.cc',
        '../ppapi/shared_impl/ppapi_globals.h',
        '../ppapi/shared_impl/ppapi_shared_export.h',
        '../ppapi/shared_impl/ppb_message_loop_shared.cc',
        '../ppapi/shared_impl/ppb_message_loop_shared.h',
        '../ppapi/shared_impl/ppb_view_shared.cc',
        '../ppapi/shared_impl/ppb_view_shared.h',
        '../ppapi/shared_impl/proxy_lock.cc',
        '../ppapi/shared_impl/proxy_lock.h',
        '../ppapi/shared_impl/resource.cc',
        '../ppapi/shared_impl/resource.h',
        '../ppapi/shared_impl/resource_tracker.cc',
        '../ppapi/shared_impl/resource_tracker.h',
        '../ppapi/shared_impl/scoped_pp_resource.cc',
        '../ppapi/shared_impl/scoped_pp_resource.h',
        '../ppapi/shared_impl/singleton_resource_id.h',
        '../ppapi/shared_impl/tracked_callback.cc',
        '../ppapi/shared_impl/tracked_callback.h',
        '../ppapi/thunk/enter.cc',
        '../ppapi/thunk/enter.h',
        '../ppapi/thunk/interfaces_ppb_private.h',
        '../ppapi/thunk/interfaces_ppb_private_flash.h',
        '../ppapi/thunk/interfaces_ppb_private_no_permissions.h',
        '../ppapi/thunk/interfaces_ppb_public_dev.h',
        '../ppapi/thunk/interfaces_ppb_public_dev_channel.h',
        '../ppapi/thunk/interfaces_ppb_public_stable.h',
        '../ppapi/thunk/interfaces_preamble.h',
        '../ppapi/thunk/ppapi_thunk_export.h',
        '../ppapi/thunk/ppb_graphics_3d_api.h',
        '../ppapi/thunk/ppb_graphics_3d_thunk.cc',
        '../ppapi/thunk/ppb_instance_api.h',
        '../ppapi/thunk/ppb_instance_thunk.cc',
        '../ppapi/thunk/ppb_message_loop_api.h',
        '../ppapi/thunk/ppb_view_api.h',
        '../ppapi/thunk/ppb_view_thunk.cc',
        '../ppapi/thunk/resource_creation_api.h',
        '../ppapi/thunk/thunk.h',

        'examples/pepper_container_app/graphics_3d_resource.cc',
        'examples/pepper_container_app/graphics_3d_resource.h',
        'examples/pepper_container_app/interface_list.cc',
        'examples/pepper_container_app/interface_list.h',
        'examples/pepper_container_app/mojo_ppapi_globals.cc',
        'examples/pepper_container_app/mojo_ppapi_globals.h',
        'examples/pepper_container_app/pepper_container_app.cc',
        'examples/pepper_container_app/plugin_instance.cc',
        'examples/pepper_container_app/plugin_instance.h',
        'examples/pepper_container_app/plugin_module.cc',
        'examples/pepper_container_app/plugin_module.h',
        'examples/pepper_container_app/ppb_core_thunk.cc',
        'examples/pepper_container_app/ppb_opengles2_thunk.cc',
        'examples/pepper_container_app/resource_creation_impl.cc',
        'examples/pepper_container_app/resource_creation_impl.h',
        'examples/pepper_container_app/thunk.h',
        'examples/pepper_container_app/type_converters.h',
        'public/cpp/application/lib/mojo_main_chromium.cc',
      ],
    },
    {
      'target_name': 'mojo_surfaces_app',
      'type': 'shared_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../cc/cc.gyp:cc',
        '../cc/cc.gyp:cc_surfaces',
        '../skia/skia.gyp:skia',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        'mojo_application',
        'mojo_common_lib',
        'mojo_environment_chromium',
        'mojo_geometry_bindings',
        'mojo_geometry_lib',
        'mojo_gles2',
        'mojo_native_viewport_bindings',
        'mojo_surfaces_bindings',
        'mojo_surfaces_app_bindings',
        'mojo_surfaces_lib',
        'mojo_system_impl',
      ],
      'sources': [
        'examples/surfaces_app/embedder.cc',
        'examples/surfaces_app/embedder.h',
        'examples/surfaces_app/surfaces_app.cc',
        'examples/surfaces_app/surfaces_util.cc',
        'examples/surfaces_app/surfaces_util.h',
        'public/cpp/application/lib/mojo_main_chromium.cc',
      ],
    },
    {
      'target_name': 'mojo_surfaces_app_bindings',
      'type': 'static_library',
      'sources': [
        'examples/surfaces_app/child.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_cpp_bindings',
      ],
    },
    {
      'target_name': 'package_mojo_surfaces_app',
      'variables': {
        'app_name': 'mojo_surfaces_app',
      },
      'includes': [ 'build/package_app.gypi' ],
    },
    {
      'target_name': 'mojo_surfaces_child_app',
      'type': 'shared_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../cc/cc.gyp:cc',
        '../cc/cc.gyp:cc_surfaces',
        '../skia/skia.gyp:skia',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        'mojo_application',
        'mojo_common_lib',
        'mojo_environment_chromium',
        'mojo_geometry_bindings',
        'mojo_geometry_lib',
        'mojo_surfaces_app_bindings',
        'mojo_surfaces_bindings',
        'mojo_surfaces_lib',
        'mojo_system_impl',
      ],
      'sources': [
        'examples/surfaces_app/child_app.cc',
        'examples/surfaces_app/child_impl.cc',
        'examples/surfaces_app/child_impl.h',
        'examples/surfaces_app/surfaces_util.cc',
        'examples/surfaces_app/surfaces_util.h',
        'public/cpp/application/lib/mojo_main_chromium.cc',
      ],
    },
  ],
  'conditions': [
    ['use_aura==1', {
      'targets': [
        {
          'target_name': 'mojo_aura_demo',
          'type': 'loadable_module',
          'dependencies': [
            '../base/base.gyp:base',
            '../cc/cc.gyp:cc',
            '../ui/aura/aura.gyp:aura',
            '../ui/base/ui_base.gyp:ui_base',
            '../ui/compositor/compositor.gyp:compositor',
            '../ui/gfx/gfx.gyp:gfx',
            '../ui/gfx/gfx.gyp:gfx_geometry',
            'mojo_application',
            'mojo_aura_support',
            'mojo_common_lib',
            'mojo_environment_chromium',
            'mojo_geometry_bindings',
            'mojo_geometry_lib',
            'mojo_view_manager_lib',
            '<(mojo_system_for_loadable_module)',
          ],
          'sources': [
            'examples/aura_demo/aura_demo.cc',
            'public/cpp/application/lib/mojo_main_chromium.cc',
          ],
        },
        {
          'target_name': 'mojo_aura_demo_init',
          'type': 'loadable_module',
          'dependencies': [
            '../base/base.gyp:base',
            'mojo_application',
            'mojo_environment_chromium',
            'mojo_view_manager_bindings',
            '<(mojo_system_for_loadable_module)',
          ],
          'sources': [
            'examples/aura_demo/view_manager_init.cc',
            'public/cpp/application/lib/mojo_main_chromium.cc',
          ],
        },
        {
          'target_name': 'mojo_browser',
          'type': 'loadable_module',
          'dependencies': [
            '../base/base.gyp:base',
            '../cc/cc.gyp:cc',
            '../third_party/icu/icu.gyp:icui18n',
            '../third_party/icu/icu.gyp:icuuc',
            '../ui/aura/aura.gyp:aura',
            '../ui/base/ui_base.gyp:ui_base',
            '../ui/compositor/compositor.gyp:compositor',
            '../ui/gfx/gfx.gyp:gfx',
            '../ui/gfx/gfx.gyp:gfx_geometry',
            '../ui/resources/ui_resources.gyp:ui_resources',
            '../ui/resources/ui_resources.gyp:ui_test_pak',
            '../ui/views/views.gyp:views',
            '../url/url.gyp:url_lib',
            'mojo_application',
            'mojo_aura_support',
            'mojo_common_lib',
            'mojo_environment_chromium',
            'mojo_geometry_bindings',
            'mojo_geometry_lib',
            'mojo_input_events_lib',
            'mojo_navigation_bindings',
            'mojo_views_support',
            'mojo_view_manager_bindings',
            'mojo_view_manager_lib',
            'mojo_window_manager_bindings',
            '<(mojo_system_for_loadable_module)',
          ],
          'sources': [
            'examples/browser/browser.cc',
            'public/cpp/application/lib/mojo_main_chromium.cc',
          ],
        },
        {
          'target_name': 'package_mojo_aura_demo',
          'variables': {
            'app_name': 'mojo_aura_demo',
          },
          'includes': [ 'build/package_app.gypi' ],
        },
        {
          'target_name': 'mojo_demo_launcher',
          'type': 'loadable_module',
          'dependencies': [
            '../base/base.gyp:base',
            '../skia/skia.gyp:skia',
            '../ui/gfx/gfx.gyp:gfx',
            '../ui/gfx/gfx.gyp:gfx_geometry',
            '../ui/gl/gl.gyp:gl',
            'mojo_application',
            'mojo_cpp_bindings',
            'mojo_environment_chromium',
            'mojo_geometry_bindings',
            'mojo_gles2',
            'mojo_view_manager_bindings',
            'mojo_utility',
            '<(mojo_system_for_loadable_module)',
          ],
          'sources': [
            'examples/demo_launcher/demo_launcher.cc',
            'public/cpp/application/lib/mojo_main_chromium.cc',
          ],
        },
        {
          'target_name': 'mojo_keyboard',
          'type': 'loadable_module',
          'dependencies': [
            '../base/base.gyp:base',
            '../cc/cc.gyp:cc',
            '../third_party/icu/icu.gyp:icui18n',
            '../third_party/icu/icu.gyp:icuuc',
            '../ui/aura/aura.gyp:aura',
            '../ui/base/ui_base.gyp:ui_base',
            '../ui/compositor/compositor.gyp:compositor',
            '../ui/gfx/gfx.gyp:gfx',
            '../ui/gfx/gfx.gyp:gfx_geometry',
            '../ui/resources/ui_resources.gyp:ui_resources',
            '../ui/resources/ui_resources.gyp:ui_test_pak',
            '../ui/views/views.gyp:views',
            '../url/url.gyp:url_lib',
            'mojo_application',
            'mojo_aura_support',
            'mojo_common_lib',
            'mojo_environment_chromium',
            'mojo_geometry_bindings',
            'mojo_geometry_lib',
            'mojo_input_events_lib',
            'mojo_keyboard_bindings',
            'mojo_navigation_bindings',
            'mojo_views_support',
            'mojo_view_manager_bindings',
            'mojo_view_manager_lib',
            '<(mojo_system_for_loadable_module)',
          ],
          'sources': [
            'examples/keyboard/keyboard_delegate.h',
            'examples/keyboard/keyboard_view.cc',
            'examples/keyboard/keyboard_view.h',
            'examples/keyboard/keyboard.cc',
            'examples/keyboard/keys.cc',
            'examples/keyboard/keys.h',
            'public/cpp/application/lib/mojo_main_chromium.cc',
          ],
        },
        {
          'target_name': 'mojo_keyboard_bindings',
          'type': 'static_library',
          'sources': [
            'examples/keyboard/keyboard.mojom',
          ],
          'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
          'export_dependent_settings': [
            'mojo_cpp_bindings',
          ],
          'dependencies': [
            'mojo_cpp_bindings',
          ],
        },
        {
          'target_name': 'mojo_window_manager_bindings',
          'type': 'static_library',
          'sources': [
            'examples/window_manager/window_manager.mojom',
          ],
          'dependencies': [
            'mojo_cpp_bindings',
            'mojo_geometry_bindings',
          ],
          'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
          'export_dependent_settings': [
            'mojo_cpp_bindings',
          ],
        },
        {
          'target_name': 'mojo_window_manager',
          'type': 'loadable_module',
          'dependencies': [
            '../base/base.gyp:base',
            '../ui/aura/aura.gyp:aura',
            '../ui/base/ui_base.gyp:ui_base',
            '../ui/gfx/gfx.gyp:gfx',
            '../ui/gfx/gfx.gyp:gfx_geometry',
            '../ui/gl/gl.gyp:gl',
            '../ui/resources/ui_resources.gyp:ui_resources',
            '../ui/resources/ui_resources.gyp:ui_test_pak',
            '../ui/views/views.gyp:views',
            'mojo_application',
            'mojo_aura_support',
            'mojo_cpp_bindings',
            'mojo_environment_chromium',
            'mojo_geometry_bindings',
            'mojo_geometry_lib',
            'mojo_gles2',
            'mojo_input_events_lib',
            'mojo_keyboard_bindings',
            'mojo_launcher_bindings',
            'mojo_navigation_bindings',
            'mojo_view_manager_lib',
            'mojo_views_support',
            'mojo_window_manager_bindings',
            'mojo_utility',
            '<(mojo_system_for_loadable_module)',
          ],
          'sources': [
            'examples/window_manager/debug_panel.h',
            'examples/window_manager/debug_panel.cc',
            'examples/window_manager/window_manager.cc',
            'public/cpp/application/lib/mojo_main_chromium.cc',
          ],
        },
        {
          'target_name': 'mojo_embedded_app',
          'type': 'loadable_module',
          'dependencies': [
            '../base/base.gyp:base',
            '../ui/gfx/gfx.gyp:gfx_geometry',
            '../ui/gl/gl.gyp:gl',
            '../url/url.gyp:url_lib',
            'mojo_application',
            'mojo_cpp_bindings',
            'mojo_environment_chromium',
            'mojo_geometry_bindings',
            'mojo_gles2',
            'mojo_navigation_bindings',
            'mojo_view_manager_lib',
            'mojo_window_manager_bindings',
            'mojo_utility',
            '<(mojo_system_for_loadable_module)',
          ],
          'sources': [
            'examples/embedded_app/embedded_app.cc',
            'public/cpp/application/lib/mojo_main_chromium.cc',
          ],
        },
        {
          'target_name': 'mojo_nesting_app',
          'type': 'loadable_module',
          'dependencies': [
            '../base/base.gyp:base',
            '../ui/gfx/gfx.gyp:gfx_geometry',
            '../ui/gl/gl.gyp:gl',
            '../url/url.gyp:url_lib',
            'mojo_application',
            'mojo_cpp_bindings',
            'mojo_environment_chromium',
            'mojo_geometry_bindings',
            'mojo_gles2',
            'mojo_navigation_bindings',
            'mojo_view_manager_lib',
            'mojo_window_manager_bindings',
            'mojo_utility',
            '<(mojo_system_for_loadable_module)',
          ],
          'sources': [
            'examples/nesting_app/nesting_app.cc',
            'public/cpp/application/lib/mojo_main_chromium.cc',
          ],
        },
        {
          'target_name': 'mojo_media_viewer',
          'type': 'loadable_module',
          'dependencies': [
            '../base/base.gyp:base',
            '../skia/skia.gyp:skia',
            '../ui/gfx/gfx.gyp:gfx_geometry',
            '../ui/views/views.gyp:views',
            'mojo_application',
            'mojo_environment_chromium',
            'mojo_input_events_lib',
            'mojo_media_viewer_bindings',
            'mojo_navigation_bindings',
            'mojo_views_support',
            'mojo_view_manager_bindings',
            'mojo_view_manager_lib',
            '<(mojo_system_for_loadable_module)',
          ],
          'sources': [
            'examples/media_viewer/media_viewer.cc',
            'public/cpp/application/lib/mojo_main_chromium.cc',
          ],
        },
      ],
    }],
    ['OS=="linux"', {
      'targets': [
        {
          'target_name': 'mojo_dbus_echo',
          'type': 'loadable_module',
          'dependencies': [
            '../base/base.gyp:base',
            'mojo_application',
            'mojo_cpp_bindings',
            'mojo_environment_standalone',
            'mojo_echo_bindings',
            'mojo_utility',
            '<(mojo_system_for_loadable_module)',
          ],
          'sources': [
            'examples/dbus_echo/dbus_echo_app.cc',
            'public/cpp/application/lib/mojo_main_standalone.cc',
          ],
        },
      ],
    }],
  ],
}
