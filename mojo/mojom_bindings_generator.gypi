# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'mojom_bindings_generator_variables.gypi',
  ],
  'variables': {
    'variables': {
      'mojom_typemap_dependencies%': [],
      'mojom_typemaps%': [],
      'mojom_variant%': 'none',
      'for_blink%': 'false',
    },
    'mojom_typemap_dependencies%': ['<@(mojom_typemap_dependencies)'],
    'mojom_typemaps%': ['<@(mojom_typemaps)'],
    'mojom_variant%': '<(mojom_variant)',
    'for_blink%': '<(for_blink)',
    'mojom_base_output_dir':
        '<!(python <(DEPTH)/build/inverse_depth.py <(DEPTH))',
    'mojom_generated_outputs': [
      '<!@(python <(DEPTH)/mojo/public/tools/bindings/mojom_list_outputs.py --basedir <(mojom_base_output_dir) --variant <(mojom_variant) <@(_sources))',
    ],
    'mojom_generator_typemap_args': [
      '<!@(python <(DEPTH)/mojo/public/tools/bindings/mojom_get_generator_typemap_args.py <@(mojom_typemaps))',
    ],
    'mojom_extra_generator_args%': [],
    'conditions': [
      ['mojom_variant=="none"', {
        'mojom_output_languages%': 'c++,javascript,java',
      }, {
        'mojom_output_languages%': 'c++',
      }],
      ['for_blink=="true"', {
        'mojom_generator_wtf_arg%': [
          '--for_blink',
        ],
        'wtf_dependencies%': [
          '<(DEPTH)/mojo/mojo_public.gyp:mojo_cpp_bindings_wtf_support',
          '<(DEPTH)/third_party/WebKit/Source/wtf/wtf.gyp:wtf',
        ],
      }, {
        'mojom_generator_wtf_arg%': [],
        'wtf_dependencies%': [],
      }],
    ],
  },
  'actions': [
    {
      'variables': {
        'java_out_dir': '<(PRODUCT_DIR)/java_mojo/<(_target_name)/src',
        'stamp_filename': '<(PRODUCT_DIR)/java_mojo/<(_target_name)/<(_target_name).stamp',
      },
      'action_name': '<(_target_name)_mojom_bindings_stamp',
      # The java output directory is deleted to ensure that the java library
      # doesn't try to compile stale files.
      'action': [
        'python', '<(DEPTH)/build/rmdir_and_stamp.py',
        '<(java_out_dir)',
        '<(stamp_filename)',
      ],
      'inputs': [ '<@(_sources)' ],
      'outputs': [ '<(stamp_filename)' ],
    }
  ],
  'rules': [
    {
      'rule_name': '<(_target_name)_mojom_bindings_generator',
      'extension': 'mojom',
      'variables': {
        'java_out_dir': '<(PRODUCT_DIR)/java_mojo/<(_target_name)/src',
        'mojom_import_args%': [
         '-I<(DEPTH)',
         '-I<(DEPTH)/mojo/services',
        ],
        'stamp_filename': '<(PRODUCT_DIR)/java_mojo/<(_target_name)/<(_target_name).stamp',
      },
      'inputs': [
        '<@(mojom_bindings_generator_sources)',
        '<@(mojom_typemaps)',
        '<(stamp_filename)',
        '<(SHARED_INTERMEDIATE_DIR)/mojo/public/tools/bindings/cpp_templates.zip',
        '<(SHARED_INTERMEDIATE_DIR)/mojo/public/tools/bindings/java_templates.zip',
        '<(SHARED_INTERMEDIATE_DIR)/mojo/public/tools/bindings/js_templates.zip',
      ],
      'conditions': [
        ['mojom_variant=="none"', {
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/<(mojom_base_output_dir)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom.cc',
            '<(SHARED_INTERMEDIATE_DIR)/<(mojom_base_output_dir)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom.h',
            '<(SHARED_INTERMEDIATE_DIR)/<(mojom_base_output_dir)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom.js',
            '<(SHARED_INTERMEDIATE_DIR)/<(mojom_base_output_dir)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom-internal.h',
          ]
        }, {
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/<(mojom_base_output_dir)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom-<(mojom_variant).cc',
            '<(SHARED_INTERMEDIATE_DIR)/<(mojom_base_output_dir)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom-<(mojom_variant).h',
            '<(SHARED_INTERMEDIATE_DIR)/<(mojom_base_output_dir)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom-<(mojom_variant)-internal.h',
          ],
        }]
      ],
      'action': [
        'python', '<@(mojom_bindings_generator)',
        '--use_bundled_pylibs', 'generate',
        './<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom',
        '-d', '<(DEPTH)',
        '<@(mojom_import_args)',
        '-o', '<(SHARED_INTERMEDIATE_DIR)',
        '--java_output_directory=<(java_out_dir)',
        '--variant', '<(mojom_variant)',
        '-g', '<(mojom_output_languages)',
        '<@(mojom_generator_typemap_args)',
        '<@(mojom_extra_generator_args)',
        '<@(mojom_generator_wtf_arg)',
        '--bytecode_path',
        '<(SHARED_INTERMEDIATE_DIR)/mojo/public/tools/bindings',
      ],
      'message': 'Generating Mojo bindings from <(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom',
      'process_outputs_as_sources': 1,
    }
  ],
  'dependencies': [
    '<(DEPTH)/base/base.gyp:base',
    '<(DEPTH)/mojo/mojo_public.gyp:mojo_interface_bindings_generation',
    '<(DEPTH)/mojo/public/tools/bindings/bindings.gyp:precompile_mojom_bindings_generator_templates',
    '<@(mojom_typemap_dependencies)',
    '<@(wtf_dependencies)',
  ],
  'export_dependent_settings': [
    '<@(mojom_typemap_dependencies)',
    '<@(wtf_dependencies)',
  ],
  'include_dirs': [
    '<(DEPTH)',
    '<(SHARED_INTERMEDIATE_DIR)',
  ],
  'direct_dependent_settings': {
    'include_dirs': [
      '<(DEPTH)',
      '<(SHARED_INTERMEDIATE_DIR)',
    ],
    'variables': {
      'generated_src_dirs': [
        '<(PRODUCT_DIR)/java_mojo/<(_target_name)/src',
      ],
      'additional_input_paths': [
        '<@(mojom_bindings_generator_sources)',
        '<@(_sources)',
      ],
    },
  },
  'hard_dependency': 1,
}
