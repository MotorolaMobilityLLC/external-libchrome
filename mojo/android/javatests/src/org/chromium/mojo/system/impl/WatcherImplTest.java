// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.system.impl;

import android.support.test.filters.SmallTest;

import org.chromium.mojo.MojoTestCase;
import org.chromium.mojo.system.Core;
import org.chromium.mojo.system.Handle;
import org.chromium.mojo.system.InvalidHandle;
import org.chromium.mojo.system.MessagePipeHandle;
import org.chromium.mojo.system.MojoException;
import org.chromium.mojo.system.MojoResult;
import org.chromium.mojo.system.Pair;
import org.chromium.mojo.system.Watcher;
import org.chromium.mojo.system.Watcher.Callback;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;

/**
 * Testing the Watcher.
 */
public class WatcherImplTest extends MojoTestCase {
    private List<Handle> mHandlesToClose = new ArrayList<Handle>();
    private Watcher mWatcher;
    private Core mCore;

    /**
     * @see MojoTestCase#setUp()
     */
    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mWatcher = new WatcherImpl();
        mCore = CoreImpl.getInstance();
    }

    /**
     * @see MojoTestCase#tearDown()
     */
    @Override
    protected void tearDown() throws Exception {
        mWatcher.destroy();
        MojoException toThrow = null;
        for (Handle handle : mHandlesToClose) {
            try {
                handle.close();
            } catch (MojoException e) {
                if (toThrow == null) {
                    toThrow = e;
                }
            }
        }
        if (toThrow != null) {
            throw toThrow;
        }
        super.tearDown();
    }

    private void addHandlePairToClose(Pair<? extends Handle, ? extends Handle> handles) {
        mHandlesToClose.add(handles.first);
        mHandlesToClose.add(handles.second);
    }

    private static class WatcherResult implements Callback {
        private int mResult = Integer.MIN_VALUE;
        private MessagePipeHandle mReadPipe;

        /**
         * @param readPipe A MessagePipeHandle to read from when onResult triggers success.
         */
        public WatcherResult(MessagePipeHandle readPipe) {
            mReadPipe = readPipe;
        }
        public WatcherResult() {
            this(null);
        }

        /**
         * @see Callback#onResult(int)
         */
        @Override
        public void onResult(int result) {
            this.mResult = result;

            if (result == MojoResult.OK && mReadPipe != null) {
                mReadPipe.readMessage(MessagePipeHandle.ReadFlags.NONE);
            }
        }

        /**
         * @return the result
         */
        public int getResult() {
            return mResult;
        }
    }

    /**
     * Testing {@link Watcher} implementation.
     */
    @SmallTest
    public void testCorrectResult() {
        // Checking a correct result.
        Pair<MessagePipeHandle, MessagePipeHandle> handles = mCore.createMessagePipe(null);
        addHandlePairToClose(handles);
        final WatcherResult watcherResult = new WatcherResult(handles.first);
        assertEquals(Integer.MIN_VALUE, watcherResult.getResult());

        mWatcher.start(handles.first, Core.HandleSignals.READABLE, watcherResult);
        assertEquals(Integer.MIN_VALUE, watcherResult.getResult());

        handles.second.writeMessage(
                ByteBuffer.allocateDirect(1), null, MessagePipeHandle.WriteFlags.NONE);
        runLoopUntilIdle();
        assertEquals(MojoResult.OK, watcherResult.getResult());
    }

    /**
     * Testing {@link Watcher} implementation.
     */
    @SmallTest
    public void testClosingPeerHandle() {
        // Closing the peer handle.
        Pair<MessagePipeHandle, MessagePipeHandle> handles = mCore.createMessagePipe(null);
        addHandlePairToClose(handles);

        final WatcherResult watcherResult = new WatcherResult();
        assertEquals(Integer.MIN_VALUE, watcherResult.getResult());

        mWatcher.start(handles.first, Core.HandleSignals.READABLE, watcherResult);
        assertEquals(Integer.MIN_VALUE, watcherResult.getResult());

        runLoopUntilIdle();
        assertEquals(Integer.MIN_VALUE, watcherResult.getResult());

        handles.second.close();
        runLoopUntilIdle();
        assertEquals(MojoResult.FAILED_PRECONDITION, watcherResult.getResult());
    }

    /**
     * Testing {@link Watcher} implementation.
     */
    @SmallTest
    public void testClosingWatchedHandle() {
        // Closing the peer handle.
        Pair<MessagePipeHandle, MessagePipeHandle> handles = mCore.createMessagePipe(null);
        addHandlePairToClose(handles);

        final WatcherResult watcherResult = new WatcherResult();
        assertEquals(Integer.MIN_VALUE, watcherResult.getResult());

        mWatcher.start(handles.first, Core.HandleSignals.READABLE, watcherResult);
        assertEquals(Integer.MIN_VALUE, watcherResult.getResult());

        runLoopUntilIdle();
        assertEquals(Integer.MIN_VALUE, watcherResult.getResult());

        handles.first.close();
        runLoopUntilIdle();
        assertEquals(MojoResult.CANCELLED, watcherResult.getResult());
    }

    /**
     * Testing {@link Watcher} implementation.
     */
    @SmallTest
    public void testInvalidHandle() {
        // Closing the peer handle.
        Pair<MessagePipeHandle, MessagePipeHandle> handles = mCore.createMessagePipe(null);
        addHandlePairToClose(handles);

        final WatcherResult watcherResult = new WatcherResult();
        assertEquals(Integer.MIN_VALUE, watcherResult.getResult());

        handles.first.close();
        assertEquals(MojoResult.INVALID_ARGUMENT,
                mWatcher.start(handles.first, Core.HandleSignals.READABLE, watcherResult));
        assertEquals(Integer.MIN_VALUE, watcherResult.getResult());

        runLoopUntilIdle();
        assertEquals(Integer.MIN_VALUE, watcherResult.getResult());
    }

    /**
     * Testing {@link Watcher} implementation.
     */
    @SmallTest
    public void testDefaultInvalidHandle() {
        final WatcherResult watcherResult = new WatcherResult();
        assertEquals(Integer.MIN_VALUE, watcherResult.getResult());

        assertEquals(MojoResult.INVALID_ARGUMENT,
                mWatcher.start(InvalidHandle.INSTANCE, Core.HandleSignals.READABLE, watcherResult));
        assertEquals(Integer.MIN_VALUE, watcherResult.getResult());

        runLoopUntilIdle();
        assertEquals(Integer.MIN_VALUE, watcherResult.getResult());
    }

    /**
     * Testing {@link Watcher} implementation.
     */
    @SmallTest
    public void testCancel() {
        // Closing the peer handle.
        Pair<MessagePipeHandle, MessagePipeHandle> handles = mCore.createMessagePipe(null);
        addHandlePairToClose(handles);

        final WatcherResult watcherResult = new WatcherResult();
        assertEquals(Integer.MIN_VALUE, watcherResult.getResult());

        mWatcher.start(handles.first, Core.HandleSignals.READABLE, watcherResult);
        assertEquals(Integer.MIN_VALUE, watcherResult.getResult());

        runLoopUntilIdle();
        assertEquals(Integer.MIN_VALUE, watcherResult.getResult());

        mWatcher.cancel();
        runLoopUntilIdle();
        assertEquals(Integer.MIN_VALUE, watcherResult.getResult());

        handles.second.writeMessage(
                ByteBuffer.allocateDirect(1), null, MessagePipeHandle.WriteFlags.NONE);
        runLoopUntilIdle();
        assertEquals(Integer.MIN_VALUE, watcherResult.getResult());
    }

    /**
     * Testing {@link Watcher} implementation.
     */
    @SmallTest
    public void testImmediateCancelOnInvalidHandle() {
        // Closing the peer handle.
        Pair<MessagePipeHandle, MessagePipeHandle> handles = mCore.createMessagePipe(null);
        addHandlePairToClose(handles);

        final WatcherResult watcherResult = new WatcherResult();
        handles.first.close();
        assertEquals(Integer.MIN_VALUE, watcherResult.getResult());

        mWatcher.start(handles.first, Core.HandleSignals.READABLE, watcherResult);
        assertEquals(Integer.MIN_VALUE, watcherResult.getResult());
        mWatcher.cancel();

        runLoopUntilIdle();
        assertEquals(Integer.MIN_VALUE, watcherResult.getResult());
    }
}
