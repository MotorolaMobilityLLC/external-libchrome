# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import ast
import contextlib
import fnmatch
import json
import os
import pipes
import shlex
import shutil
import subprocess
import sys
import tempfile
import zipfile


CHROMIUM_SRC = os.path.normpath(
    os.path.join(os.path.dirname(__file__),
                 os.pardir, os.pardir, os.pardir, os.pardir))
COLORAMA_ROOT = os.path.join(CHROMIUM_SRC,
                             'third_party', 'colorama', 'src')


@contextlib.contextmanager
def TempDir():
  dirname = tempfile.mkdtemp()
  try:
    yield dirname
  finally:
    shutil.rmtree(dirname)


def MakeDirectory(dir_path):
  try:
    os.makedirs(dir_path)
  except OSError:
    pass


def DeleteDirectory(dir_path):
  if os.path.exists(dir_path):
    shutil.rmtree(dir_path)


def Touch(path, fail_if_missing=False):
  if fail_if_missing and not os.path.exists(path):
    raise Exception(path + ' doesn\'t exist.')

  MakeDirectory(os.path.dirname(path))
  with open(path, 'a'):
    os.utime(path, None)


def FindInDirectory(directory, filename_filter):
  files = []
  for root, _dirnames, filenames in os.walk(directory):
    matched_files = fnmatch.filter(filenames, filename_filter)
    files.extend((os.path.join(root, f) for f in matched_files))
  return files


def FindInDirectories(directories, filename_filter):
  all_files = []
  for directory in directories:
    all_files.extend(FindInDirectory(directory, filename_filter))
  return all_files


def ParseGnList(gn_string):
  return ast.literal_eval(gn_string)


def ParseGypList(gyp_string):
  # The ninja generator doesn't support $ in strings, so use ## to
  # represent $.
  # TODO(cjhopman): Remove when
  # https://code.google.com/p/gyp/issues/detail?id=327
  # is addressed.
  gyp_string = gyp_string.replace('##', '$')

  if gyp_string.startswith('['):
    return ParseGnList(gyp_string)
  return shlex.split(gyp_string)


def CheckOptions(options, parser, required=None):
  if not required:
    return
  for option_name in required:
    if getattr(options, option_name) is None:
      parser.error('--%s is required' % option_name.replace('_', '-'))


def WriteJson(obj, path, only_if_changed=False):
  old_dump = None
  if os.path.exists(path):
    with open(path, 'r') as oldfile:
      old_dump = oldfile.read()

  new_dump = json.dumps(obj, sort_keys=True, indent=2, separators=(',', ': '))

  if not only_if_changed or old_dump != new_dump:
    with open(path, 'w') as outfile:
      outfile.write(new_dump)


def ReadJson(path):
  with open(path, 'r') as jsonfile:
    return json.load(jsonfile)


class CalledProcessError(Exception):
  """This exception is raised when the process run by CheckOutput
  exits with a non-zero exit code."""

  def __init__(self, cwd, args, output):
    super(CalledProcessError, self).__init__()
    self.cwd = cwd
    self.args = args
    self.output = output

  def __str__(self):
    # A user should be able to simply copy and paste the command that failed
    # into their shell.
    copyable_command = '( cd {}; {} )'.format(os.path.abspath(self.cwd),
        ' '.join(map(pipes.quote, self.args)))
    return 'Command failed: {}\n{}'.format(copyable_command, self.output)


# This can be used in most cases like subprocess.check_output(). The output,
# particularly when the command fails, better highlights the command's failure.
# If the command fails, raises a build_utils.CalledProcessError.
def CheckOutput(args, cwd=None,
                print_stdout=False, print_stderr=True,
                stdout_filter=None,
                stderr_filter=None,
                fail_func=lambda returncode, stderr: returncode != 0):
  if not cwd:
    cwd = os.getcwd()

  child = subprocess.Popen(args,
      stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd=cwd)
  stdout, stderr = child.communicate()

  if stdout_filter is not None:
    stdout = stdout_filter(stdout)

  if stderr_filter is not None:
    stderr = stderr_filter(stderr)

  if fail_func(child.returncode, stderr):
    raise CalledProcessError(cwd, args, stdout + stderr)

  if print_stdout:
    sys.stdout.write(stdout)
  if print_stderr:
    sys.stderr.write(stderr)

  return stdout


def GetModifiedTime(path):
  # For a symlink, the modified time should be the greater of the link's
  # modified time and the modified time of the target.
  return max(os.lstat(path).st_mtime, os.stat(path).st_mtime)


def IsTimeStale(output, inputs):
  if not os.path.exists(output):
    return True

  output_time = GetModifiedTime(output)
  for i in inputs:
    if GetModifiedTime(i) > output_time:
      return True
  return False


def IsDeviceReady():
  device_state = CheckOutput(['adb', 'get-state'])
  return device_state.strip() == 'device'


def CheckZipPath(name):
  if os.path.normpath(name) != name:
    raise Exception('Non-canonical zip path: %s' % name)
  if os.path.isabs(name):
    raise Exception('Absolute zip path: %s' % name)


def ExtractAll(zip_path, path=None, no_clobber=True):
  if path is None:
    path = os.getcwd()
  elif not os.path.exists(path):
    MakeDirectory(path)

  with zipfile.ZipFile(zip_path) as z:
    for name in z.namelist():
      CheckZipPath(name)
      if no_clobber:
        output_path = os.path.join(path, name)
        if os.path.exists(output_path):
          raise Exception(
              'Path already exists from zip: %s %s %s'
              % (zip_path, name, output_path))

    z.extractall(path=path)


def DoZip(inputs, output, base_dir):
  with zipfile.ZipFile(output, 'w') as outfile:
    for f in inputs:
      CheckZipPath(os.path.relpath(f, base_dir))
      outfile.write(f, os.path.relpath(f, base_dir))


def PrintWarning(message):
  print 'WARNING: ' + message


def PrintBigWarning(message):
  print '*****     ' * 8
  PrintWarning(message)
  print '*****     ' * 8


def GetPythonDependencies():
  """Gets the paths of imported non-system python modules.

  A path is assumed to be a "system" import if it is outside of chromium's
  src/. The paths will be relative to the current directory.
  """
  module_paths = (m.__file__ for m in sys.modules.itervalues()
                  if m is not None and hasattr(m, '__file__'))

  abs_module_paths = map(os.path.abspath, module_paths)

  non_system_module_paths = [
      p for p in abs_module_paths if p.startswith(CHROMIUM_SRC)]
  def ConvertPycToPy(s):
    if s.endswith('.pyc'):
      return s[:-1]
    return s

  non_system_module_paths = map(ConvertPycToPy, non_system_module_paths)
  non_system_module_paths = map(os.path.relpath, non_system_module_paths)
  return sorted(set(non_system_module_paths))


def AddDepfileOption(parser):
  parser.add_option('--depfile',
                    help='Path to depfile. This must be specified as the '
                    'action\'s first output.')


def WriteDepfile(path, dependencies):
  with open(path, 'w') as depfile:
    depfile.write(path)
    depfile.write(': ')
    depfile.write(' '.join(dependencies))
    depfile.write('\n')


def ExpandFileArgs(args):
  """Replaces file-arg placeholders in args.

  These placeholders have the form:
    @(filename:key1:key2:...:keyn)

  The value of such a placeholder is calculated by reading 'filename' as json.
  And then extracting the value at [key1][key2]...[keyn].

  Note: This intentionally does not return the list of files that appear in such
  placeholders. An action that uses file-args *must* know the paths of those
  files prior to the parsing of the arguments (typically by explicitly listing
  them in the action's inputs in build files).
  """
  new_args = list(args)
  file_jsons = dict()
  for i, arg in enumerate(args):
    start = arg.find('@(')
    if start < 0:
      continue
    end = arg[start:].find(')')
    if end < 0:
      continue
    end += start

    if '@(' in arg[end:]:
      raise Exception('Only one file-lookup-expansion is allowed in each arg.')

    lookup_path = arg[start + 2:end].split(':')
    file_path = lookup_path[0]
    if not file_path in file_jsons:
      file_jsons[file_path] = ReadJson(file_path)

    expansion = file_jsons[file_path]
    for k in lookup_path[1:]:
      expansion = expansion[k]

    new_args[i] = arg[:start] + str(expansion) + arg[end + 1:]
  return new_args

