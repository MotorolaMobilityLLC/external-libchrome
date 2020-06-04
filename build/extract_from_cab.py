#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Extracts a single file from a CAB archive."""

import os
import subprocess
import sys


def main():
  if len(sys.argv) != 4:
    print 'Usage: extract_from_cab.py cab_path archived_file output_dir'
    return 1

  [cab_path, archived_file, output_dir] = sys.argv[1:]

  # Invoke the Windows expand utility to extract the file.
  level = subprocess.call(
      ['expand', cab_path, '-F:' + archived_file, output_dir])
  if level != 0:
    print 'Cab extraction(%s, %s, %s) failed.' % (
        cab_path, archived_file, output_dir)
    print 'Trying a second time.'
    level = subprocess.call(
        ['expand', cab_path, '-F:' + archived_file, output_dir])
    if level != 0:
      return level

  # The expand utility preserves the modification date and time of the archived
  # file. Touch the extracted file. This helps build systems that compare the
  # modification times of input and output files to determine whether to do an
  # action.
  os.utime(os.path.join(output_dir, archived_file), None)
  return 0


if __name__ == '__main__':
  sys.exit(main())
