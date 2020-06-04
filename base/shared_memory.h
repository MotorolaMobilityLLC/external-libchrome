// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SHARED_MEMORY_H_
#define BASE_SHARED_MEMORY_H_

#include <string>
#include "base/basictypes.h"
#include "base/process.h"

// SharedMemoryHandle is a platform specific type which represents
// the underlying OS handle to a shared memory segment.
#if defined(OS_WIN)
typedef HANDLE SharedMemoryHandle;
typedef HANDLE SharedMemoryLock;
#elif defined(OS_POSIX)
#include <semaphore.h>
typedef int SharedMemoryHandle;
typedef sem_t* SharedMemoryLock;
#endif

// Platform abstraction for shared memory.  Provides a C++ wrapper
// around the OS primitive for a memory mapped file.
class SharedMemory {
 public:
  // Create a new SharedMemory object.
  SharedMemory();

  // Create a new SharedMemory object from an existing, open
  // shared memory file.
  SharedMemory(SharedMemoryHandle handle, bool read_only);

  // Create a new SharedMemory object from an existing, open
  // shared memory file that was created by a remote process and not shared
  // to the current process.
  SharedMemory(SharedMemoryHandle handle, bool read_only,
      ProcessHandle process);

  // Destructor.  Will close any open files.
  ~SharedMemory();

  // Creates or opens a shared memory segment based on a name.
  // If read_only is true, opens the memory as read-only.
  // If open_existing is true, and the shared memory already exists,
  // opens the existing shared memory and ignores the size parameter.
  // Returns true on success, false on failure.
  bool Create(const std::wstring& name, bool read_only, bool open_existing,
              size_t size);

  // Opens a shared memory segment based on a name.
  // If read_only is true, opens for read-only access.
  // Returns true on success, false on failure.
  bool Open(const std::wstring& name, bool read_only);

  // Maps the shared memory into the caller's address space.
  // Returns true on success, false otherwise.  The memory address
  // is accessed via the memory() accessor.
  bool Map(size_t bytes);

  // Unmaps the shared memory from the caller's address space.
  // Returns true if successful; returns false on error or if the
  // memory is not mapped.
  bool Unmap();

  // Get the size of the opened shared memory backing file.
  // Note:  This size is only available to the creator of the
  // shared memory, and not to those that opened shared memory
  // created externally.
  // Returns 0 if not opened or unknown.
  size_t max_size() const { return max_size_; }

  // Gets a pointer to the opened memory space if it has been
  // Mapped via Map().  Returns NULL if it is not mapped.
  void *memory() const { return memory_; }

  // Get access to the underlying OS handle for this segment.
  // Use of this handle for anything other than an opaque
  // identifier is not portable.
  SharedMemoryHandle handle() const { return mapped_file_; }

  // Closes the open shared memory segment.
  // It is safe to call Close repeatedly.
  void Close();

  // Share the shared memory to another process.  Attempts
  // to create a platform-specific new_handle which can be
  // used in a remote process to access the shared memory
  // file.  new_handle is an ouput parameter to receive
  // the handle for use in the remote process.
  // Returns true on success, false otherwise.
  bool ShareToProcess(ProcessHandle process,
                      SharedMemoryHandle* new_handle) {
    return ShareToProcessCommon(process, new_handle, false);
  }

  // Logically equivalent to:
  //   bool ok = ShareToProcess(process, new_handle);
  //   Close();
  //   return ok;
  bool GiveToProcess(ProcessHandle process,
                     SharedMemoryHandle* new_handle) {
    return ShareToProcessCommon(process, new_handle, true);
  }

  // Lock the shared memory.
  // This is a cross-process lock which may be recursively
  // locked by the same thread.
  void Lock();

  // Release the shared memory lock.
  void Unlock();

 private:
#if defined(OS_POSIX)
  bool CreateOrOpen(const std::wstring &name, int posix_flags);
#endif
  bool ShareToProcessCommon(ProcessHandle process,
                            SharedMemoryHandle* new_handle,
                            bool close_self);

  std::wstring       name_;
  SharedMemoryHandle mapped_file_;
  void*              memory_;
  bool               read_only_;
  size_t             max_size_;
  SharedMemoryLock   lock_;

  DISALLOW_EVIL_CONSTRUCTORS(SharedMemory);
};

// A helper class that acquires the shared memory lock while
// the SharedMemoryAutoLock is in scope.
class SharedMemoryAutoLock {
 public:
  explicit SharedMemoryAutoLock(SharedMemory* shared_memory)
      : shared_memory_(shared_memory) {
    shared_memory_->Lock();
  }

  ~SharedMemoryAutoLock() {
    shared_memory_->Unlock();
  }

 private:
  SharedMemory* shared_memory_;
  DISALLOW_EVIL_CONSTRUCTORS(SharedMemoryAutoLock);
};


#endif  // BASE_SHARED_MEMORY_H_
