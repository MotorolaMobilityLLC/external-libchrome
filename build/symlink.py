#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Make a symlink and optionally touch a file (to handle dependencies).

Usage:
  symlink.py [options] sources... target

A sym link to source is created at target. If multiple sources are specfied,
then target is assumed to be a directory, and will contain all the links to
the sources (basenames identical to their source).
"""

import errno
import optparse
import os.path
import shutil
import sys


def Main(argv):
  parser = optparse.OptionParser()
  parser.add_option('-f', '--force', action='store_true')
  parser.add_option('--touch')
  parser.add_option('--update-target-mtimes', action='store_true')

  options, args = parser.parse_args(argv[1:])
  if len(args) < 2:
    parser.error('at least two arguments required.')

  target = args[-1]
  sources = args[:-1]
  for s in sources:
    t = os.path.join(target, os.path.basename(s))
    if len(sources) == 1 and not os.path.isdir(target):
      t = target
    t = os.path.expanduser(t)
    if os.path.realpath(t) == s:
      continue
    try:
      os.symlink(s, t)
    except OSError, e:
      if e.errno == errno.EEXIST and options.force:
        if os.path.isdir(t):
          shutil.rmtree(t, ignore_errors=True)
        else:
          os.remove(t)
        os.symlink(s, t)
      else:
        raise
    if options.update_target_mtimes:
      # Work-around for ninja bug:
      # https://github.com/ninja-build/ninja/issues/1186
      os.utime(s, None)


  if options.touch:
    with open(options.touch, 'w') as f:
      pass


if __name__ == '__main__':
  sys.exit(Main(sys.argv))
