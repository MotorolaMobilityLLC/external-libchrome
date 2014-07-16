// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.system;

import java.util.List;

/**
 * Core mojo interface giving access to the base operations. See |src/mojo/public/c/system/core.h|
 * for the underlying api.
 */
public interface Core {

    /**
     * Used to indicate an infinite deadline (timeout).
     */
    public static final long DEADLINE_INFINITE = -1;

    /**
     * Signals for the wait operations on handles.
     */
    public static class HandleSignals extends Flags<HandleSignals> {
        /**
         * Constructor.
         *
         * @param signals the serialized signals.
         */
        private HandleSignals(int signals) {
            super(signals);
        }

        private static final int FLAG_NONE = 0;
        private static final int FLAG_READABLE = 1 << 0;
        private static final int FLAG_WRITABLE = 1 << 1;

        /**
         * Immutable signals.
         */
        public static final HandleSignals NONE = HandleSignals.none().immutable();
        public static final HandleSignals READABLE =
                HandleSignals.none().setReadable(true).immutable();
        public static final HandleSignals WRITABLE =
                HandleSignals.none().setWritable(true).immutable();

        /**
         * Change the readable bit of this signal.
         *
         * @param readable the new value of the readable bit.
         * @return this.
         */
        public HandleSignals setReadable(boolean readable) {
            return setFlag(FLAG_READABLE, readable);
        }

        /**
         * Change the writable bit of this signal.
         *
         * @param writable the new value of the writable bit.
         * @return this.
         */
        public HandleSignals setWritable(boolean writable) {
            return setFlag(FLAG_WRITABLE, writable);
        }

        /**
         * @return a signal with no bit set.
         */
        public static HandleSignals none() {
            return new HandleSignals(FLAG_NONE);
        }

    }

    /**
     * @return a platform-dependent monotonically increasing tick count representing "right now."
     */
    public long getTimeTicksNow();

    /**
     * Waits on the given |handle| until the state indicated by |signals| is satisfied or until
     * |deadline| has passed.
     *
     * @return |MojoResult.OK| if some signal in |signals| was satisfied (or is already satisfied).
     *         <p>
     *         |MojoResult.DEADLINE_EXCEEDED| if the deadline has passed without any of the signals
     *         being satisfied.
     *         <p>
     *         |MojoResult.CANCELLED| if |handle| is closed concurrently by another thread.
     *         <p>
     *         |MojoResult.FAILED_PRECONDITION| if it is or becomes impossible that any flag in
     *         |signals| will ever be satisfied (for example, if the other endpoint is close).
     */
    public int wait(Handle handle, HandleSignals signals, long deadline);

    /**
     * Result for the |waitMany| method.
     */
    public static class WaitManyResult {

        /**
         * See |wait| for the different possible values.
         */
        private int mMojoResult;
        /**
         * If |mojoResult| is |MojoResult.OK|, |handleIndex| is the index of the handle for which
         * some flag was satisfied (or is already satisfied). If |mojoResult| is
         * |MojoResult.CANCELLED| or |MojoResult.FAILED_PRECONDITION|, |handleIndex| is the index of
         * the handle for which the issue occurred.
         */
        private int mHandleIndex;

        /**
         * @return the mojoResult
         */
        public int getMojoResult() {
            return mMojoResult;
        }

        /**
         * @param mojoResult the mojoResult to set
         */
        public void setMojoResult(int mojoResult) {
            mMojoResult = mojoResult;
        }

        /**
         * @return the handleIndex
         */
        public int getHandleIndex() {
            return mHandleIndex;
        }

        /**
         * @param handleIndex the handleIndex to set
         */
        public void setHandleIndex(int handleIndex) {
            mHandleIndex = handleIndex;
        }
    }

    /**
     * Waits on handle in |handles| for at least one of them to satisfy the associated
     * |HandleSignals|, or until |deadline| has passed.
     *
     * @returns a |WaitManyResult|.
     */
    public WaitManyResult waitMany(List<Pair<Handle, HandleSignals>> handles, long deadline);

    /**
     * Creates a message pipe, which is a bidirectional communication channel for framed data (i.e.,
     * messages), with the given options. Messages can contain plain data and/or Mojo handles.
     *
     * @return the set of handles for the two endpoints (ports) of the message pipe.
     */
    public Pair<MessagePipeHandle, MessagePipeHandle> createMessagePipe(
            MessagePipeHandle.CreateOptions options);

    /**
     * Creates a data pipe, which is a unidirectional communication channel for unframed data, with
     * the given options. Data is unframed, but must come as (multiples of) discrete elements, of
     * the size given in |options|. See |DataPipe.CreateOptions| for a description of the different
     * options available for data pipes. |options| may be set to null for a data pipe with the
     * default options (which will have an element size of one byte and have some system-dependent
     * capacity).
     *
     * @return the set of handles for the two endpoints of the data pipe.
     */
    public Pair<DataPipe.ProducerHandle, DataPipe.ConsumerHandle> createDataPipe(
            DataPipe.CreateOptions options);

    /**
     * Creates a buffer that can be shared between applications (by duplicating the handle -- see
     * |SharedBufferHandle.duplicate()| -- and passing it over a message pipe). To access the
     * buffer, one must call |SharedBufferHandle.map|.
     *
     * @return the new |SharedBufferHandle|.
     */
    public SharedBufferHandle createSharedBuffer(SharedBufferHandle.CreateOptions options,
            long numBytes);

    /**
     * Returns a default implementation of {@link AsyncWaiter}.
     */
    public AsyncWaiter getDefaultAsyncWaiter();

}
