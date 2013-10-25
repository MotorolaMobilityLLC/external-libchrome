# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import mojom
import mojom_pack
import mojom_test
import sys

EXPECT_EQ = mojom_test.EXPECT_EQ
EXPECT_TRUE = mojom_test.EXPECT_TRUE
RunTest = mojom_test.RunTest


def TestOrdinalOrder():
  errors = 0
  struct = mojom.Struct('test')
  struct.AddField('testfield1', mojom.INT32, 2)
  struct.AddField('testfield2', mojom.INT32, 1)
  ps = mojom_pack.PackedStruct(struct)

  errors += EXPECT_EQ(2, len(ps.packed_fields))
  errors += EXPECT_EQ('testfield2', ps.packed_fields[0].field.name)
  errors += EXPECT_EQ('testfield1', ps.packed_fields[1].field.name)

  return errors

def TestZeroFields():
  errors = 0
  struct = mojom.Struct('test')
  ps = mojom_pack.PackedStruct(struct)
  errors += EXPECT_EQ(0, len(ps.packed_fields))
  return errors


def TestOneField():
  errors = 0
  struct = mojom.Struct('test')
  struct.AddField('testfield1', mojom.INT8)
  ps = mojom_pack.PackedStruct(struct)
  errors += EXPECT_EQ(1, len(ps.packed_fields))
  return errors

# Pass three tuples.
# |kinds| is a sequence of mojom.Kinds that specify the fields that are to
# be created.
# |fields| is the expected order of the resulting fields, with the integer
# "1" first.
# |offsets| is the expected order of offsets, with the integer "0" first.
def TestSequence(kinds, fields, offsets):
  errors = 0
  struct = mojom.Struct('test')
  index = 1
  for kind in kinds:
    struct.AddField("%d" % index, kind)
    index += 1
  ps = mojom_pack.PackedStruct(struct)
  num_fields = len(ps.packed_fields)
  errors += EXPECT_EQ(len(kinds), num_fields)
  for i in xrange(num_fields):
    EXPECT_EQ("%d" % fields[i], ps.packed_fields[i].field.name)
    EXPECT_EQ(offsets[i], ps.packed_fields[i].offset)

  return errors


def TestPaddingPackedInOrder():
  return TestSequence(
      (mojom.INT8, mojom.UINT8, mojom.INT32),
      (1, 2, 3),
      (0, 1, 4))


def TestPaddingPackedOutOfOrder():
  return TestSequence(
      (mojom.INT8, mojom.INT32, mojom.UINT8),
      (1, 3, 2),
      (0, 1, 4))


def TestPaddingPackedOverflow():
  kinds = (mojom.INT8, mojom.INT32, mojom.INT16, mojom.INT8, mojom.INT8)
  # 2 bytes should be packed together first, followed by short, then by int.
  fields = (1, 4, 3, 2, 5)
  offsets = (0, 1, 2, 4, 8)
  return TestSequence(kinds, fields, offsets)


def TestAllTypes():
  struct = mojom.Struct('test')
  array = mojom.Array()
  return TestSequence(
      (mojom.BOOL, mojom.INT8, mojom.STRING, mojom.UINT8,
       mojom.INT16, mojom.DOUBLE, mojom.UINT16,
       mojom.INT32, mojom.UINT32, mojom.INT64,
       mojom.FLOAT, mojom.STRING, mojom.HANDLE,
       mojom.UINT64, mojom.Struct('test'), mojom.Array()),
      (1, 2, 4, 5, 7, 3, 6,  8,  9,  10, 11, 13, 12, 14, 15, 16, 17),
      (0, 1, 2, 4, 6, 8, 16, 24, 28, 32, 40, 44, 48, 56, 64, 72, 80))


def TestPaddingPackedOutOfOrderByOrdinal():
  errors = 0
  struct = mojom.Struct('test')
  struct.AddField('testfield1', mojom.INT8)
  struct.AddField('testfield3', mojom.UINT8, 3)
  struct.AddField('testfield2', mojom.INT32, 2)
  ps = mojom_pack.PackedStruct(struct)
  errors += EXPECT_EQ(3, len(ps.packed_fields))

  # Second byte should be packed in behind first, altering order.
  errors += EXPECT_EQ('testfield1', ps.packed_fields[0].field.name)
  errors += EXPECT_EQ('testfield3', ps.packed_fields[1].field.name)
  errors += EXPECT_EQ('testfield2', ps.packed_fields[2].field.name)

  # Second byte should be packed with first.
  errors += EXPECT_EQ(0, ps.packed_fields[0].offset)
  errors += EXPECT_EQ(1, ps.packed_fields[1].offset)
  errors += EXPECT_EQ(4, ps.packed_fields[2].offset)

  return errors


def TestBools():
  errors = 0
  struct = mojom.Struct('test')
  struct.AddField('bit0', mojom.BOOL)
  struct.AddField('bit1', mojom.BOOL)
  struct.AddField('int', mojom.INT32)
  struct.AddField('bit2', mojom.BOOL)
  struct.AddField('bit3', mojom.BOOL)
  struct.AddField('bit4', mojom.BOOL)
  struct.AddField('bit5', mojom.BOOL)
  struct.AddField('bit6', mojom.BOOL)
  struct.AddField('bit7', mojom.BOOL)
  struct.AddField('bit8', mojom.BOOL)
  ps = mojom_pack.PackedStruct(struct)
  errors += EXPECT_EQ(10, len(ps.packed_fields))

  # First 8 bits packed together.
  for i in xrange(8):
    pf = ps.packed_fields[i]
    errors += EXPECT_EQ(0, pf.offset)
    errors += EXPECT_EQ("bit%d" % i, pf.field.name)
    errors += EXPECT_EQ(i, pf.bit)

  # Ninth bit goes into second byte.
  errors += EXPECT_EQ("bit8", ps.packed_fields[8].field.name)
  errors += EXPECT_EQ(1, ps.packed_fields[8].offset)
  errors += EXPECT_EQ(0, ps.packed_fields[8].bit)

  # int comes last.
  errors += EXPECT_EQ("int", ps.packed_fields[9].field.name)
  errors += EXPECT_EQ(4, ps.packed_fields[9].offset)

  return errors


def Main(args):
  errors = 0
  errors += RunTest(TestZeroFields)
  errors += RunTest(TestOneField)
  errors += RunTest(TestPaddingPackedInOrder)
  errors += RunTest(TestPaddingPackedOutOfOrder)
  errors += RunTest(TestPaddingPackedOverflow)
  errors += RunTest(TestAllTypes)
  errors += RunTest(TestPaddingPackedOutOfOrderByOrdinal)
  errors += RunTest(TestBools)

  return errors


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
