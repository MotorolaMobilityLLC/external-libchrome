// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.mojo.bindings.Struct.DataHeader;
import org.chromium.mojo.system.AsyncWaiter;
import org.chromium.mojo.system.Handle;

/**
 * Helper functions.
 */
public class BindingsHelper {
    /**
     * Alignment in bytes for mojo serialization.
     */
    public static final int ALIGNMENT = 8;

    /**
     * The size, in bytes, of a serialized handle. A handle is serialized as an int representing the
     * offset of the handle in the list of handles.
     */
    public static final int SERIALIZED_HANDLE_SIZE = 4;

    /**
     * The size, in bytes, of a serialized pointer. A pointer is serializaed as an unsigned long
     * representing the offset from its position to the pointed elemnt.
     */
    public static final int POINTER_SIZE = 8;

    /**
     * The header for a serialized map element.
     */
    public static final DataHeader MAP_STRUCT_HEADER = new DataHeader(24, 2);

    /**
     * The value used for the expected length of a non-fixed size array.
     */
    public static final int UNSPECIFIED_ARRAY_LENGTH = -1;

    /**
     * Passed as |arrayNullability| when neither the array nor its elements are nullable.
     */
    public static final int NOTHING_NULLABLE = 0;

    /**
     * "Array bit" of |arrayNullability| is set iff the array itself is nullable.
     */
    public static final int ARRAY_NULLABLE = (1 << 0);

    /**
     * "Element bit" of |arrayNullability| is set iff the array elements are nullable.
     */
    public static final int ELEMENT_NULLABLE = (1 << 1);

    public static boolean isArrayNullable(int arrayNullability) {
        return (arrayNullability & ARRAY_NULLABLE) > 0;
    }

    public static boolean isElementNullable(int arrayNullability) {
        return (arrayNullability & ELEMENT_NULLABLE) > 0;
    }

    /**
     * Align |size| on {@link BindingsHelper#ALIGNMENT}.
     */
    public static int align(int size) {
        return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    }

    /**
     * Align |size| on {@link BindingsHelper#ALIGNMENT}.
     */
    public static long align(long size) {
        return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    }

    /**
     * Compute the size in bytes of the given string encoded as utf8.
     */
    public static int utf8StringSizeInBytes(String s) {
        int res = 0;
        for (int i = 0; i < s.length(); ++i) {
            char c = s.charAt(i);
            int codepoint = c;
            if (isSurrogate(c)) {
                i++;
                char c2 = s.charAt(i);
                codepoint = Character.toCodePoint(c, c2);
            }
            res += 1;
            if (codepoint > 0x7f) {
                res += 1;
                if (codepoint > 0x7ff) {
                    res += 1;
                    if (codepoint > 0xffff) {
                        res += 1;
                        if (codepoint > 0x1fffff) {
                            res += 1;
                            if (codepoint > 0x3ffffff) {
                                res += 1;
                            }
                        }
                    }
                }
            }
        }
        return res;
    }

    /**
     * Determines if the given {@code char} value is a Unicode <i>surrogate code unit</i>. See
     * {@link Character#isSurrogate}. Extracting here because the method only exists at API level
     * 19.
     */
    private static boolean isSurrogate(char c) {
        return c >= Character.MIN_SURROGATE && c < (Character.MAX_SURROGATE + 1);
    }

    /**
     * Returns an {@link AsyncWaiter} to use with the given handle, or <code>null</code> if none if
     * available.
     */
    static AsyncWaiter getDefaultAsyncWaiterForHandle(Handle handle) {
        if (handle.getCore() != null) {
            return handle.getCore().getDefaultAsyncWaiter();
        } else {
            return null;
        }
    }

}
