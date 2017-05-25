#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Find header files missing in GN.

This script gets all the header files from ninja_deps, which is from the true
dependency generated by the compiler, and report if they don't exist in GN.
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from multiprocessing import Process, Queue


def GetHeadersFromNinja(out_dir, q):
  """Return all the header files from ninja_deps"""

  def NinjaSource():
    cmd = ['ninja', '-C', out_dir, '-t', 'deps']
    # A negative bufsize means to use the system default, which usually
    # means fully buffered.
    popen = subprocess.Popen(cmd, stdout=subprocess.PIPE, bufsize=-1)
    for line in iter(popen.stdout.readline, ''):
      yield line.rstrip()

    popen.stdout.close()
    return_code = popen.wait()
    if return_code:
      raise subprocess.CalledProcessError(return_code, cmd)

  ninja_out = NinjaSource()
  q.put(ParseNinjaDepsOutput(ninja_out))


def ParseNinjaDepsOutput(ninja_out):
  """Parse ninja output and get the header files"""
  all_headers = set()

  prefix = '..' + os.sep + '..' + os.sep

  is_valid = False
  for line in ninja_out:
    if line.startswith('    '):
      if not is_valid:
        continue
      if line.endswith('.h') or line.endswith('.hh'):
        f = line.strip()
        if f.startswith(prefix):
          f = f[6:]  # Remove the '../../' prefix
          # build/ only contains build-specific files like build_config.h
          # and buildflag.h, and system header files, so they should be
          # skipped.
          if not f.startswith('build'):
            all_headers.add(f)
    else:
      is_valid = line.endswith('(VALID)')

  return all_headers


def GetHeadersFromGN(out_dir, q):
  """Return all the header files from GN"""

  tmp = None
  try:
    tmp = tempfile.mkdtemp()
    shutil.copy2(os.path.join(out_dir, 'args.gn'),
                 os.path.join(tmp, 'args.gn'))
    # Do "gn gen" in a temp dir to prevent dirtying |out_dir|.
    subprocess.check_call(['gn', 'gen', tmp, '--ide=json', '-q'])
    gn_json = json.load(open(os.path.join(tmp, 'project.json')))
  finally:
    if tmp:
      shutil.rmtree(tmp)
  q.put(ParseGNProjectJSON(gn_json, out_dir, tmp))


def ParseGNProjectJSON(gn, out_dir, tmp_out):
  """Parse GN output and get the header files"""
  all_headers = set()

  for _target, properties in gn['targets'].iteritems():
    sources = properties.get('sources', [])
    public = properties.get('public', [])
    # Exclude '"public": "*"'.
    if type(public) is list:
      sources += public
    for f in sources:
      if f.endswith('.h') or f.endswith('.hh'):
        if f.startswith('//'):
          f = f[2:]  # Strip the '//' prefix.
          if f.startswith(tmp_out):
            f = out_dir + f[len(tmp_out):]
          all_headers.add(f)

  return all_headers


def GetDepsPrefixes(q):
  """Return all the folders controlled by DEPS file"""
  gclient_out = subprocess.check_output(
      ['gclient', 'recurse', '--no-progress', '-j1',
       'python', '-c', 'import os;print os.environ["GCLIENT_DEP_PATH"]'])
  prefixes = set()
  for i in gclient_out.split('\n'):
    if i.startswith('src/'):
      i = i[4:]
      prefixes.add(i)
  q.put(prefixes)


def ParseWhiteList(whitelist):
  out = set()
  for line in whitelist.split('\n'):
    line = re.sub(r'#.*', '', line).strip()
    if line:
      out.add(line)
  return out


def FilterOutDepsedRepo(files, deps):
  return {f for f in files if not any(f.startswith(d) for d in deps)}


def GetNonExistingFiles(lst):
  out = set()
  for f in lst:
    if not os.path.isfile(f):
      out.add(f)
  return out


def main():
  parser = argparse.ArgumentParser(description='''
      NOTE: Use ninja to build all targets in OUT_DIR before running
      this script.''')
  parser.add_argument('--out-dir', metavar='OUT_DIR', default='out/Release',
                      help='output directory of the build')
  parser.add_argument('--json',
                      help='JSON output filename for missing headers')
  parser.add_argument('--whitelist', help='file containing whitelist')

  args, _extras = parser.parse_known_args()

  if not os.path.isdir(args.out_dir):
    parser.error('OUT_DIR "%s" does not exist.' % args.out_dir)

  d_q = Queue()
  d_p = Process(target=GetHeadersFromNinja, args=(args.out_dir, d_q,))
  d_p.start()

  gn_q = Queue()
  gn_p = Process(target=GetHeadersFromGN, args=(args.out_dir, gn_q,))
  gn_p.start()

  deps_q = Queue()
  deps_p = Process(target=GetDepsPrefixes, args=(deps_q,))
  deps_p.start()

  d = d_q.get()
  gn = gn_q.get()
  missing = d - gn
  nonexisting = GetNonExistingFiles(gn)

  deps = deps_q.get()
  missing = FilterOutDepsedRepo(missing, deps)
  nonexisting = FilterOutDepsedRepo(nonexisting, deps)

  d_p.join()
  gn_p.join()
  deps_p.join()

  if len(GetNonExistingFiles(d)) > 0:
    parser.error('''Found non-existing files in ninja deps. You should
        build all in OUT_DIR.''')
  if len(d) == 0:
    parser.error('OUT_DIR looks empty. You should build all there.')
  if any((('/gen/' in i) for i in nonexisting)):
    parser.error('OUT_DIR looks wrong. You should build all there.')

  if args.whitelist:
    whitelist = ParseWhiteList(open(args.whitelist).read())
    missing -= whitelist
    nonexisting -= whitelist

  missing = sorted(missing)
  nonexisting = sorted(nonexisting)

  if args.json:
    with open(args.json, 'w') as f:
      json.dump(sorted(missing + nonexisting), f)

  if len(missing) == 0 and len(nonexisting) == 0:
    return 0

  if len(missing) > 0:
    print '\nThe following files should be included in gn files:'
    for i in missing:
      print i

  if len(nonexisting) > 0:
    print '\nThe following non-existing files should be removed from gn files:'
    for i in nonexisting:
      print i

  return 1


if __name__ == '__main__':
  sys.exit(main())
