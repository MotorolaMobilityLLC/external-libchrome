# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class ContentSettings(dict):

  """A dict interface to interact with device content settings.

  System properties are key/value pairs as exposed by adb shell content.
  """

  def __init__(self, table, adb):
    super(ContentSettings, self).__init__()
    self._table = table
    self._adb = adb

  def _GetTypeBinding(self, value):
    if isinstance(value, bool):
      return 'b'
    if isinstance(value, float):
      return 'f'
    if isinstance(value, int):
      return 'i'
    if isinstance(value, long):
      return 'l'
    if isinstance(value, str):
      return 's'
    raise ValueError('Unsupported type %s' % type(value))

  def iteritems(self):
    # Example row:
    # 'Row: 0 _id=13, name=logging_id2, value=-1fccbaa546705b05'
    for row in self._adb.RunShellCommandWithSU(
        'content query --uri content://%s' % self._table):
      fields = row.split(', ')
      key = None
      value = None
      for field in fields:
        k, _, v = field.partition('=')
        if k == 'name':
          key = v
        elif k == 'value':
          value = v
      assert key, value
      yield key, value

  def __getitem__(self, key):
    return self._adb.RunShellCommandWithSU(
        'content query --uri content://%s --where "name=\'%s\'" '
        '--projection value' % (self._table, key)).strip()

  def __setitem__(self, key, value):
    if key in self:
      self._adb.RunShellCommandWithSU(
          'content update --uri content://%s '
          '--bind value:%s:%s --where "name=\'%s\'"' % (
              self._table,
              self._GetTypeBinding(value), value, key))
    else:
      self._adb.RunShellCommandWithSU(
          'content insert --uri content://%s '
          '--bind name:%s:%s --bind value:%s:%s' % (
              self._table,
              self._GetTypeBinding(key), key,
              self._GetTypeBinding(value), value))

  def __delitem__(self, key):
    self._adb.RunShellCommandWithSU(
        'content delete --uri content://%s '
        '--bind name:%s:%s' % (
            self._table,
            self._GetTypeBinding(key), key))
