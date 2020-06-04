# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
The descriptors used to define generated elements of the mojo python bindings.
"""

import array
import itertools
import struct

# pylint: disable=F0401
import mojo.bindings.serialization as serialization
import mojo.system


class Type(object):
  """Describes the type of a struct field or a method parameter,"""

  def Convert(self, value): # pylint: disable=R0201
    """
    Convert the given value into its canonical representation, raising an
    exception if the value cannot be converted.
    """
    return value

  def GetDefaultValue(self, value):
    """
    Returns the default value for this type associated with the given value.
    This method must be able to correcly handle value being None.
    """
    return self.Convert(value)


class SerializableType(Type):
  """Describe a type that can be serialized by itself."""

  def __init__(self, typecode):
    Type.__init__(self)
    self.typecode = typecode
    self.byte_size = struct.calcsize('=%s' % self.GetTypeCode())

  def GetTypeCode(self):
    """
    Returns the type code (as defined by the struct module) used to encode
    this type.
    """
    return self.typecode

  def GetByteSize(self):
    """
    Returns the size of the encoding of this type.
    """
    return self.byte_size

  def Serialize(self, value, data_offset, data, handle_offset):
    """
    Serialize a value of this type.

    Args:
      value: the value to serialize.
      data_offset: the offset to the end of the data bytearray. Used to encode
                   pointers.
      data: the bytearray to append additional data to.
      handle_offset: the offset to use to encode handles.

    Returns a a tuple where the first element is the value to encode, and the
    second is the array of handles to add to the message.
    """
    raise NotImplementedError()

  def Deserialize(self, value, data, handles):
    """
    Deserialize a value of this type.

    Args:
      value: the base value for this type. This is always a numeric type, and
             corresponds to the first element in the tuple returned by
             Serialize.
      data: the bytearray to retrieve additional data from.
      handles: the array of handles contained in the message to deserialize.

    Returns the deserialized value.
    """
    raise NotImplementedError()


class BooleanType(Type):
  """Type object for booleans"""

  def Convert(self, value):
    return bool(value)


class NumericType(SerializableType):
  """Base Type object for all numeric types"""

  def GetDefaultValue(self, value):
    if value is None:
      return self.Convert(0)
    return self.Convert(value)

  def Serialize(self, value, data_offset, data, handle_offset):
    return (value, [])

  def Deserialize(self, value, data, handles):
    return value


class IntegerType(NumericType):
  """Type object for integer types."""

  def __init__(self, typecode):
    NumericType.__init__(self, typecode)
    size = 8 * self.byte_size
    signed = typecode.islower()
    if signed:
      self._min_value = -(1 << (size - 1))
      self._max_value = (1 << (size - 1)) - 1
    else:
      self._min_value = 0
      self._max_value = (1 << size) - 1

  def Convert(self, value):
    if value is None:
      raise TypeError('None is not an integer.')
    if not isinstance(value, (int, long)):
      raise TypeError('%r is not an integer type' % value)
    if value < self._min_value or value > self._max_value:
      raise OverflowError('%r is not in the range [%d, %d]' %
                          (value, self._min_value, self._max_value))
    return value


class FloatType(NumericType):
  """Type object for floating point number types."""

  def Convert(self, value):
    if value is None:
      raise TypeError('None is not an floating point number.')
    if not isinstance(value, (int, long, float)):
      raise TypeError('%r is not a numeric type' % value)
    return float(value)


class PointerType(SerializableType):
  """Base Type object for pointers."""

  def __init__(self, nullable=False):
    SerializableType.__init__(self, 'Q')
    self.nullable = nullable

  def Serialize(self, value, data_offset, data, handle_offset):
    if value is None and not self.nullable:
      raise serialization.SerializationException(
          'Trying to serialize null for non nullable type.')
    if value is None:
      return (0, [])
    return self.SerializePointer(value, data_offset, data, handle_offset)

  def Deserialize(self, value, data, handles):
    if value == 0:
      if not self.nullable:
        raise serialization.DeserializationException(
            'Trying to deserialize null for non nullable type.')
      return None
    pointed_data = buffer(data, value)
    (size, nb_elements) = serialization.HEADER_STRUCT.unpack_from(pointed_data)
    return self.DeserializePointer(size, nb_elements, pointed_data, handles)

  def SerializePointer(self, value, data_offset, data, handle_offset):
    """Serialize the not null value."""
    raise NotImplementedError()

  def DeserializePointer(self, size, nb_elements, data, handles):
    raise NotImplementedError()


class StringType(PointerType):
  """
  Type object for strings.

  Strings are represented as unicode, and the conversion is done using the
  default encoding if a string instance is used.
  """

  def __init__(self, nullable=False):
    PointerType.__init__(self, nullable)
    self._array_type = NativeArrayType('B', nullable)

  def Convert(self, value):
    if value is None or isinstance(value, unicode):
      return value
    if isinstance(value, str):
      return unicode(value)
    raise TypeError('%r is not a string' % value)

  def SerializePointer(self, value, data_offset, data, handle_offset):
    string_array = array.array('b')
    string_array.fromstring(value.encode('utf8'))
    return self._array_type.SerializeArray(
        string_array, data_offset, data, handle_offset)

  def DeserializePointer(self, size, nb_elements, data, handles):
    string_array = self._array_type.DeserializeArray(
        size, nb_elements, data, handles)
    return unicode(string_array.tostring(), 'utf8')


class HandleType(SerializableType):
  """Type object for handles."""

  def __init__(self, nullable=False):
    SerializableType.__init__(self, 'i')
    self.nullable = nullable

  def Convert(self, value):
    if value is None:
      return mojo.system.Handle()
    if not isinstance(value, mojo.system.Handle):
      raise TypeError('%r is not a handle' % value)
    return value

  def Serialize(self, value, data_offset, data, handle_offset):
    if not value.IsValid() and not self.nullable:
      raise serialization.SerializationException(
          'Trying to serialize null for non nullable type.')
    if not value.IsValid():
      return (-1, [])
    return (handle_offset, [value])

  def Deserialize(self, value, data, handles):
    if value == -1:
      if not self.nullable:
        raise serialization.DeserializationException(
            'Trying to deserialize null for non nullable type.')
      return mojo.system.Handle()
    # TODO(qsr) validate handle order
    return handles[value]


class BaseArrayType(PointerType):
  """Abstract Type object for arrays."""

  def __init__(self, nullable=False, length=0):
    PointerType.__init__(self, nullable)
    self.length = length

  def SerializePointer(self, value, data_offset, data, handle_offset):
    if self.length != 0 and len(value) != self.length:
      raise serialization.SerializationException('Incorrect array size')
    return self.SerializeArray(value, data_offset, data, handle_offset)

  def SerializeArray(self, value, data_offset, data, handle_offset):
    """Serialize the not null array."""
    raise NotImplementedError()

  def DeserializePointer(self, size, nb_elements, data, handles):
    if self.length != 0 and size != self.length:
      raise serialization.DeserializationException('Incorrect array size')
    return self.DeserializeArray(size, nb_elements, data, handles)

  def DeserializeArray(self, size, nb_elements, data, handles):
    raise NotImplementedError()


class BooleanArrayType(BaseArrayType):

  def __init__(self, nullable=False, length=0):
    BaseArrayType.__init__(self, nullable, length)
    self._array_type = NativeArrayType('B', nullable)

  def Convert(self, value):
    if value is None:
      return value
    return [TYPE_BOOL.Convert(x) for x in value]

  def SerializeArray(self, value, data_offset, data, handle_offset):
    groups = [value[i:i+8] for i in range(0, len(value), 8)]
    converted = array.array('B', [_ConvertBooleansToByte(x) for x in groups])
    return _SerializeNativeArray(converted, data_offset, data, len(value))

  def DeserializeArray(self, size, nb_elements, data, handles):
    converted = self._array_type.DeserializeArray(
        size, nb_elements, data, handles)
    elements = list(itertools.islice(
        itertools.chain.from_iterable(
            [_ConvertByteToBooleans(x, 8) for x in converted]),
        0,
        nb_elements))
    return elements


class GenericArrayType(BaseArrayType):
  """Type object for arrays of pointers."""

  def __init__(self, sub_type, nullable=False, length=0):
    BaseArrayType.__init__(self, nullable, length)
    assert isinstance(sub_type, SerializableType)
    self.sub_type = sub_type

  def Convert(self, value):
    if value is None:
      return value
    return [self.sub_type.Convert(x) for x in value]

  def SerializeArray(self, value, data_offset, data, handle_offset):
    size = (serialization.HEADER_STRUCT.size +
            self.sub_type.GetByteSize() * len(value))
    data_end = len(data)
    position = len(data) + serialization.HEADER_STRUCT.size
    data.extend(bytearray(size +
                          serialization.NeededPaddingForAlignment(size)))
    returned_handles = []
    to_pack = []
    for item in value:
      (new_data, new_handles) = self.sub_type.Serialize(
          item,
          len(data) - position,
          data,
          handle_offset + len(returned_handles))
      to_pack.append(new_data)
      returned_handles.extend(new_handles)
      position = position + self.sub_type.GetByteSize()
    serialization.HEADER_STRUCT.pack_into(data, data_end, size, len(value))
    struct.pack_into('%d%s' % (len(value), self.sub_type.GetTypeCode()),
                     data,
                     data_end + serialization.HEADER_STRUCT.size,
                     *to_pack)
    return (data_offset, returned_handles)

  def DeserializeArray(self, size, nb_elements, data, handles):
    values = struct.unpack_from(
        '%d%s' % (nb_elements, self.sub_type.GetTypeCode()),
        buffer(data, serialization.HEADER_STRUCT.size))
    result = []
    position = serialization.HEADER_STRUCT.size
    for value in values:
      result.append(
          self.sub_type.Deserialize(value, buffer(data, position), handles))
      position += self.sub_type.GetByteSize()
    return result


class NativeArrayType(BaseArrayType):
  """Type object for arrays of native types."""

  def __init__(self, typecode, nullable=False, length=0):
    BaseArrayType.__init__(self, nullable, length)
    self.array_typecode = typecode

  def Convert(self, value):
    if value is None:
      return value
    if (isinstance(value, array.array) and
        value.array_typecode == self.array_typecode):
      return value
    return array.array(self.array_typecode, value)

  def SerializeArray(self, value, data_offset, data, handle_offset):
    return _SerializeNativeArray(value, data_offset, data, len(value))

  def DeserializeArray(self, size, nb_elements, data, handles):
    result = array.array(self.array_typecode)
    result.fromstring(buffer(data,
                             serialization.HEADER_STRUCT.size,
                             size - serialization.HEADER_STRUCT.size))
    return result


class StructType(PointerType):
  """Type object for structs."""

  def __init__(self, struct_type, nullable=False):
    PointerType.__init__(self)
    self.struct_type = struct_type
    self.nullable = nullable

  def Convert(self, value):
    if value is None or isinstance(value, self.struct_type):
      return value
    raise TypeError('%r is not an instance of %r' % (value, self.struct_type))

  def GetDefaultValue(self, value):
    if value:
      return self.struct_type()
    return None

  def SerializePointer(self, value, data_offset, data, handle_offset):
    (new_data, new_handles) = value.Serialize(handle_offset)
    data.extend(new_data)
    return (data_offset, new_handles)

  def DeserializePointer(self, size, nb_elements, data, handles):
    return self.struct_type.Deserialize(data, handles)


class NoneType(SerializableType):
  """Placeholder type, used temporarily until all mojo types are handled."""

  def __init__(self):
    SerializableType.__init__(self, 'B')

  def Convert(self, value):
    return None

  def Serialize(self, value, data_offset, data, handle_offset):
    return (0, [])

  def Deserialize(self, value, data, handles):
    return None


TYPE_NONE = NoneType()

TYPE_BOOL = BooleanType()

TYPE_INT8 = IntegerType('b')
TYPE_INT16 = IntegerType('h')
TYPE_INT32 = IntegerType('i')
TYPE_INT64 = IntegerType('q')

TYPE_UINT8 = IntegerType('B')
TYPE_UINT16 = IntegerType('H')
TYPE_UINT32 = IntegerType('I')
TYPE_UINT64 = IntegerType('Q')

TYPE_FLOAT = FloatType('f')
TYPE_DOUBLE = FloatType('d')

TYPE_STRING = StringType()
TYPE_NULLABLE_STRING = StringType(True)

TYPE_HANDLE = HandleType()
TYPE_NULLABLE_HANDLE = HandleType(True)


class FieldDescriptor(object):
  """Describes a field in a generated struct."""

  def __init__(self, name, field_type, field_number, default_value=None):
    self.name = name
    self.field_type = field_type
    self.field_number = field_number
    self._default_value = default_value

  def GetDefaultValue(self):
    return self.field_type.GetDefaultValue(self._default_value)


class FieldGroup(object):
  """
  Describe a list of field in the generated struct that must be
  serialized/deserialized together.
  """
  def __init__(self, descriptors):
    self.descriptors = descriptors

  def GetDescriptors(self):
    return self.descriptors

  def GetTypeCode(self):
    raise NotImplementedError()

  def GetByteSize(self):
    raise NotImplementedError()

  def GetVersion(self):
    raise NotImplementedError()

  def Serialize(self, obj, data_offset, data, handle_offset):
    raise NotImplementedError()

  def Deserialize(self, value, data, handles):
    raise NotImplementedError()


class SingleFieldGroup(FieldGroup, FieldDescriptor):
  """A FieldGroup that contains a single FieldDescriptor."""

  def __init__(self, name, field_type, field_number, default_value=None):
    FieldDescriptor.__init__(
        self, name, field_type, field_number, default_value)
    FieldGroup.__init__(self, [self])

  def GetTypeCode(self):
    return self.field_type.GetTypeCode()

  def GetByteSize(self):
    return self.field_type.GetByteSize()

  def GetVersion(self):
    return self.field_number

  def Serialize(self, obj, data_offset, data, handle_offset):
    value = getattr(obj, self.name)
    return self.field_type.Serialize(value, data_offset, data, handle_offset)

  def Deserialize(self, value, data, handles):
    entity = self.field_type.Deserialize(value, data, handles)
    return { self.name: entity }


class BooleanGroup(FieldGroup):
  """A FieldGroup to pack booleans."""
  def __init__(self, descriptors):
    FieldGroup.__init__(self, descriptors)
    self.version = min([descriptor.field_number  for descriptor in descriptors])

  def GetTypeCode(self):
    return 'B'

  def GetByteSize(self):
    return 1

  def GetVersion(self):
    return self.version

  def Serialize(self, obj, data_offset, data, handle_offset):
    value = _ConvertBooleansToByte(
        [getattr(obj, field.name) for field in self.GetDescriptors()])
    return (value, [])

  def Deserialize(self, value, data, handles):
    values =  itertools.izip_longest([x.name for x in self.descriptors],
                                      _ConvertByteToBooleans(value),
                                     fillvalue=False)
    return dict(values)


def _SerializeNativeArray(value, data_offset, data, length):
  data_size = len(data)
  data.extend(bytearray(serialization.HEADER_STRUCT.size))
  data.extend(buffer(value))
  data_length = len(data) - data_size
  data.extend(bytearray(serialization.NeededPaddingForAlignment(data_length)))
  serialization.HEADER_STRUCT.pack_into(data, data_size, data_length, length)
  return (data_offset, [])


def _ConvertBooleansToByte(booleans):
  """Pack a list of booleans into an integer."""
  return reduce(lambda x, y: x * 2 + y, reversed(booleans), 0)


def _ConvertByteToBooleans(value, min_size=0):
  "Unpack an integer into a list of booleans."""
  res = []
  while value:
    res.append(bool(value&1))
    value = value / 2
  res.extend([False] * (min_size - len(res)))
  return res
