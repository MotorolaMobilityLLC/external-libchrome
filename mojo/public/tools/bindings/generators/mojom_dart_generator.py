# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates dart source files from a mojom.Module."""

import re

import mojom.generate.generator as generator
import mojom.generate.module as mojom
import mojom.generate.pack as pack
from mojom.generate.template_expander import UseJinja

GENERATOR_PREFIX = 'dart'

_HEADER_SIZE = 8

_kind_to_dart_default_value = {
  mojom.BOOL:                  "false",
  mojom.INT8:                  "0",
  mojom.UINT8:                 "0",
  mojom.INT16:                 "0",
  mojom.UINT16:                "0",
  mojom.INT32:                 "0",
  mojom.UINT32:                "0",
  mojom.FLOAT:                 "0.0",
  mojom.HANDLE:                "null",
  mojom.DCPIPE:                "null",
  mojom.DPPIPE:                "null",
  mojom.MSGPIPE:               "null",
  mojom.SHAREDBUFFER:          "null",
  mojom.NULLABLE_HANDLE:       "null",
  mojom.NULLABLE_DCPIPE:       "null",
  mojom.NULLABLE_DPPIPE:       "null",
  mojom.NULLABLE_MSGPIPE:      "null",
  mojom.NULLABLE_SHAREDBUFFER: "null",
  mojom.INT64:                 "0",
  mojom.UINT64:                "0",
  mojom.DOUBLE:                "0.0",
  mojom.STRING:                "null",
  mojom.NULLABLE_STRING:       "null"
}

_kind_to_dart_decl_type = {
  mojom.BOOL:                  "bool",
  mojom.INT8:                  "int",
  mojom.UINT8:                 "int",
  mojom.INT16:                 "int",
  mojom.UINT16:                "int",
  mojom.INT32:                 "int",
  mojom.UINT32:                "int",
  mojom.FLOAT:                 "double",
  mojom.HANDLE:                "core.MojoHandle",
  mojom.DCPIPE:                "core.MojoHandle",
  mojom.DPPIPE:                "core.MojoHandle",
  mojom.MSGPIPE:               "core.MojoHandle",
  mojom.SHAREDBUFFER:          "core.MojoHandle",
  mojom.NULLABLE_HANDLE:       "core.MojoHandle",
  mojom.NULLABLE_DCPIPE:       "core.MojoHandle",
  mojom.NULLABLE_DPPIPE:       "core.MojoHandle",
  mojom.NULLABLE_MSGPIPE:      "core.MojoHandle",
  mojom.NULLABLE_SHAREDBUFFER: "core.MojoHandle",
  mojom.INT64:                 "int",
  mojom.UINT64:                "int",
  mojom.DOUBLE:                "double",
  mojom.STRING:                "String",
  mojom.NULLABLE_STRING:       "String"
}

_spec_to_decode_method = {
  mojom.BOOL.spec:                  'decodeBool',
  mojom.DCPIPE.spec:                'decodeHandle',
  mojom.DOUBLE.spec:                'decodeDouble',
  mojom.DPPIPE.spec:                'decodeHandle',
  mojom.FLOAT.spec:                 'decodeFloat',
  mojom.HANDLE.spec:                'decodeHandle',
  mojom.INT16.spec:                 'decodeInt16',
  mojom.INT32.spec:                 'decodeInt32',
  mojom.INT64.spec:                 'decodeInt64',
  mojom.INT8.spec:                  'decodeInt8',
  mojom.MSGPIPE.spec:               'decodeHandle',
  mojom.NULLABLE_DCPIPE.spec:       'decodeHandle',
  mojom.NULLABLE_DPPIPE.spec:       'decodeHandle',
  mojom.NULLABLE_HANDLE.spec:       'decodeHandle',
  mojom.NULLABLE_MSGPIPE.spec:      'decodeHandle',
  mojom.NULLABLE_SHAREDBUFFER.spec: 'decodeHandle',
  mojom.NULLABLE_STRING.spec:       'decodeString',
  mojom.SHAREDBUFFER.spec:          'decodeHandle',
  mojom.STRING.spec:                'decodeString',
  mojom.UINT16.spec:                'decodeUint16',
  mojom.UINT32.spec:                'decodeUint32',
  mojom.UINT64.spec:                'decodeUint64',
  mojom.UINT8.spec:                 'decodeUint8',
}

_spec_to_encode_method = {
  mojom.BOOL.spec:                  'encodeBool',
  mojom.DCPIPE.spec:                'encodeHandle',
  mojom.DOUBLE.spec:                'encodeDouble',
  mojom.DPPIPE.spec:                'encodeHandle',
  mojom.FLOAT.spec:                 'encodeFloat',
  mojom.HANDLE.spec:                'encodeHandle',
  mojom.INT16.spec:                 'encodeInt16',
  mojom.INT32.spec:                 'encodeInt32',
  mojom.INT64.spec:                 'encodeInt64',
  mojom.INT8.spec:                  'encodeInt8',
  mojom.MSGPIPE.spec:               'encodeHandle',
  mojom.NULLABLE_DCPIPE.spec:       'encodeHandle',
  mojom.NULLABLE_DPPIPE.spec:       'encodeHandle',
  mojom.NULLABLE_HANDLE.spec:       'encodeHandle',
  mojom.NULLABLE_MSGPIPE.spec:      'encodeHandle',
  mojom.NULLABLE_SHAREDBUFFER.spec: 'encodeHandle',
  mojom.NULLABLE_STRING.spec:       'encodeString',
  mojom.SHAREDBUFFER.spec:          'encodeHandle',
  mojom.STRING.spec:                'encodeString',
  mojom.UINT16.spec:                'encodeUint16',
  mojom.UINT32.spec:                'encodeUint32',
  mojom.UINT64.spec:                'encodeUint64',
  mojom.UINT8.spec:                 'encodeUint8',
}

def GetDartType(kind):
  if kind.imported_from:
    return kind.imported_from["unique_name"] + "." + kind.name
  return kind.name

def DartDefaultValue(field):
  if field.default:
    if mojom.IsStructKind(field.kind):
      assert field.default == "default"
      return "new %s()" % GetDartType(field.kind)
    return ExpressionToText(field.default)
  if field.kind in mojom.PRIMITIVES:
    return _kind_to_dart_default_value[field.kind]
  if mojom.IsStructKind(field.kind):
    return "null"
  if mojom.IsArrayKind(field.kind):
    return "null"
  if mojom.IsMapKind(field.kind):
    return "null"
  if mojom.IsInterfaceKind(field.kind) or \
     mojom.IsInterfaceRequestKind(field.kind):
    return _kind_to_dart_default_value[mojom.MSGPIPE]
  if mojom.IsEnumKind(field.kind):
    return "0"

def DartDeclType(kind):
  if kind in mojom.PRIMITIVES:
    return _kind_to_dart_decl_type[kind]
  if mojom.IsStructKind(kind):
    return GetDartType(kind)
  if mojom.IsArrayKind(kind):
    array_type = DartDeclType(kind.kind)
    return "List<" + array_type + ">"
  if mojom.IsMapKind(kind):
    key_type = DartDeclType(kind.key_kind)
    value_type = DartDeclType(kind.value_kind)
    return "Map<"+ key_type + ", " + value_type + ">"
  if mojom.IsInterfaceKind(kind) or \
     mojom.IsInterfaceRequestKind(kind):
    return _kind_to_dart_decl_type[mojom.MSGPIPE]
  if mojom.IsEnumKind(kind):
    return "int"

def NameToComponent(name):
  # insert '_' between anything and a Title name (e.g, HTTPEntry2FooBar ->
  # HTTP_Entry2_FooBar)
  name = re.sub('([^_])([A-Z][^A-Z_]+)', r'\1_\2', name)
  # insert '_' between non upper and start of upper blocks (e.g.,
  # HTTP_Entry2_FooBar -> HTTP_Entry2_Foo_Bar)
  name = re.sub('([^A-Z_])([A-Z])', r'\1_\2', name)
  return [x.lower() for x in name.split('_')]

def UpperCamelCase(name):
  return ''.join([x.capitalize() for x in NameToComponent(name)])

def CamelCase(name):
  uccc = UpperCamelCase(name)
  return uccc[0].lower() + uccc[1:]

def ConstantStyle(name):
  components = NameToComponent(name)
  if components[0] == 'k' and len(components) > 1:
    components = components[1:]
  # variable cannot starts with a digit.
  if components[0][0].isdigit():
    components[0] = '_' + components[0]
  return '_'.join([x.upper() for x in components])

def GetNameForElement(element):
  if (mojom.IsEnumKind(element) or mojom.IsInterfaceKind(element) or
      mojom.IsStructKind(element)):
    return UpperCamelCase(element.name)
  if mojom.IsInterfaceRequestKind(element):
    return GetNameForElement(element.kind)
  if isinstance(element, (mojom.Method,
                          mojom.Parameter,
                          mojom.Field)):
    return CamelCase(element.name)
  if isinstance(element, mojom.EnumValue):
    return (GetNameForElement(element.enum) + '.' +
            ConstantStyle(element.name))
  if isinstance(element, (mojom.NamedValue,
                          mojom.Constant,
                          mojom.EnumField)):
    return ConstantStyle(element.name)
  raise Exception('Unexpected element: %s' % element)

def GetInterfaceResponseName(method):
  return UpperCamelCase(method.name + 'Response')

def GetResponseStructFromMethod(method):
  return generator.GetDataHeader(
      False, generator.GetResponseStructFromMethod(method))

def GetStructFromMethod(method):
  return generator.GetDataHeader(
      False, generator.GetStructFromMethod(method))

def GetDartTrueFalse(value):
  return 'true' if value else 'false'

def GetArrayNullabilityFlags(kind):
    """Returns nullability flags for an array type, see codec.dart.

    As we have dedicated decoding functions for arrays, we have to pass
    nullability information about both the array itself, as well as the array
    element type there.
    """
    assert mojom.IsArrayKind(kind)
    ARRAY_NULLABLE   = 'bindings.kArrayNullable'
    ELEMENT_NULLABLE = 'bindings.kElementNullable'
    NOTHING_NULLABLE = 'bindings.kNothingNullable'

    flags_to_set = []
    if mojom.IsNullableKind(kind):
        flags_to_set.append(ARRAY_NULLABLE)
    if mojom.IsNullableKind(kind.kind):
        flags_to_set.append(ELEMENT_NULLABLE)

    if not flags_to_set:
        flags_to_set = [NOTHING_NULLABLE]
    return ' | '.join(flags_to_set)

def AppendEncodeDecodeParams(initial_params, kind, bit):
  """ Appends standard parameters shared between encode and decode calls. """
  params = list(initial_params)
  if (kind == mojom.BOOL):
    params.append(str(bit))
  if mojom.IsReferenceKind(kind):
    if mojom.IsArrayKind(kind):
      params.append(GetArrayNullabilityFlags(kind))
    else:
      params.append(GetDartTrueFalse(mojom.IsNullableKind(kind)))
  if mojom.IsArrayKind(kind):
    params.append(GetArrayExpectedLength(kind))
  return params

def DecodeMethod(kind, offset, bit):
  def _DecodeMethodName(kind):
    if mojom.IsArrayKind(kind):
      return _DecodeMethodName(kind.kind) + 'Array'
    if mojom.IsEnumKind(kind):
      return _DecodeMethodName(mojom.INT32)
    if mojom.IsInterfaceRequestKind(kind):
      return 'decodeHandle'
    if mojom.IsInterfaceKind(kind):
      return 'decodeHandle'
    return _spec_to_decode_method[kind.spec]
  methodName = _DecodeMethodName(kind)
  params = AppendEncodeDecodeParams([ str(offset) ], kind, bit)
  return '%s(%s)' % (methodName, ', '.join(params))

def EncodeMethod(kind, variable, offset, bit):
  def _EncodeMethodName(kind):
    if mojom.IsStructKind(kind):
      return 'encodeStruct'
    if mojom.IsArrayKind(kind):
      return _EncodeMethodName(kind.kind) + 'Array'
    if mojom.IsEnumKind(kind):
      return _EncodeMethodName(mojom.INT32)
    if mojom.IsInterfaceRequestKind(kind):
      return 'encodeHandle'
    if mojom.IsInterfaceKind(kind):
      return 'encodeHandle'
    return _spec_to_encode_method[kind.spec]
  methodName = _EncodeMethodName(kind)
  params = AppendEncodeDecodeParams([ variable, str(offset) ], kind, bit)
  return '%s(%s)' % (methodName, ', '.join(params))

def TranslateConstants(token):
  if isinstance(token, (mojom.EnumValue, mojom.NamedValue)):
    # Both variable and enum constants are constructed like:
    # NamespaceUid.Struct.Enum_CONSTANT_NAME
    name = ""
    if token.imported_from:
      name = token.imported_from["unique_name"] + "."
    if token.parent_kind:
      name = name + token.parent_kind.name + "."
    if isinstance(token, mojom.EnumValue):
      name = name + token.enum.name + "_"
    return name + token.name

  if isinstance(token, mojom.BuiltinValue):
    if token.value == "double.INFINITY" or token.value == "float.INFINITY":
      return "double.INFINITY";
    if token.value == "double.NEGATIVE_INFINITY" or \
       token.value == "float.NEGATIVE_INFINITY":
      return "double.NEGATIVE_INFINITY";
    if token.value == "double.NAN" or token.value == "float.NAN":
      return "double.NAN";

  # Strip leading '+'.
  if token[0] == '+':
    token = token[1:]

  return token

def ExpressionToText(value):
  return TranslateConstants(value)

def GetArrayKind(kind, size = None):
  if size is None:
    return mojom.Array(kind)
  else:
    array = mojom.Array(kind, 0)
    array.dart_map_size = size
    return array

def GetArrayExpectedLength(kind):
  if mojom.IsArrayKind(kind) and kind.length is not None:
    return getattr(kind, 'dart_map_size', str(kind.length))
  else:
    return 'bindings.kUnspecifiedArrayLength'

def IsPointerArrayKind(kind):
  if not mojom.IsArrayKind(kind):
    return False
  sub_kind = kind.kind
  return mojom.IsObjectKind(sub_kind)

class Generator(generator.Generator):

  dart_filters = {
    'array_expected_length': GetArrayExpectedLength,
    'array': GetArrayKind,
    'decode_method': DecodeMethod,
    'default_value': DartDefaultValue,
    'encode_method': EncodeMethod,
    'expression_to_text': ExpressionToText,
    'is_handle': mojom.IsNonInterfaceHandleKind,
    'is_map_kind': mojom.IsMapKind,
    'is_nullable_kind': mojom.IsNullableKind,
    'is_pointer_array_kind': IsPointerArrayKind,
    'is_struct_kind': mojom.IsStructKind,
    'dart_true_false': GetDartTrueFalse,
    'dart_type': DartDeclType,
    'name': GetNameForElement,
    'interface_response_name': GetInterfaceResponseName,
    'response_struct_from_method': GetResponseStructFromMethod,
    'struct_from_method': GetStructFromMethod,
    'upper_camel_case': UpperCamelCase,
    'struct_size': lambda ps: ps.GetTotalSize() + _HEADER_SIZE,
  }

  def GetParameters(self):
    return {
      "namespace": self.module.namespace,
      "imports": self.GetImports(),
      "kinds": self.module.kinds,
      "enums": self.module.enums,
      "module": self.module,
      "structs": self.GetStructs() + self.GetStructsFromMethods(),
      "interfaces": self.module.interfaces,
      "imported_interfaces": self.GetImportedInterfaces(),
      "imported_from": self.ImportedFrom(),
    }

  @UseJinja("dart_templates/module.lib.tmpl", filters=dart_filters)
  def GenerateLibModule(self):
    return self.GetParameters()

  def GenerateFiles(self, args):
    self.Write(self.GenerateLibModule(),
        self.MatchMojomFilePath("%s.dart" % self.module.name))

  def GetImports(self):
    used_names = set()
    for each_import in self.module.imports:
      simple_name = each_import["module_name"].split(".")[0]

      # Since each import is assigned a library in Dart, they need to have
      # unique names.
      unique_name = simple_name
      counter = 0
      while unique_name in used_names:
        counter += 1
        unique_name = simple_name + str(counter)

      used_names.add(unique_name)
      each_import["unique_name"] = unique_name
      counter += 1
    return self.module.imports

  def GetImportedInterfaces(self):
    interface_to_import = {}
    for each_import in self.module.imports:
      for each_interface in each_import["module"].interfaces:
        name = each_interface.name
        interface_to_import[name] = each_import["unique_name"] + "." + name
    return interface_to_import

  def ImportedFrom(self):
    interface_to_import = {}
    for each_import in self.module.imports:
      for each_interface in each_import["module"].interfaces:
        name = each_interface.name
        interface_to_import[name] = each_import["unique_name"] + "."
    return interface_to_import
