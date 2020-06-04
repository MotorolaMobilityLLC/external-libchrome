# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Translates parse tree to Mojom IR."""


import ast


def _MapTree(func, tree, name):
  if not tree:
    return []
  return [func(subtree) for subtree in tree if subtree[0] == name]

def _MapKind(kind):
  map_to_kind = { 'bool': 'b',
                  'int8': 'i8',
                  'int16': 'i16',
                  'int32': 'i32',
                  'int64': 'i64',
                  'uint8': 'u8',
                  'uint16': 'u16',
                  'uint32': 'u32',
                  'uint64': 'u64',
                  'float': 'f',
                  'double': 'd',
                  'string': 's',
                  'handle': 'h',
                  'handle<data_pipe_consumer>': 'h:d:c',
                  'handle<data_pipe_producer>': 'h:d:p',
                  'handle<message_pipe>': 'h:m',
                  'handle<shared_buffer>': 'h:s'}
  if kind.endswith('[]'):
    return 'a:' + _MapKind(kind[0:len(kind)-2])
  if kind in map_to_kind:
    return map_to_kind[kind]
  return 'x:' + kind

def _GetAttribute(attributes, name):
  out = None
  if attributes:
    for attribute in attributes:
      if attribute[0] == 'ATTRIBUTE' and attribute[1] == name:
        out = attribute[2]
  return out

def _MapField(tree):
  assert type(tree[3]) is ast.Ordinal
  return {'name': tree[2],
          'kind': _MapKind(tree[1]),
          'ordinal': tree[3].value,
          'default': tree[4]}

def _MapParameter(tree):
  assert type(tree[3]) is ast.Ordinal
  return {'name': tree[2],
          'kind': _MapKind(tree[1]),
          'ordinal': tree[3].value}

def _MapMethod(tree):
  assert type(tree[3]) is ast.Ordinal
  method = {'name': tree[1],
            'parameters': _MapTree(_MapParameter, tree[2], 'PARAM'),
            'ordinal': tree[3].value}
  if tree[4] != None:
    method['response_parameters'] = _MapTree(_MapParameter, tree[4], 'PARAM')
  return method

def _MapEnumField(tree):
  return {'name': tree[1],
          'value': tree[2]}

def _MapStruct(tree):
  struct = {}
  struct['name'] = tree[1]
  # TODO(darin): Add support for |attributes|
  #struct['attributes'] = MapAttributes(tree[2])
  struct['fields'] = _MapTree(_MapField, tree[3], 'FIELD')
  struct['enums'] = _MapTree(_MapEnum, tree[3], 'ENUM')
  return struct

def _MapInterface(tree):
  interface = {}
  interface['name'] = tree[1]
  interface['peer'] = _GetAttribute(tree[2], 'Peer')
  interface['methods'] = _MapTree(_MapMethod, tree[3], 'METHOD')
  interface['enums'] = _MapTree(_MapEnum, tree[3], 'ENUM')
  return interface

def _MapEnum(tree):
  enum = {}
  enum['name'] = tree[1]
  enum['fields'] = _MapTree(_MapEnumField, tree[2], 'ENUM_FIELD')
  return enum

def _MapModule(tree, name):
  mojom = {}
  mojom['name'] = name
  mojom['namespace'] = tree[1]
  mojom['structs'] = _MapTree(_MapStruct, tree[2], 'STRUCT')
  mojom['interfaces'] = _MapTree(_MapInterface, tree[2], 'INTERFACE')
  mojom['enums'] = _MapTree(_MapEnum, tree[2], 'ENUM')
  return mojom

def _MapImport(tree):
  import_item = {}
  import_item['filename'] = tree[1]
  return import_item


class _MojomBuilder(object):
  def __init__(self):
    self.mojom = {}

  def Build(self, tree, name):
    modules = [_MapModule(item, name) for item in tree if item[0] == 'MODULE']
    if len(modules) != 1:
      raise Exception('A mojom file must contain exactly 1 module.')
    self.mojom = modules[0]
    self.mojom['imports'] = _MapTree(_MapImport, tree, 'IMPORT')
    return self.mojom


def Translate(tree, name):
  return _MojomBuilder().Build(tree, name)
