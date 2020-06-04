// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/raw_channel.h"

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

#include <algorithm>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/lock.h"
#include "mojo/embedder/platform_channel_utils_posix.h"
#include "mojo/embedder/platform_handle.h"
#include "mojo/embedder/platform_handle_vector.h"
#include "mojo/system/transport_data.h"

namespace mojo {
namespace system {

namespace {

class RawChannelPosix : public RawChannel,
                        public base::MessageLoopForIO::Watcher {
 public:
  RawChannelPosix(embedder::ScopedPlatformHandle handle);
  virtual ~RawChannelPosix();

  // |RawChannel| public methods:
  virtual size_t GetSerializedPlatformHandleSize() const OVERRIDE;

 private:
  // |RawChannel| protected methods:
  virtual IOResult Read(size_t* bytes_read) OVERRIDE;
  virtual IOResult ScheduleRead() OVERRIDE;
  virtual embedder::ScopedPlatformHandleVectorPtr GetReadPlatformHandles(
      size_t num_platform_handles,
      const void* platform_handle_table) OVERRIDE;
  virtual IOResult WriteNoLock(size_t* platform_handles_written,
                               size_t* bytes_written) OVERRIDE;
  virtual IOResult ScheduleWriteNoLock() OVERRIDE;
  virtual bool OnInit() OVERRIDE;
  virtual void OnShutdownNoLock(
      scoped_ptr<ReadBuffer> read_buffer,
      scoped_ptr<WriteBuffer> write_buffer) OVERRIDE;

  // |base::MessageLoopForIO::Watcher| implementation:
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE;

  // Watches for |fd_| to become writable. Must be called on the I/O thread.
  void WaitToWrite();

  embedder::ScopedPlatformHandle fd_;

  // The following members are only used on the I/O thread:
  scoped_ptr<base::MessageLoopForIO::FileDescriptorWatcher> read_watcher_;
  scoped_ptr<base::MessageLoopForIO::FileDescriptorWatcher> write_watcher_;

  bool pending_read_;

  embedder::ScopedPlatformHandleVectorPtr read_platform_handles_;

  // The following members are used on multiple threads and protected by
  // |write_lock()|:
  bool pending_write_;

  // This is used for posting tasks from write threads to the I/O thread. It
  // must only be accessed under |write_lock_|. The weak pointers it produces
  // are only used/invalidated on the I/O thread.
  base::WeakPtrFactory<RawChannelPosix> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RawChannelPosix);
};

RawChannelPosix::RawChannelPosix(embedder::ScopedPlatformHandle handle)
    : fd_(handle.Pass()),
      pending_read_(false),
      pending_write_(false),
      weak_ptr_factory_(this) {
  DCHECK(fd_.is_valid());
}

RawChannelPosix::~RawChannelPosix() {
  DCHECK(!pending_read_);
  DCHECK(!pending_write_);

  // No need to take the |write_lock()| here -- if there are still weak pointers
  // outstanding, then we're hosed anyway (since we wouldn't be able to
  // invalidate them cleanly, since we might not be on the I/O thread).
  DCHECK(!weak_ptr_factory_.HasWeakPtrs());

  // These must have been shut down/destroyed on the I/O thread.
  DCHECK(!read_watcher_.get());
  DCHECK(!write_watcher_.get());
}

size_t RawChannelPosix::GetSerializedPlatformHandleSize() const {
  // We don't actually need any space on POSIX (since we just send FDs).
  return 0;
}

RawChannel::IOResult RawChannelPosix::Read(size_t* bytes_read) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_for_io());
  DCHECK(!pending_read_);

  char* buffer = NULL;
  size_t bytes_to_read = 0;
  read_buffer()->GetBuffer(&buffer, &bytes_to_read);

  size_t old_num_platform_handles =
      read_platform_handles_ ? read_platform_handles_->size() : 0;
  ssize_t read_result =
      embedder::PlatformChannelRecvmsg(fd_.get(),
                                       buffer,
                                       bytes_to_read,
                                       &read_platform_handles_);
  if (read_result > 0) {
    *bytes_read = static_cast<size_t>(read_result);
    return IO_SUCCEEDED;
  }

  if (read_platform_handles_ &&
      read_platform_handles_->size() > old_num_platform_handles) {
    if (read_result != 1) {
      LOG(WARNING) << "Invalid control message with platform handles";
      return IO_FAILED;
    }

    if (read_platform_handles_->size() > TransportData::kMaxPlatformHandles) {
      LOG(WARNING) << "Received too many platform handles";
      read_platform_handles_.reset();
      return IO_FAILED;
    }
  }

  // |read_result == 0| means "end of file".
  if (read_result == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
    PLOG_IF(ERROR, read_result != 0) << "recvmsg";

    // Make sure that |OnFileCanReadWithoutBlocking()| won't be called again.
    read_watcher_.reset();

    return IO_FAILED;
  }

  return ScheduleRead();
}

RawChannel::IOResult RawChannelPosix::ScheduleRead() {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_for_io());
  DCHECK(!pending_read_);

  pending_read_ = true;

  return IO_PENDING;
}

embedder::ScopedPlatformHandleVectorPtr RawChannelPosix::GetReadPlatformHandles(
    size_t num_platform_handles,
    const void* /*platform_handle_table*/) {
  DCHECK_GT(num_platform_handles, 0u);

  if (!read_platform_handles_ ||
      read_platform_handles_->size() != num_platform_handles) {
    read_platform_handles_.reset();
    return embedder::ScopedPlatformHandleVectorPtr();
  }

  return read_platform_handles_.Pass();
}

RawChannel::IOResult RawChannelPosix::WriteNoLock(
    size_t* platform_handles_written,
    size_t* bytes_written) {
  write_lock().AssertAcquired();

  DCHECK(!pending_write_);

  if (write_buffer_no_lock()->HavePlatformHandlesToSend()) {
    size_t num_platform_handles;
    embedder::PlatformHandle* platform_handles;
    void* serialization_data;  // Actually unused.
    write_buffer_no_lock()->GetPlatformHandlesToSend(&num_platform_handles,
                                                     &platform_handles,
                                                     &serialization_data);
    DCHECK_GT(num_platform_handles, 0u);
    DCHECK(platform_handles);
    DCHECK(serialization_data);

    size_t num_to_send = std::min(num_platform_handles,
                                  embedder::kPlatformChannelMaxNumHandles);
    bool succeeded = embedder::PlatformChannelSendHandles(fd_.get(),
                                                          platform_handles,
                                                          num_to_send);
    if (succeeded) {
      *platform_handles_written = num_to_send;
      *bytes_written = 0;
      return IO_SUCCEEDED;
    }
  } else {
    std::vector<WriteBuffer::Buffer> buffers;
    write_buffer_no_lock()->GetBuffers(&buffers);
    DCHECK(!buffers.empty());

    ssize_t write_result;
    if (buffers.size() == 1) {
      write_result = embedder::PlatformChannelWrite(fd_.get(), buffers[0].addr,
                                                    buffers[0].size);
    } else {
      const size_t kMaxBufferCount = 10;
      iovec iov[kMaxBufferCount];
      size_t buffer_count = std::min(buffers.size(), kMaxBufferCount);

      for (size_t i = 0; i < buffer_count; ++i) {
        iov[i].iov_base = const_cast<char*>(buffers[i].addr);
        iov[i].iov_len = buffers[i].size;
      }

      write_result = embedder::PlatformChannelWritev(fd_.get(), iov,
                                                     buffer_count);
    }

    if (write_result >= 0) {
      *platform_handles_written = 0;
      *bytes_written = static_cast<size_t>(write_result);
      return IO_SUCCEEDED;
    }
  }

  if (errno != EAGAIN && errno != EWOULDBLOCK) {
    PLOG(ERROR) << "sendmsg/write/writev";
    return IO_FAILED;
  }

  return ScheduleWriteNoLock();
}

RawChannel::IOResult RawChannelPosix::ScheduleWriteNoLock() {
  write_lock().AssertAcquired();

  DCHECK(!pending_write_);

  // Set up to wait for the FD to become writable.
  // If we're not on the I/O thread, we have to post a task to do this.
  if (base::MessageLoop::current() != message_loop_for_io()) {
    message_loop_for_io()->PostTask(
        FROM_HERE,
        base::Bind(&RawChannelPosix::WaitToWrite,
                   weak_ptr_factory_.GetWeakPtr()));
    pending_write_ = true;
    return IO_PENDING;
  }

  if (message_loop_for_io()->WatchFileDescriptor(fd_.get().fd, false,
          base::MessageLoopForIO::WATCH_WRITE, write_watcher_.get(), this)) {
    pending_write_ = true;
    return IO_PENDING;
  }

  return IO_FAILED;
}

bool RawChannelPosix::OnInit() {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_for_io());

  DCHECK(!read_watcher_.get());
  read_watcher_.reset(new base::MessageLoopForIO::FileDescriptorWatcher());
  DCHECK(!write_watcher_.get());
  write_watcher_.reset(new base::MessageLoopForIO::FileDescriptorWatcher());

  if (!message_loop_for_io()->WatchFileDescriptor(fd_.get().fd, true,
          base::MessageLoopForIO::WATCH_READ, read_watcher_.get(), this)) {
    // TODO(vtl): I'm not sure |WatchFileDescriptor()| actually fails cleanly
    // (in the sense of returning the message loop's state to what it was before
    // it was called).
    read_watcher_.reset();
    write_watcher_.reset();
    return false;
  }

  return true;
}

void RawChannelPosix::OnShutdownNoLock(
    scoped_ptr<ReadBuffer> /*read_buffer*/,
    scoped_ptr<WriteBuffer> /*write_buffer*/) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_for_io());
  write_lock().AssertAcquired();

  read_watcher_.reset();  // This will stop watching (if necessary).
  write_watcher_.reset();  // This will stop watching (if necessary).

  pending_read_ = false;
  pending_write_ = false;

  DCHECK(fd_.is_valid());
  fd_.reset();

  weak_ptr_factory_.InvalidateWeakPtrs();
}

void RawChannelPosix::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_EQ(fd, fd_.get().fd);
  DCHECK_EQ(base::MessageLoop::current(), message_loop_for_io());

  if (!pending_read_) {
    NOTREACHED();
    return;
  }

  pending_read_ = false;
  size_t bytes_read = 0;
  IOResult result = Read(&bytes_read);
  if (result != IO_PENDING)
    OnReadCompleted(result == IO_SUCCEEDED, bytes_read);

  // On failure, |read_watcher_| must have been reset; on success,
  // we assume that |OnReadCompleted()| always schedules another read.
  // Otherwise, we could end up spinning -- getting
  // |OnFileCanReadWithoutBlocking()| again and again but not doing any actual
  // read.
  // TODO(yzshen): An alternative is to stop watching if RawChannel doesn't
  // schedule a new read. But that code won't be reached under the current
  // RawChannel implementation.
  DCHECK(!read_watcher_.get() || pending_read_);
}

void RawChannelPosix::OnFileCanWriteWithoutBlocking(int fd) {
  DCHECK_EQ(fd, fd_.get().fd);
  DCHECK_EQ(base::MessageLoop::current(), message_loop_for_io());

  IOResult result = IO_FAILED;
  size_t platform_handles_written = 0;
  size_t bytes_written = 0;
  {
    base::AutoLock locker(write_lock());

    DCHECK(pending_write_);

    pending_write_ = false;
    result = WriteNoLock(&platform_handles_written, &bytes_written);
  }

  if (result != IO_PENDING) {
    OnWriteCompleted(result == IO_SUCCEEDED,
                     platform_handles_written,
                     bytes_written);
  }
}

void RawChannelPosix::WaitToWrite() {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_for_io());

  DCHECK(write_watcher_.get());

  if (!message_loop_for_io()->WatchFileDescriptor(
          fd_.get().fd, false, base::MessageLoopForIO::WATCH_WRITE,
          write_watcher_.get(), this)) {
    {
      base::AutoLock locker(write_lock());

      DCHECK(pending_write_);
      pending_write_ = false;
    }
    OnWriteCompleted(false, 0, 0);
  }
}

}  // namespace

// -----------------------------------------------------------------------------

// Static factory method declared in raw_channel.h.
// static
scoped_ptr<RawChannel> RawChannel::Create(
    embedder::ScopedPlatformHandle handle) {
  return scoped_ptr<RawChannel>(new RawChannelPosix(handle.Pass()));
}

}  // namespace system
}  // namespace mojo
