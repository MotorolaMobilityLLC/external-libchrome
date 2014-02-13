# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import mojom

# mojom_data provides a mechanism to turn mojom Modules to dictionaries and
# back again. This can be used to persist a mojom Module created progromatically
# or to read a dictionary from code or a file.
# Example:
# test_dict = {
#   'name': 'test',
#   'namespace': 'testspace',
#   'structs': [{
#     'name': 'teststruct',
#     'fields': [
#       {'name': 'testfield1', 'kind': 'i32'},
#       {'name': 'testfield2', 'kind': 'a:i32', 'ordinal': 42}]}],
#   'interfaces': [{
#     'name': 'Server',
#     'methods': [{
#       'name': 'Foo',
#       'parameters': [{
#         'name': 'foo', 'kind': 'i32'},
#         {'name': 'bar', 'kind': 'a:x:teststruct'}],
#     'ordinal': 42}]}]
# }
# test_module = mojom_data.ModuleFromData(test_dict)


# Used to create a subclass of str that supports sorting by index, to make
# pretty printing maintain the order.
def istr(index, string):
  class IndexedString(str):
    def __lt__(self, other):
      return self.__index__ < other.__index__

  istr = IndexedString(string)
  istr.__index__ = index
  return istr

def KindToData(kind):
  return kind.spec

def KindFromData(kinds, data):
  if kinds.has_key(data):
    return kinds[data]
  if data.startswith('a:'):
    kind = mojom.Array()
    kind.kind = KindFromData(kinds, data[2:])
  else:
    kind = mojom.Kind()
  kind.spec = data
  kinds[data] = kind
  return kind

def ImportFromData(module, data):
  import_module = data['module']

  import_item = {}
  import_item['module_name'] = import_module.name
  import_item['namespace'] = import_module.namespace

  # Copy the struct kinds from our imports into the current module.
  for kind in import_module.kinds.itervalues():
    # TODO(mpcomplete): Handle enums
    if isinstance(kind, mojom.Struct) and kind.imported_from is None:
      kind = mojom.Struct.CreateFromImport(kind, import_item)
      module.kinds[kind.spec] = kind
  return import_item

def StructToData(struct):
  return {
    istr(0, 'name'): struct.name,
    istr(1, 'fields'): map(FieldToData, struct.fields)
  }

def StructFromData(module, data):
  struct = mojom.Struct()
  struct.name = data['name']
  struct.spec = 'x:' + module.namespace + '.' + struct.name
  module.kinds[struct.spec] = struct
  struct.fields = map(lambda field:
      FieldFromData(module.kinds, field), data['fields'])
  if data.has_key('enums'):
    struct.enums = map(lambda enum:
        EnumFromData(module.kinds, enum), data['enums'])
  return struct

def FieldToData(field):
  data = {
    istr(0, 'name'): field.name,
    istr(1, 'kind'): KindToData(field.kind)
  }
  if field.ordinal != None:
    data[istr(2, 'ordinal')] = field.ordinal
  if field.default != None:
    data[istr(3, 'default')] = field.default
  return data

def FieldFromData(kinds, data):
  field = mojom.Field()
  field.name = data['name']
  field.kind = KindFromData(kinds, data['kind'])
  field.ordinal = data.get('ordinal')
  field.default = data.get('default')
  return field

def ParameterToData(parameter):
  data = {
    istr(0, 'name'): parameter.name,
    istr(1, 'kind'): parameter.kind.spec
  }
  if parameter.ordinal != None:
    data[istr(2, 'ordinal')] = parameter.ordinal
  if parameter.default != None:
    data[istr(3, 'default')] = parameter.default
  return data

def ParameterFromData(kinds, data):
  parameter = mojom.Parameter()
  parameter.name = data['name']
  parameter.kind = KindFromData(kinds, data['kind'])
  parameter.ordinal = data.get('ordinal')
  parameter.default = data.get('default')
  return parameter

def MethodToData(method):
  data = {
    istr(0, 'name'):       method.name,
    istr(1, 'parameters'): map(ParameterToData, method.parameters)
  }
  if method.ordinal != None:
    data[istr(2, 'ordinal')] = method.ordinal
  return data

def MethodFromData(kinds, data):
  method = mojom.Method()
  method.name = data['name']
  method.ordinal = data.get('ordinal')
  method.default = data.get('default')
  method.parameters = map(
      lambda parameter: ParameterFromData(kinds, parameter), data['parameters'])
  return method

def InterfaceToData(interface):
  return {
    istr(0, 'name'):    interface.name,
    istr(1, 'peer'):    interface.peer,
    istr(2, 'methods'): map(MethodToData, interface.methods)
  }

def InterfaceFromData(module, data):
  interface = mojom.Interface()
  interface.name = data['name']
  interface.spec = 'x:' + module.namespace + '.' + interface.name
  interface.peer = data['peer'] if data.has_key('peer') else None
  module.kinds[interface.spec] = interface
  interface.methods = map(
      lambda method: MethodFromData(module.kinds, method), data['methods'])
  if data.has_key('enums'):
    interface.enums = map(lambda enum:
        EnumFromData(module.kinds, enum), data['enums'])
  return interface

def EnumFieldFromData(kinds, data):
  field = mojom.EnumField()
  field.name = data['name']
  field.value = data['value']
  return field

def EnumFromData(kinds, data):
  enum = mojom.Enum()
  enum.name = data['name']
  enum.fields = map(
      lambda field: EnumFieldFromData(kinds, field), data['fields'])
  return enum

def ModuleToData(module):
  return {
    istr(0, 'name'):       module.name,
    istr(1, 'namespace'):  module.namespace,
    istr(2, 'structs'):    map(StructToData, module.structs),
    istr(3, 'interfaces'): map(InterfaceToData, module.interfaces)
  }

def ModuleFromData(data):
  module = mojom.Module()
  module.kinds = {}
  for kind in mojom.PRIMITIVES:
    module.kinds[kind.spec] = kind

  module.name = data['name']
  module.namespace = data['namespace']
  # Imports must come first, because they add to module.kinds which is used
  # by by the others.
  module.imports = map(
      lambda import_data: ImportFromData(module, import_data),
      data['imports'])
  module.structs = map(
      lambda struct: StructFromData(module, struct), data['structs'])
  module.interfaces = map(
      lambda interface: InterfaceFromData(module, interface),
      data['interfaces'])
  if data.has_key('enums'):
    module.enums = map(
        lambda enum: EnumFromData(module.kinds, enum), data['enums'])
  return module

def OrderedModuleFromData(data):
  module = ModuleFromData(data)
  next_interface_ordinal = 0
  for interface in module.interfaces:
    next_ordinal = 0
    for method in interface.methods:
      if method.ordinal is None:
        method.ordinal = next_ordinal
      next_ordinal = method.ordinal + 1
  return module
