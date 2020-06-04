# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'mojom_bindings_generator_variables.gypi',
  ],
  'variables': {
    'mojom_base_output_dir':
        '<!(python <(DEPTH)/build/inverse_depth.py <(DEPTH))',
    'mojom_generated_outputs': [
      '<!@(python <(DEPTH)/mojo/public/tools/bindings/mojom_list_outputs.py --basedir <(mojom_base_output_dir) <@(mojom_files))',
    ],
  },
  'actions': [
    {
      'action_name': '<(_target_name)_mojom_bindings_generator',
      'variables': {
        'java_out_dir': '<(PRODUCT_DIR)/java_mojo/<(_target_name)/src',
        'mojom_import_args%': [
         '-I<(DEPTH)'
        ],
      },
      'inputs': [
        '<@(mojom_bindings_generator_sources)',
        '<@(mojom_files)',
      ],
      'outputs': [
        '<@(mojom_generated_outputs)',
      ],
      'action': [
        'python', '<@(mojom_bindings_generator)',
        '<@(mojom_files)',
        '--use_chromium_bundled_pylibs',
        '-d', '<(DEPTH)',
        '<@(mojom_import_args)',
        '-o', '<(SHARED_INTERMEDIATE_DIR)',
        '--java_output_directory=<(java_out_dir)',
      ],
      'message': 'Generating Mojo bindings from <@(mojom_files)',
      'process_outputs_as_sources': 1,
    }
  ],
  'direct_dependent_settings': {
    'sources': [
      '<@(mojom_generated_outputs)',
    ],
    # Include paths needed to compile the generated sources into a library.
    'include_dirs': [
      '<(DEPTH)',
      '<(SHARED_INTERMEDIATE_DIR)',
    ],
    'direct_dependent_settings': {
      # Include paths needed to find the generated header files and their
      # transitive dependancies when using the library.
      'include_dirs': [
        '<(DEPTH)',
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'variables': {
	'generated_src_dirs': [
	  '<(PRODUCT_DIR)/java_mojo/<(_target_name)/src',
	],
      },
    }
  },
  'hard_dependency': 1,
}
