// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.system.impl;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.mojo.MojoTestCase;
import org.chromium.mojo.system.AsyncWaiter;
import org.chromium.mojo.system.AsyncWaiter.Callback;
import org.chromium.mojo.system.AsyncWaiter.Cancellable;
import org.chromium.mojo.system.Core;
import org.chromium.mojo.system.Core.WaitManyResult;
import org.chromium.mojo.system.DataPipe;
import org.chromium.mojo.system.Handle;
import org.chromium.mojo.system.InvalidHandle;
import org.chromium.mojo.system.MessagePipeHandle;
import org.chromium.mojo.system.MojoException;
import org.chromium.mojo.system.MojoResult;
import org.chromium.mojo.system.Pair;
import org.chromium.mojo.system.SharedBufferHandle;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Random;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

/**
 * Testing the core API.
 */
public class CoreImplTest extends MojoTestCase {

    private static final long RUN_LOOP_TIMEOUT_MS = 5;

    private static final ScheduledExecutorService WORKER =
            Executors.newSingleThreadScheduledExecutor();

    /**
     * Runnable that will close the given handle.
     */
    private static class CloseHandle implements Runnable {
        private Handle mHandle;

        CloseHandle(Handle handle) {
            mHandle = handle;
        }

        @Override
        public void run() {
            mHandle.close();
        }
    }

    private static void checkSendingMessage(MessagePipeHandle in, MessagePipeHandle out) {
        Random random = new Random();

        // Writing a random 8 bytes message.
        byte[] bytes = new byte[8];
        random.nextBytes(bytes);
        ByteBuffer buffer = ByteBuffer.allocateDirect(bytes.length);
        buffer.put(bytes);
        in.writeMessage(buffer, null, MessagePipeHandle.WriteFlags.NONE);

        // Try to read into a small buffer.
        ByteBuffer receiveBuffer = ByteBuffer.allocateDirect(bytes.length / 2);
        MessagePipeHandle.ReadMessageResult result = out.readMessage(
                receiveBuffer, 0, MessagePipeHandle.ReadFlags.NONE);
        assertEquals(MojoResult.RESOURCE_EXHAUSTED, result.getMojoResult());
        assertEquals(bytes.length, result.getMessageSize());
        assertEquals(0, result.getHandlesCount());

        // Read into a correct buffer.
        receiveBuffer = ByteBuffer.allocateDirect(bytes.length);
        result = out.readMessage(receiveBuffer, 0, MessagePipeHandle.ReadFlags.NONE);
        assertEquals(MojoResult.OK, result.getMojoResult());
        assertEquals(bytes.length, result.getMessageSize());
        assertEquals(0, result.getHandlesCount());
        assertEquals(0, receiveBuffer.position());
        assertEquals(result.getMessageSize(), receiveBuffer.limit());
        byte[] receivedBytes = new byte[result.getMessageSize()];
        receiveBuffer.get(receivedBytes);
        assertTrue(Arrays.equals(bytes, receivedBytes));

    }

    /**
     * Testing {@link Core#waitMany(List, long)}.
     */
    @SmallTest
    public void testWaitMany() {
        Core core = CoreImpl.getInstance();
        Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe();

        try {
            List<Pair<Handle, Core.WaitFlags>> handlesToWaitOn = new ArrayList<
                    Pair<Handle, Core.WaitFlags>>();

            handlesToWaitOn.add(
                    new Pair<Handle, Core.WaitFlags>(handles.second, Core.WaitFlags.READABLE));
            handlesToWaitOn.add(
                    new Pair<Handle, Core.WaitFlags>(handles.first, Core.WaitFlags.WRITABLE));
            WaitManyResult result = core.waitMany(handlesToWaitOn, 0);
            assertEquals(MojoResult.OK, result.getMojoResult());
            assertEquals(1, result.getHandleIndex());

            handlesToWaitOn.clear();
            handlesToWaitOn.add(
                    new Pair<Handle, Core.WaitFlags>(handles.first, Core.WaitFlags.WRITABLE));
            handlesToWaitOn.add(
                    new Pair<Handle, Core.WaitFlags>(handles.second, Core.WaitFlags.READABLE));
            result = core.waitMany(handlesToWaitOn, 0);
            assertEquals(MojoResult.OK, result.getMojoResult());
            assertEquals(0, result.getHandleIndex());
        } finally {
            handles.first.close();
            handles.second.close();
        }
    }

    /**
     * Testing {@link MessagePipeHandle}.
     */
    @SmallTest
    public void testMessagePipeEmpty() {
        Core core = CoreImpl.getInstance();
        Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe();

        try {
            // Testing wait.
            assertEquals(MojoResult.OK, handles.first.wait(Core.WaitFlags.all(), 0));
            assertEquals(MojoResult.OK, handles.first.wait(Core.WaitFlags.WRITABLE, 0));
            assertEquals(MojoResult.DEADLINE_EXCEEDED,
                    handles.first.wait(Core.WaitFlags.READABLE, 0));

            // Testing read on an empty pipe.
            MessagePipeHandle.ReadMessageResult result = handles.first.readMessage(null, 0,
                    MessagePipeHandle.ReadFlags.NONE);
            assertEquals(MojoResult.SHOULD_WAIT, result.getMojoResult());

            // Closing a pipe while waiting.
            WORKER.schedule(new CloseHandle(handles.first), 10, TimeUnit.MILLISECONDS);
            assertEquals(MojoResult.CANCELLED,
                    handles.first.wait(Core.WaitFlags.READABLE, 1000000L));
        } finally {
            handles.first.close();
            handles.second.close();
        }

        handles = core.createMessagePipe();

        try {
            // Closing the other pipe while waiting.
            WORKER.schedule(new CloseHandle(handles.first), 10, TimeUnit.MILLISECONDS);
            assertEquals(MojoResult.FAILED_PRECONDITION,
                    handles.second.wait(Core.WaitFlags.READABLE, 1000000L));

            // Waiting on a closed pipe.
            assertEquals(MojoResult.FAILED_PRECONDITION,
                    handles.second.wait(Core.WaitFlags.READABLE, 0));
            assertEquals(MojoResult.FAILED_PRECONDITION,
                    handles.second.wait(Core.WaitFlags.WRITABLE, 0));
        } finally {
            handles.first.close();
            handles.second.close();
        }

    }

    /**
     * Testing {@link MessagePipeHandle}.
     */
    @SmallTest
    public void testMessagePipeSend() {
        Core core = CoreImpl.getInstance();
        Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe();

        try {
            checkSendingMessage(handles.first, handles.second);
            checkSendingMessage(handles.second, handles.first);
        } finally {
            handles.first.close();
            handles.second.close();
        }
    }

    /**
     * Testing {@link MessagePipeHandle}.
     */
    @SmallTest
    public void testMessagePipeReceiveOnSmallBuffer() {
        Random random = new Random();
        Core core = CoreImpl.getInstance();
        Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe();

        try {
            // Writing a random 8 bytes message.
            byte[] bytes = new byte[8];
            random.nextBytes(bytes);
            ByteBuffer buffer = ByteBuffer.allocateDirect(bytes.length);
            buffer.put(bytes);
            handles.first.writeMessage(buffer, null, MessagePipeHandle.WriteFlags.NONE);

            ByteBuffer receiveBuffer = ByteBuffer.allocateDirect(1);
            MessagePipeHandle.ReadMessageResult result = handles.second
                    .readMessage(receiveBuffer, 0, MessagePipeHandle.ReadFlags.NONE);
            assertEquals(MojoResult.RESOURCE_EXHAUSTED, result.getMojoResult());
            assertEquals(bytes.length, result.getMessageSize());
            assertEquals(0, result.getHandlesCount());
        } finally {
            handles.first.close();
            handles.second.close();
        }
    }

    /**
     * Testing {@link MessagePipeHandle}.
     */
    @SmallTest
    public void testMessagePipeSendHandles() {
        Core core = CoreImpl.getInstance();
        Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe();
        Pair<MessagePipeHandle, MessagePipeHandle> handlesToShare = core.createMessagePipe();

        try {
            handles.first.writeMessage(null,
                    Collections.<Handle> singletonList(handlesToShare.second),
                    MessagePipeHandle.WriteFlags.NONE);
            assertFalse(handlesToShare.second.isValid());
            MessagePipeHandle.ReadMessageResult readMessageResult =
                    handles.second.readMessage(null, 1, MessagePipeHandle.ReadFlags.NONE);
            assertEquals(1, readMessageResult.getHandlesCount());
            MessagePipeHandle newHandle = readMessageResult.getHandles().get(0)
                    .toMessagePipeHandle();
            try {
                assertTrue(newHandle.isValid());
                checkSendingMessage(handlesToShare.first, newHandle);
                checkSendingMessage(newHandle, handlesToShare.first);
            } finally {
                newHandle.close();
            }
        } finally {
            handles.first.close();
            handles.second.close();
            handlesToShare.first.close();
            handlesToShare.second.close();
        }
    }

    private static void createAndCloseDataPipe(DataPipe.CreateOptions options) {
        Core core = CoreImpl.getInstance();
        Pair<DataPipe.ProducerHandle, DataPipe.ConsumerHandle> handles = core.createDataPipe(
                options);
        handles.first.close();
        handles.second.close();
    }

    /**
     * Testing {@link DataPipe}.
     */
    @SmallTest
    public void testDataPipeCreation() {
        // Create datapipe with null options.
        createAndCloseDataPipe(null);
        DataPipe.CreateOptions options = new DataPipe.CreateOptions();
        // Create datapipe with element size set.
        options.setElementNumBytes(24);
        createAndCloseDataPipe(options);
        // Create datapipe with a flag set.
        options.getFlags().setMayDiscard(true);
        createAndCloseDataPipe(options);
        // Create datapipe with capacity set.
        options.setCapacityNumBytes(1024 * options.getElementNumBytes());
        createAndCloseDataPipe(options);
    }

    /**
     * Testing {@link DataPipe}.
     */
    @SmallTest
    public void testDataPipeSend() {
        Core core = CoreImpl.getInstance();
        Random random = new Random();

        Pair<DataPipe.ProducerHandle, DataPipe.ConsumerHandle> handles = core.createDataPipe(null);
        try {
            // Writing a random 8 bytes message.
            byte[] bytes = new byte[8];
            random.nextBytes(bytes);
            ByteBuffer buffer = ByteBuffer.allocateDirect(bytes.length);
            buffer.put(bytes);
            int result = handles.first.writeData(buffer, DataPipe.WriteFlags.NONE);
            assertEquals(bytes.length, result);

            // Query number of bytes available.
            result = handles.second.readData(null, DataPipe.ReadFlags.none().query(true));
            assertEquals(bytes.length, result);

            // Read into a buffer.
            ByteBuffer receiveBuffer = ByteBuffer.allocateDirect(bytes.length);
            result = handles.second.readData(receiveBuffer, DataPipe.ReadFlags.NONE);
            assertEquals(bytes.length, result);
            assertEquals(0, receiveBuffer.position());
            assertEquals(bytes.length, receiveBuffer.limit());
            byte[] receivedBytes = new byte[bytes.length];
            receiveBuffer.get(receivedBytes);
            assertTrue(Arrays.equals(bytes, receivedBytes));
        } finally {
            handles.first.close();
            handles.second.close();
        }
    }

    /**
     * Testing {@link DataPipe}.
     */
    @SmallTest
    public void testDataPipeTwoPhaseSend() {
        Random random = new Random();
        Core core = CoreImpl.getInstance();
        Pair<DataPipe.ProducerHandle, DataPipe.ConsumerHandle> handles = core.createDataPipe(null);

        try {
            // Writing a random 8 bytes message.
            byte[] bytes = new byte[8];
            random.nextBytes(bytes);
            ByteBuffer buffer = handles.first.beginWriteData(bytes.length,
                    DataPipe.WriteFlags.NONE);
            assertTrue(buffer.capacity() >= bytes.length);
            buffer.put(bytes);
            handles.first.endWriteData(bytes.length);

            // Read into a buffer.
            ByteBuffer receiveBuffer = handles.second.beginReadData(bytes.length,
                    DataPipe.ReadFlags.NONE);
            assertEquals(0, receiveBuffer.position());
            assertEquals(bytes.length, receiveBuffer.limit());
            byte[] receivedBytes = new byte[bytes.length];
            receiveBuffer.get(receivedBytes);
            assertTrue(Arrays.equals(bytes, receivedBytes));
            handles.second.endReadData(bytes.length);
        } finally {
            handles.first.close();
            handles.second.close();
        }
    }

    /**
     * Testing {@link DataPipe}.
     */
    @SmallTest
    public void testDataPipeDiscard() {
        Random random = new Random();
        Core core = CoreImpl.getInstance();
        Pair<DataPipe.ProducerHandle, DataPipe.ConsumerHandle> handles = core.createDataPipe(null);

        try {
            // Writing a random 8 bytes message.
            byte[] bytes = new byte[8];
            random.nextBytes(bytes);
            ByteBuffer buffer = ByteBuffer.allocateDirect(bytes.length);
            buffer.put(bytes);
            int result = handles.first.writeData(buffer, DataPipe.WriteFlags.NONE);
            assertEquals(bytes.length, result);

            // Discard bytes.
            final int nbBytesToDiscard = 4;
            assertEquals(nbBytesToDiscard,
                    handles.second.discardData(nbBytesToDiscard, DataPipe.ReadFlags.NONE));

            // Read into a buffer.
            ByteBuffer receiveBuffer = ByteBuffer.allocateDirect(bytes.length - nbBytesToDiscard);
            result = handles.second.readData(receiveBuffer, DataPipe.ReadFlags.NONE);
            assertEquals(bytes.length - nbBytesToDiscard, result);
            assertEquals(0, receiveBuffer.position());
            assertEquals(bytes.length - nbBytesToDiscard, receiveBuffer.limit());
            byte[] receivedBytes = new byte[bytes.length - nbBytesToDiscard];
            receiveBuffer.get(receivedBytes);
            assertTrue(Arrays.equals(Arrays.copyOfRange(bytes, nbBytesToDiscard, bytes.length),
                    receivedBytes));
        } finally {
            handles.first.close();
            handles.second.close();
        }
    }

    /**
     * Testing {@link SharedBufferHandle}.
     */
    @SmallTest
    public void testSharedBufferCreation() {
        Core core = CoreImpl.getInstance();
        // Test creation with empty options.
        core.createSharedBuffer(null, 8).close();
        // Test creation with default options.
        core.createSharedBuffer(new SharedBufferHandle.CreateOptions(), 8);
    }

    /**
     * Testing {@link SharedBufferHandle}.
     */
    @SmallTest
    public void testSharedBufferDuplication() {
        Core core = CoreImpl.getInstance();
        SharedBufferHandle handle = core.createSharedBuffer(null, 8);
        try {
            // Test duplication with empty options.
            handle.duplicate(null).close();
            // Test creation with default options.
            handle.duplicate(new SharedBufferHandle.DuplicateOptions()).close();
        } finally {
            handle.close();
        }
    }

    /**
     * Testing {@link SharedBufferHandle}.
     */
    @SmallTest
    public void testSharedBufferSending() {
        Random random = new Random();
        Core core = CoreImpl.getInstance();
        SharedBufferHandle handle = core.createSharedBuffer(null, 8);
        SharedBufferHandle newHandle = handle.duplicate(null);

        try {
            ByteBuffer buffer1 = handle.map(0, 8, SharedBufferHandle.MapFlags.NONE);
            assertEquals(8, buffer1.capacity());
            ByteBuffer buffer2 = newHandle.map(0, 8, SharedBufferHandle.MapFlags.NONE);
            assertEquals(8, buffer2.capacity());

            byte[] bytes = new byte[8];
            random.nextBytes(bytes);
            buffer1.put(bytes);

            byte[] receivedBytes = new byte[bytes.length];
            buffer2.get(receivedBytes);

            assertTrue(Arrays.equals(bytes, receivedBytes));

            handle.unmap(buffer1);
            newHandle.unmap(buffer2);
        } finally {
            handle.close();
            newHandle.close();
        }
    }

    /**
     * Testing that invalid handle can be used with this implementation.
     */
    @SmallTest
    public void testInvalidHandle() {
        Core core = CoreImpl.getInstance();
        Handle handle = new InvalidHandle();

        // Checking wait.
        boolean exception = false;
        try {
            core.wait(handle, Core.WaitFlags.all(), 0);
        } catch (MojoException e) {
            assertEquals(MojoResult.INVALID_ARGUMENT, e.getMojoResult());
            exception = true;
        }
        assertTrue(exception);

        // Checking waitMany.
        exception = false;
        try {
            List<Pair<Handle, Core.WaitFlags>> handles = new ArrayList<
                    Pair<Handle, Core.WaitFlags>>();
            handles.add(Pair.create(handle, Core.WaitFlags.all()));
            core.waitMany(handles, 0);
        } catch (MojoException e) {
            assertEquals(MojoResult.INVALID_ARGUMENT, e.getMojoResult());
            exception = true;
        }
        assertTrue(exception);

        // Checking sending an invalid handle.
        // Until the behavior is changed on the C++ side, handle gracefully 2 different use case:
        // - Receive a INVALID_ARGUMENT exception
        // - Receive an invalid handle on the other side.
        Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe();
        try {
            handles.first.writeMessage(null, Collections.<Handle> singletonList(handle),
                    MessagePipeHandle.WriteFlags.NONE);
            MessagePipeHandle.ReadMessageResult readMessageResult =
                    handles.second.readMessage(null, 1, MessagePipeHandle.ReadFlags.NONE);
            assertEquals(1, readMessageResult.getHandlesCount());
            assertFalse(readMessageResult.getHandles().get(0).isValid());
        } catch (MojoException e) {
            assertEquals(MojoResult.INVALID_ARGUMENT, e.getMojoResult());
        } finally {
            handles.first.close();
            handles.second.close();
        }
    }

    private static class AsyncWaiterResult implements Callback {
        private int mResult = Integer.MIN_VALUE;
        private MojoException mException = null;

        /**
         * @see Callback#onResult(int)
         */
        @Override
        public void onResult(int result) {
            this.mResult = result;
        }

        /**
         * @see Callback#onError(MojoException)
         */
        @Override
        public void onError(MojoException exception) {
            this.mException = exception;
        }

        /**
         * @return the result
         */
        public int getResult() {
            return mResult;
        }

        /**
         * @return the exception
         */
        public MojoException getException() {
            return mException;
        }

    }

    /**
     * Testing core {@link AsyncWaiter} implementation.
     */
    @SmallTest
    public void testAsyncWaiterCorrectResult() {
        Core core = CoreImpl.getInstance();

        // Checking a correct result.
        Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe();
        try {
            final AsyncWaiterResult asyncWaiterResult = new AsyncWaiterResult();
            assertEquals(Integer.MIN_VALUE, asyncWaiterResult.getResult());
            assertEquals(null, asyncWaiterResult.getException());

            core.getDefaultAsyncWaiter().asyncWait(handles.first, Core.WaitFlags.READABLE,
                    Core.DEADLINE_INFINITE, asyncWaiterResult);
            assertEquals(Integer.MIN_VALUE, asyncWaiterResult.getResult());
            assertEquals(null, asyncWaiterResult.getException());

            handles.second.writeMessage(ByteBuffer.allocateDirect(1), null,
                    MessagePipeHandle.WriteFlags.NONE);
            nativeRunLoop(RUN_LOOP_TIMEOUT_MS);
            assertNull(asyncWaiterResult.getException());
            assertEquals(MojoResult.OK, asyncWaiterResult.getResult());
        } finally {
            handles.first.close();
            handles.second.close();
        }
    }

    /**
     * Testing core {@link AsyncWaiter} implementation.
     */
    @SmallTest
    public void testAsyncWaiterClosingPeerHandle() {
        Core core = CoreImpl.getInstance();

        // Closing the peer handle.
        Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe();
        try {
            final AsyncWaiterResult asyncWaiterResult = new AsyncWaiterResult();
            assertEquals(Integer.MIN_VALUE, asyncWaiterResult.getResult());
            assertEquals(null, asyncWaiterResult.getException());

            core.getDefaultAsyncWaiter().asyncWait(handles.first, Core.WaitFlags.READABLE,
                    Core.DEADLINE_INFINITE, asyncWaiterResult);
            assertEquals(Integer.MIN_VALUE, asyncWaiterResult.getResult());
            assertEquals(null, asyncWaiterResult.getException());

            nativeRunLoop(RUN_LOOP_TIMEOUT_MS);
            assertEquals(Integer.MIN_VALUE, asyncWaiterResult.getResult());
            assertEquals(null, asyncWaiterResult.getException());

            handles.second.close();
            nativeRunLoop(RUN_LOOP_TIMEOUT_MS);
            assertNull(asyncWaiterResult.getException());
            assertEquals(MojoResult.FAILED_PRECONDITION, asyncWaiterResult.getResult());
        } finally {
            handles.first.close();
            handles.second.close();
        }
    }

    /**
     * Testing core {@link AsyncWaiter} implementation.
     */
    @SmallTest
    public void testAsyncWaiterClosingWaitingHandle() {
        Core core = CoreImpl.getInstance();

        // Closing the peer handle.
        Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe();
        try {
            final AsyncWaiterResult asyncWaiterResult = new AsyncWaiterResult();
            assertEquals(Integer.MIN_VALUE, asyncWaiterResult.getResult());
            assertEquals(null, asyncWaiterResult.getException());

            core.getDefaultAsyncWaiter().asyncWait(handles.first, Core.WaitFlags.READABLE,
                    Core.DEADLINE_INFINITE, asyncWaiterResult);
            assertEquals(Integer.MIN_VALUE, asyncWaiterResult.getResult());
            assertEquals(null, asyncWaiterResult.getException());

            nativeRunLoop(RUN_LOOP_TIMEOUT_MS);
            assertEquals(Integer.MIN_VALUE, asyncWaiterResult.getResult());
            assertEquals(null, asyncWaiterResult.getException());

            handles.first.close();
            nativeRunLoop(RUN_LOOP_TIMEOUT_MS);
            // TODO(qsr) Re-enable when MojoWaitMany handles it correctly.
            // assertNull(asyncWaiterResult.getException());
            // assertEquals(MojoResult.CANCELLED, asyncWaiterResult.getResult());
        } finally {
            handles.first.close();
            handles.second.close();
        }
    }

    /**
     * Testing core {@link AsyncWaiter} implementation.
     */
    @SmallTest
    public void testAsyncWaiterWaitingOnInvalidHandle() {
        Core core = CoreImpl.getInstance();

        // Closing the peer handle.
        Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe();
        try {
            final AsyncWaiterResult asyncWaiterResult = new AsyncWaiterResult();
            assertEquals(Integer.MIN_VALUE, asyncWaiterResult.getResult());
            assertEquals(null, asyncWaiterResult.getException());

            handles.first.close();
            core.getDefaultAsyncWaiter().asyncWait(handles.first, Core.WaitFlags.READABLE,
                    Core.DEADLINE_INFINITE, asyncWaiterResult);
            assertEquals(Integer.MIN_VALUE, asyncWaiterResult.getResult());
            assertEquals(null, asyncWaiterResult.getException());

            nativeRunLoop(RUN_LOOP_TIMEOUT_MS);
            assertNotNull(asyncWaiterResult.getException());
            assertEquals(MojoResult.INVALID_ARGUMENT,
                    asyncWaiterResult.getException().getMojoResult());
            assertEquals(Integer.MIN_VALUE, asyncWaiterResult.getResult());
        } finally {
            handles.first.close();
            handles.second.close();
        }
    }

    /**
     * Testing core {@link AsyncWaiter} implementation.
     */
    @SmallTest
    public void testAsyncWaiterWaitingOnDefaultInvalidHandle() {
        Core core = CoreImpl.getInstance();

        final AsyncWaiterResult asyncWaiterResult = new AsyncWaiterResult();
        assertEquals(Integer.MIN_VALUE, asyncWaiterResult.getResult());
        assertEquals(null, asyncWaiterResult.getException());

        core.getDefaultAsyncWaiter().asyncWait(new InvalidHandle(), Core.WaitFlags.READABLE,
                Core.DEADLINE_INFINITE, asyncWaiterResult);
        assertEquals(Integer.MIN_VALUE, asyncWaiterResult.getResult());
        assertEquals(null, asyncWaiterResult.getException());

        nativeRunLoop(RUN_LOOP_TIMEOUT_MS);
        assertNotNull(asyncWaiterResult.getException());
        assertEquals(MojoResult.INVALID_ARGUMENT, asyncWaiterResult.getException().getMojoResult());
        assertEquals(Integer.MIN_VALUE, asyncWaiterResult.getResult());
    }

    /**
     * Testing core {@link AsyncWaiter} implementation.
     */
    @SmallTest
    public void testAsyncWaiterWaitingWithTimeout() {
        Core core = CoreImpl.getInstance();

        // Closing the peer handle.
        Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe();
        try {
            final AsyncWaiterResult asyncWaiterResult = new AsyncWaiterResult();
            assertEquals(Integer.MIN_VALUE, asyncWaiterResult.getResult());
            assertEquals(null, asyncWaiterResult.getException());

            core.getDefaultAsyncWaiter().asyncWait(handles.first, Core.WaitFlags.READABLE,
                    RUN_LOOP_TIMEOUT_MS, asyncWaiterResult);
            assertEquals(Integer.MIN_VALUE, asyncWaiterResult.getResult());
            assertEquals(null, asyncWaiterResult.getException());

            nativeRunLoop(10 * RUN_LOOP_TIMEOUT_MS);
            assertNull(asyncWaiterResult.getException());
            assertEquals(MojoResult.DEADLINE_EXCEEDED, asyncWaiterResult.getResult());
        } finally {
            handles.first.close();
            handles.second.close();
        }
    }

    /**
     * Testing core {@link AsyncWaiter} implementation.
     */
    @SmallTest
    public void testAsyncWaiterCancelWaiting() {
        Core core = CoreImpl.getInstance();

        // Closing the peer handle.
        Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe();
        try {
            final AsyncWaiterResult asyncWaiterResult = new AsyncWaiterResult();
            assertEquals(Integer.MIN_VALUE, asyncWaiterResult.getResult());
            assertEquals(null, asyncWaiterResult.getException());

            Cancellable cancellable = core.getDefaultAsyncWaiter().asyncWait(handles.first,
                    Core.WaitFlags.READABLE, Core.DEADLINE_INFINITE, asyncWaiterResult);
            assertEquals(Integer.MIN_VALUE, asyncWaiterResult.getResult());
            assertEquals(null, asyncWaiterResult.getException());

            nativeRunLoop(RUN_LOOP_TIMEOUT_MS);
            assertEquals(Integer.MIN_VALUE, asyncWaiterResult.getResult());
            assertEquals(null, asyncWaiterResult.getException());

            cancellable.cancel();
            nativeRunLoop(RUN_LOOP_TIMEOUT_MS);
            assertEquals(Integer.MIN_VALUE, asyncWaiterResult.getResult());
            assertEquals(null, asyncWaiterResult.getException());

            handles.second.writeMessage(ByteBuffer.allocateDirect(1), null,
                    MessagePipeHandle.WriteFlags.NONE);
            nativeRunLoop(RUN_LOOP_TIMEOUT_MS);
            assertEquals(Integer.MIN_VALUE, asyncWaiterResult.getResult());
            assertEquals(null, asyncWaiterResult.getException());
        } finally {
            handles.first.close();
            handles.second.close();
        }
    }

    /**
     * Testing core {@link AsyncWaiter} implementation.
     */
    @SmallTest
    public void testAsyncWaiterImmediateCancelOnInvalidHandle() {
        Core core = CoreImpl.getInstance();

        // Closing the peer handle.
        Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe();
        try {
            final AsyncWaiterResult asyncWaiterResult = new AsyncWaiterResult();
            handles.first.close();
            assertEquals(Integer.MIN_VALUE, asyncWaiterResult.getResult());
            assertEquals(null, asyncWaiterResult.getException());

            Cancellable cancellable = core.getDefaultAsyncWaiter().asyncWait(handles.first,
                    Core.WaitFlags.READABLE, Core.DEADLINE_INFINITE, asyncWaiterResult);
            assertEquals(Integer.MIN_VALUE, asyncWaiterResult.getResult());
            assertEquals(null, asyncWaiterResult.getException());
            cancellable.cancel();

            nativeRunLoop(RUN_LOOP_TIMEOUT_MS);
            assertEquals(Integer.MIN_VALUE, asyncWaiterResult.getResult());
            assertEquals(null, asyncWaiterResult.getException());
        } finally {
            handles.first.close();
            handles.second.close();
        }
    }

}
