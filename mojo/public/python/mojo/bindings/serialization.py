# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility classes for serialization"""

import struct


# Format of a header for a struct or an array.
HEADER_STRUCT = struct.Struct("<II")


class SerializationException(Exception):
  """Error when strying to serialize a struct."""
  pass


class DeserializationException(Exception):
  """Error when strying to deserialize a struct."""
  pass


class DeserializationContext(object):

  def ClaimHandle(self, handle):
    raise NotImplementedError()

  def ClaimMemory(self, start, size):
    raise NotImplementedError()

  def GetSubContext(self, offset):
    raise NotImplementedError()

  def IsInitialContext(self):
    raise NotImplementedError()


class RootDeserializationContext(DeserializationContext):
  def __init__(self, data, handles):
    if isinstance(data, buffer):
      self.data = data
    else:
      self.data = buffer(data)
    self._handles = handles
    self._next_handle = 0;
    self._next_memory = 0;

  def ClaimHandle(self, handle):
    if handle < self._next_handle:
      raise DeserializationException('Accessing handles out of order.')
    self._next_handle = handle + 1
    return self._handles[handle]

  def ClaimMemory(self, start, size):
    if start < self._next_memory:
      raise DeserializationException('Accessing buffer out of order.')
    self._next_memory = start + size

  def GetSubContext(self, offset):
    return _ChildDeserializationContext(self, offset)

  def IsInitialContext(self):
    return True


class _ChildDeserializationContext(DeserializationContext):
  def __init__(self, parent, offset):
    self._parent = parent
    self._offset = offset
    self.data = buffer(parent.data, offset)

  def ClaimHandle(self, handle):
    return self._parent.ClaimHandle(handle)

  def ClaimMemory(self, start, size):
    return self._parent.ClaimMemory(self._offset + start, size)

  def GetSubContext(self, offset):
    return self._parent.GetSubContext(self._offset + offset)

  def IsInitialContext(self):
    return False


class Serialization(object):
  """
  Helper class to serialize/deserialize a struct.
  """
  def __init__(self, groups):
    self.version = _GetVersion(groups)
    self._groups = groups
    main_struct = _GetStruct(groups)
    self.size = HEADER_STRUCT.size + main_struct.size
    self._struct_per_version = {
        self.version: main_struct,
    }
    self._groups_per_version = {
        self.version: groups,
    }

  def _GetMainStruct(self):
    return self._GetStruct(self.version)

  def _GetGroups(self, version):
    # If asking for a version greater than the last known.
    version = min(version, self.version)
    if version not in self._groups_per_version:
      self._groups_per_version[version] = _FilterGroups(self._groups, version)
    return self._groups_per_version[version]

  def _GetStruct(self, version):
    # If asking for a version greater than the last known.
    version = min(version, self.version)
    if version not in self._struct_per_version:
      self._struct_per_version[version] = _GetStruct(self._GetGroups(version))
    return self._struct_per_version[version]

  def Serialize(self, obj, handle_offset):
    """
    Serialize the given obj. handle_offset is the the first value to use when
    encoding handles.
    """
    handles = []
    data = bytearray(self.size)
    HEADER_STRUCT.pack_into(data, 0, self.size, self.version)
    position = HEADER_STRUCT.size
    to_pack = []
    for group in self._groups:
      position = position + NeededPaddingForAlignment(position,
                                                      group.GetByteSize())
      (entry, new_handles) = group.Serialize(
          obj,
          len(data) - position,
          data,
          handle_offset + len(handles))
      to_pack.append(entry)
      handles.extend(new_handles)
      position = position + group.GetByteSize()
    self._GetMainStruct().pack_into(data, HEADER_STRUCT.size, *to_pack)
    return (data, handles)

  def Deserialize(self, fields, context):
    if len(context.data) < HEADER_STRUCT.size:
      raise DeserializationException(
          'Available data too short to contain header.')
    (size, version) = HEADER_STRUCT.unpack_from(context.data)
    if len(context.data) < size or size < HEADER_STRUCT.size:
      raise DeserializationException('Header size is incorrect.')
    if context.IsInitialContext():
      context.ClaimMemory(0, size)
    version_struct = self._GetStruct(version)
    entitities = version_struct.unpack_from(context.data, HEADER_STRUCT.size)
    filtered_groups = self._GetGroups(version)
    position = HEADER_STRUCT.size
    for (group, value) in zip(filtered_groups, entitities):
      position = position + NeededPaddingForAlignment(position,
                                                      group.GetByteSize())
      fields.update(group.Deserialize(value, context.GetSubContext(position)))
      position += group.GetByteSize()


def NeededPaddingForAlignment(value, alignment=8):
  """Returns the padding necessary to align value with the given alignment."""
  if value % alignment:
    return alignment - (value % alignment)
  return 0


def _GetVersion(groups):
  return sum([len(x.descriptors) for x in groups])


def _FilterGroups(groups, version):
  return [group for group in groups if group.GetVersion() < version]


def _GetStruct(groups):
  index = 0
  codes = [ '<' ]
  for group in groups:
    code = group.GetTypeCode()
    size = group.GetByteSize()
    needed_padding = NeededPaddingForAlignment(index, size)
    if needed_padding:
      codes.append('x' * needed_padding)
      index = index + needed_padding
    codes.append(code)
    index = index + size
  alignment_needed = NeededPaddingForAlignment(index)
  if alignment_needed:
    codes.append('x' * alignment_needed)
  return struct.Struct(''.join(codes))
