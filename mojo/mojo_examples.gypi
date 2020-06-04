# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'mojo_sample_app',
      'type': 'shared_library',
      'dependencies': [
        # TODO(darin): we should not be linking against these libraries!
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../ui/gl/gl.gyp:gl',
        'mojo_environment_standalone',
        'mojo_gles2',
        'mojo_native_viewport_bindings',
        'mojo_shell_bindings',
        'mojo_system',
        'mojo_utility',
      ],
      'sources': [
        'examples/sample_app/gles2_client_impl.cc',
        'examples/sample_app/gles2_client_impl.cc',
        'examples/sample_app/sample_app.cc',
        'examples/sample_app/spinning_cube.cc',
        'examples/sample_app/spinning_cube.h',
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
      'type': 'shared_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../cc/cc.gyp:cc',
        '../gpu/gpu.gyp:gles2_implementation',
        '../skia/skia.gyp:skia',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../ui/gl/gl.gyp:gl',
        'mojo_common_lib',
        'mojo_environment_chromium',
        'mojo_gles2',
        'mojo_gles2_bindings',
        'mojo_native_viewport_bindings',
        'mojo_shell_bindings',
        'mojo_system',
      ],
      'sources': [
        'examples/compositor_app/compositor_app.cc',
        'examples/compositor_app/compositor_host.cc',
        'examples/compositor_app/compositor_host.h',
        'examples/compositor_app/gles2_client_impl.cc',
        'examples/compositor_app/gles2_client_impl.cc',
      ],
    },
    {
      'target_name': 'package_mojo_compositor_app',
      'variables': {
        'app_name': 'mojo_compositor_app',
      },
      'includes': [ 'build/package_app.gypi' ],
    },
  ],
  'conditions': [
    ['use_aura==1', {
      'targets': [
        {
          'target_name': 'mojo_aura_demo',
          'type': 'shared_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../cc/cc.gyp:cc',
            '../gpu/gpu.gyp:gles2_implementation',
            '../skia/skia.gyp:skia',
            '../ui/aura/aura.gyp:aura',
            '../ui/compositor/compositor.gyp:compositor',
            '../ui/events/events.gyp:events',
            '../ui/events/events.gyp:events_base',
            '../ui/gfx/gfx.gyp:gfx',
            '../ui/gfx/gfx.gyp:gfx_geometry',
            '../ui/gl/gl.gyp:gl',
            '../ui/ui.gyp:ui',
            '../webkit/common/gpu/webkit_gpu.gyp:webkit_gpu',
            'mojo_common_lib',
            'mojo_environment_chromium',
            'mojo_gles2',
            'mojo_gles2_bindings',
            'mojo_native_viewport_bindings',
            'mojo_shell_bindings',
            'mojo_system',
          ],
          'sources': [
            'examples/aura_demo/aura_demo.cc',
            'examples/aura_demo/demo_context_factory.cc',
            'examples/aura_demo/demo_context_factory.h',
            'examples/aura_demo/demo_screen.cc',
            'examples/aura_demo/demo_screen.h',
            'examples/aura_demo/root_window_host_mojo.cc',
            'examples/aura_demo/root_window_host_mojo.h',
            'examples/compositor_app/gles2_client_impl.cc',
            'examples/compositor_app/gles2_client_impl.cc',
          ],
        },
        {
          'target_name': 'package_mojo_aura_demo',
          'variables': {
            'app_name': 'mojo_aura_demo',
          },
          'includes': [ 'build/package_app.gypi' ],
        },
      ],
    }],
  ],
}
