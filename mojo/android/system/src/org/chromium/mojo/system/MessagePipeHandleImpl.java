// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.system;

import java.nio.ByteBuffer;
import java.util.List;

/**
 * Implementation of {@link MessagePipeHandle}.
 */
class MessagePipeHandleImpl extends HandleBase implements MessagePipeHandle {

    /**
     * @see HandleBase#HandleBase(CoreImpl, int)
     */
    MessagePipeHandleImpl(CoreImpl core, int mojoHandle) {
        super(core, mojoHandle);
    }

    /**
     * @see HandleBase#HandleBase(HandleBase)
     */
    MessagePipeHandleImpl(UntypedHandleImpl handle) {
        super(handle);
    }

    /**
     * @see MessagePipeHandle#writeMessage(ByteBuffer, List, WriteFlags)
     */
    @Override
    public void writeMessage(ByteBuffer bytes, List<? extends Handle> handles, WriteFlags flags) {
        mCore.writeMessage(this, bytes, handles, flags);
    }

    /**
     * @see MessagePipeHandle#readMessage(ByteBuffer, int, ReadFlags)
     */
    @Override
    public ReadMessageResult readMessage(ByteBuffer bytes,
            int maxNumberOfHandles,
            ReadFlags flags) {
        return mCore.readMessage(this, bytes, maxNumberOfHandles, flags);
    }

}
