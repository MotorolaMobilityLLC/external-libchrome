// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_DISPATCHER_H_
#define MOJO_SYSTEM_DISPATCHER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "mojo/public/system/core.h"
#include "mojo/public/system/system_export.h"

namespace mojo {
namespace system {

class CoreImpl;
class Waiter;

// A |Dispatcher| implements Mojo primitives that are "attached" to a particular
// handle. This includes most (all?) primitives except for |MojoWait...()|. This
// object is thread-safe, with its state being protected by a single lock
// |lock_|, which is also made available to implementation subclasses (via the
// |lock()| method).
class MOJO_SYSTEM_EXPORT Dispatcher :
    public base::RefCountedThreadSafe<Dispatcher> {
 public:
  // These methods implement the various primitives named |Mojo...()|. These
  // take |lock_| and handle races with |Close()|. Then they call out to
  // subclasses' |...ImplNoLock()| methods (still under |lock_|), which actually
  // implement the primitives.
  // NOTE(vtl): This puts a big lock around each dispatcher (i.e., handle), and
  // prevents the various |...ImplNoLock()|s from releasing the lock as soon as
  // possible. If this becomes an issue, we can rethink this.
  MojoResult Close();
  // |dispatchers| may be non-null if and only if there are handles to be
  // written, in which case this will be called with all the dispatchers' locks
  // held. On success, all the dispatchers must have been moved to a closed
  // state; on failure, they should remain in their original state.
  MojoResult WriteMessage(const void* bytes, uint32_t num_bytes,
                          const std::vector<Dispatcher*>* dispatchers,
                          MojoWriteMessageFlags flags);
  // |dispatchers| must be non-null but empty, if |num_dispatchers| is non-null
  // and nonzero. On success, it will be set to the dispatchers to be received
  // (and assigned handles) as part of the message.
  MojoResult ReadMessage(
      void* bytes, uint32_t* num_bytes,
      std::vector<scoped_refptr<Dispatcher> >* dispatchers,
      uint32_t* num_dispatchers,
      MojoReadMessageFlags flags);

  // Adds a waiter to this dispatcher. The waiter will be woken up when this
  // object changes state to satisfy |flags| with result |wake_result| (which
  // must be >= 0, i.e., a success status). It will also be woken up when it
  // becomes impossible for the object to ever satisfy |flags| with a suitable
  // error status.
  //
  // Returns:
  //  - |MOJO_RESULT_OK| if the waiter was added;
  //  - |MOJO_RESULT_ALREADY_EXISTS| if |flags| is already satisfied;
  //  - |MOJO_RESULT_INVALID_ARGUMENT| if the dispatcher has been closed; and
  //  - |MOJO_RESULT_FAILED_PRECONDITION| if it is not (or no longer) possible
  //    that |flags| will ever be satisfied.
  MojoResult AddWaiter(Waiter* waiter,
                       MojoWaitFlags flags,
                       MojoResult wake_result);
  void RemoveWaiter(Waiter* waiter);

  // Creates an equivalent dispatcher -- representing the same resource as this
  // dispatcher -- and close (i.e., disable) this dispatcher. I.e., this
  // dispatcher will look as though it was closed, but the resource it
  // represents will be assigned to the new dispatcher. This must be called
  // under the dispatcher's lock.
  scoped_refptr<Dispatcher> CreateEquivalentDispatcherAndCloseNoLock();

 protected:
  Dispatcher();

  friend class base::RefCountedThreadSafe<Dispatcher>;
  virtual ~Dispatcher();

  // These are to be overridden by subclasses (if necessary). They are called
  // exactly once -- first |CancelAllWaitersNoLock()|, then |CloseImplNoLock()|,
  // when the dispatcher is being closed. They are called under |lock_|.
  virtual void CancelAllWaitersNoLock();
  virtual MojoResult CloseImplNoLock();

  // These are to be overridden by subclasses (if necessary). They are never
  // called after the dispatcher has been closed. They are called under |lock_|.
  // See the descriptions of the methods without the "ImplNoLock" for more
  // information.
  virtual MojoResult WriteMessageImplNoLock(
      const void* bytes, uint32_t num_bytes,
      const std::vector<Dispatcher*>* dispatchers,
      MojoWriteMessageFlags flags);
  virtual MojoResult ReadMessageImplNoLock(
      void* bytes, uint32_t* num_bytes,
      std::vector<scoped_refptr<Dispatcher> >* dispatchers,
      uint32_t* num_dispatchers,
      MojoReadMessageFlags flags);
  virtual MojoResult AddWaiterImplNoLock(Waiter* waiter,
                                         MojoWaitFlags flags,
                                         MojoResult wake_result);
  virtual void RemoveWaiterImplNoLock(Waiter* waiter);

  // This must be implemented by subclasses, since only they can instantiate a
  // new dispatcher of the same class. See
  // |CreateEquivalentDispatcherAndCloseNoLock()| for more details.
  virtual scoped_refptr<Dispatcher>
      CreateEquivalentDispatcherAndCloseImplNoLock() = 0;

  // Available to subclasses. (Note: Returns a non-const reference, just like
  // |base::AutoLock|'s constructor takes a non-const reference.)
  base::Lock& lock() const { return lock_; }

  bool is_closed_no_lock() const { return is_closed_; }

 private:
  // For |WriteMessage()|, |CoreImpl| needs access to |lock()| and
  // |is_closed_no_lock()|.
  friend class CoreImpl;

  // This protects the following members as well as any state added by
  // subclasses.
  mutable base::Lock lock_;
  bool is_closed_;

  DISALLOW_COPY_AND_ASSIGN(Dispatcher);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_DISPATCHER_H_
