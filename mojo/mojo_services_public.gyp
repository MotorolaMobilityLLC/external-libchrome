# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../third_party/mojo/mojo_variables.gypi',
  ],
  'variables': {
    'mojo_sdk_root': '<(DEPTH)/third_party/mojo/src',
  },
  'targets': [
    {
      'target_name': 'mojo_services_public',
      'type': 'none',
      'dependencies': [
        'mojo_clipboard_bindings',
        'mojo_content_handler_bindings',
        'mojo_geometry_bindings',
        'mojo_gpu_bindings',
        'mojo_input_events_bindings',
        'mojo_native_viewport_bindings',
        'mojo_navigation_bindings',
        'mojo_network_bindings',
        'mojo_surface_id_bindings',
        'mojo_surfaces_bindings',
        'mojo_view_manager_bindings',
        'mojo_view_manager_common',
        'mojo_window_manager_bindings',
      ],
    },
    {
      # GN version: //mojo/services/clipboard/public/interfaces
      'target_name': 'mojo_clipboard_bindings',
      'type': 'static_library',
      'variables': {
        'mojom_import_args': [ '-I<(mojo_sdk_root)', '-Iservices' ],
      },
      'sources': [
        'services/clipboard/public/interfaces/clipboard.mojom',
      ],
      'includes': [ '../third_party/mojo/mojom_bindings_generator.gypi' ],
      'dependencies': [
        '../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        '../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/services/input_events/public/interfaces
      'target_name': 'mojo_input_events_bindings',
      'type': 'static_library',
      'variables': {
        'mojom_import_args': [ '-I<(mojo_sdk_root)', '-Iservices' ],
      },
      'sources': [
        'services/input_events/public/interfaces/input_event_constants.mojom',
        'services/input_events/public/interfaces/input_events.mojom',
        'services/input_events/public/interfaces/input_key_codes.mojom',
      ],
      'includes': [ '../third_party/mojo/mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_geometry_bindings',
        '../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        'mojo_geometry_bindings',
        '../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/services/geometry/public/interfaces
      'target_name': 'mojo_geometry_bindings',
      'type': 'static_library',
      'variables': {
        'mojom_import_args': [ '-I<(mojo_sdk_root)', '-Iservices' ],
      },
      'sources': [
        'services/geometry/public/interfaces/geometry.mojom',
      ],
      'includes': [ '../third_party/mojo/mojom_bindings_generator.gypi' ],
      'dependencies': [
        '../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        '../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/services/gpu/public/interfaces
      'target_name': 'mojo_gpu_bindings',
      'type': 'static_library',
      'variables': {
        'mojom_import_args': [ '-I<(mojo_sdk_root)', '-Iservices' ],
      },
      'sources': [
        'services/gpu/public/interfaces/command_buffer.mojom',
        'services/gpu/public/interfaces/gpu.mojom',
        'services/gpu/public/interfaces/gpu_capabilities.mojom',
        'services/gpu/public/interfaces/viewport_parameter_listener.mojom',
      ],
      'includes': [ '../third_party/mojo/mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_geometry_bindings',
        '../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        'mojo_geometry_bindings',
        '../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/services/native_viewport/public/interfaces
      'target_name': 'mojo_native_viewport_bindings',
      'type': 'static_library',
      'variables': {
        'mojom_import_args': [ '-I<(mojo_sdk_root)', '-Iservices' ],
      },
      'sources': [
        'services/native_viewport/public/interfaces/native_viewport.mojom',
      ],
      'includes': [ '../third_party/mojo/mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_geometry_bindings',
        'mojo_gpu_bindings',
        'mojo_input_events_bindings',
        '../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
        'mojo_surface_id_bindings',
      ],
      'export_dependent_settings': [
        'mojo_geometry_bindings',
        'mojo_gpu_bindings',
        'mojo_input_events_bindings',
        '../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
        'mojo_surface_id_bindings',
      ],
    },
    {
      # GN version: //mojo/services/navigation/public/interfaces
      'target_name': 'mojo_navigation_bindings',
      'type': 'static_library',
      'variables': {
        'mojom_import_args': [ '-I<(mojo_sdk_root)', '-Iservices' ],
      },
      'sources': [
        'services/navigation/public/interfaces/navigation.mojom',
      ],
      'includes': [ '../third_party/mojo/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        '../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_network_bindings',
        '../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/services/content_handler/public/interfaces
      'target_name': 'mojo_content_handler_bindings',
      'type': 'static_library',
      'variables': {
        'mojom_import_args': [ '-I<(mojo_sdk_root)', '-Iservices' ],
      },
      'sources': [
        'services/content_handler/public/interfaces/content_handler.mojom',
      ],
      'includes': [ '../third_party/mojo/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        '../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_network_bindings',
        '../third_party/mojo/mojo_public.gyp:mojo_application_bindings',
        '../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/services/network/public/interfaces
      'target_name': 'mojo_network_bindings',
      'type': 'static_library',
       'variables' : {
         'mojom_import_args': [ '-I<(mojo_sdk_root)', '-Iservices' ],
       },
       'sources': [
        'services/network/public/interfaces/cookie_store.mojom',
        'services/network/public/interfaces/net_address.mojom',
        'services/network/public/interfaces/network_error.mojom',
        'services/network/public/interfaces/network_service.mojom',
        'services/network/public/interfaces/tcp_bound_socket.mojom',
        'services/network/public/interfaces/tcp_connected_socket.mojom',
        'services/network/public/interfaces/tcp_server_socket.mojom',
        'services/network/public/interfaces/udp_socket.mojom',
        'services/network/public/interfaces/url_loader.mojom',
        'services/network/public/interfaces/web_socket.mojom',
      ],
      'includes': [ '../third_party/mojo/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        '../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'dependencies': [
        '../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/services/view_manager/public/cpp:common
      'target_name': 'mojo_view_manager_common',
      'type': 'static_library',
      'sources': [
        'services/view_manager/public/cpp/types.h',
      ],
    },
    {
      # GN version: //mojo/services/view_manager/public/interfaces/
      'target_name': 'mojo_view_manager_bindings',
      'type': 'static_library',
      'variables': {
        'mojom_import_args': [ '-I<(mojo_sdk_root)', '-Iservices' ],
      },
      'sources': [
        'services/view_manager/public/interfaces/view_manager.mojom',
        'services/view_manager/public/interfaces/view_manager_constants.mojom',
      ],
      'includes': [ '../third_party/mojo/mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_geometry_bindings',
        'mojo_input_events_bindings',
        'mojo_native_viewport_bindings',
        '../third_party/mojo/mojo_public.gyp:mojo_application_bindings',
        '../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
        'mojo_surface_id_bindings',
      ],
      'export_dependent_settings': [
        'mojo_geometry_bindings',
        'mojo_input_events_bindings',
        'mojo_native_viewport_bindings',
        '../third_party/mojo/mojo_public.gyp:mojo_application_bindings',
        '../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
        'mojo_surface_id_bindings',
      ],
    },
    {
      # GN version: //mojo/services/surfaces/public/interfaces
      'target_name': 'mojo_surfaces_bindings',
      'type': 'static_library',
      'variables': {
        'mojom_import_args': [ '-I<(mojo_sdk_root)', '-Iservices' ],
      },
      'sources': [
        'services/surfaces/public/interfaces/surfaces.mojom',
        'services/surfaces/public/interfaces/surfaces_service.mojom',
        'services/surfaces/public/interfaces/quads.mojom',
      ],
      'includes': [ '../third_party/mojo/mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_geometry_bindings',
        'mojo_gpu_bindings',
        '../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
        'mojo_surface_id_bindings',
      ],
      'export_dependent_settings': [
        'mojo_geometry_bindings',
        '../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
        'mojo_surface_id_bindings',
      ],
    },
    {
      # GN version: //mojo/services/surfaces/public/interfaces:surface_id
      'target_name': 'mojo_surface_id_bindings',
      'type': 'static_library',
      'variables': {
        'mojom_import_args': [ '-I<(mojo_sdk_root)', '-Iservices' ],
      },
      'sources': [
        'services/surfaces/public/interfaces/surface_id.mojom',
      ],
      'includes': [ '../third_party/mojo/mojom_bindings_generator.gypi' ],
      'dependencies': [
        '../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        '../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/services/window_manager/public/interfaces
      'target_name': 'mojo_window_manager_bindings',
      'type': 'static_library',
      'variables': {
        'mojom_import_args': [ '-I<(mojo_sdk_root)', '-Iservices' ],
      },
      'sources': [
        'services/window_manager/public/interfaces/window_manager.mojom',
      ],
      'includes': [ '../third_party/mojo/mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_input_events_bindings',
        '../third_party/mojo/mojo_public.gyp:mojo_application_bindings',
        '../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        'mojo_input_events_bindings',
        '../third_party/mojo/mojo_public.gyp:mojo_application_bindings',
        '../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
  ],
}
