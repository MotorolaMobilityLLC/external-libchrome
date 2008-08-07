// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef BASE_SINGLETON_H__
#define BASE_SINGLETON_H__

#include <stdlib.h>

#include <utility>

#include "base/at_exit.h"
#include "base/atomicops.h"
#include "base/platform_thread.h"

// Default traits for Singleton<Type>. Calls operator new and operator delete on
// the object. Registers automatic deletion at process exit.
// Overload if you need arguments or another memory allocation function.
template<typename Type>
struct DefaultSingletonTraits {
  // Allocates the object.
  static Type* New() {
    // The parenthesis is very important here; it forces POD type
    // initialization.
    return new Type();
  }

  // Destroys the object.
  static void Delete(Type* x) {
    delete x;
  }

  // Set to true to automatically register deletion of the object on process
  // exit. See below for the required call that makes this happen.
  static const bool kRegisterAtExit = true;
};

// The Singleton<Type, Traits, DifferentiatingType> class manages a single
// instance of Type which will be created on first use and will be destroyed at
// normal process exit). The Trait::Delete function will not be called on
// abnormal process exit.
//
// DifferentiatingType is used as a key to differentiate two different
// singletons having the same memory allocation functions but serving a
// different purpose. This is mainly used for Locks serving different purposes.
//
// Example usages: (none are preferred, they all result in the same code)
//   1. FooClass* ptr = Singleton<FooClass>::get();
//      ptr->Bar();
//   2. Singleton<FooClass>()->Bar();
//   3. Singleton<FooClass>::get()->Bar();
//
// Singleton<> has no non-static members and doesn't need to actually be
// instantiated. It does no harm to instantiate it and use it as a class member
// or at global level since it is acting as a POD type.
//
// This class is itself thread-safe. The underlying Type must of course be
// thread-safe if you want to use it concurrently. Two parameters may be tuned
// depending on the user's requirements.
//
// Glossary:
//   RAE = kRegisterAtExit
//
// On every platform, if Traits::RAE is true, the singleton will be destroyed at
// process exit. More precisely it uses base::AtExitManager which requires an
// object of this type to be instanciated. AtExitManager mimics the semantics
// of atexit() such as LIFO order but under Windows is safer to call. For more
// information see at_exit.h.
//
// If Traits::RAE is false, the singleton will not be freed at process exit,
// thus the singleton will be leaked if it is ever accessed. Traits::RAE
// shouldn't be false unless absolutely necessary. Remember that the heap where
// the object is allocated may be destroyed by the CRT anyway.
//
// If you want to ensure that your class can only exist as a singleton, make
// its constructors private, and make DefaultSingletonTraits<> a friend:
//
//   #include "base/singleton.h"
//   class FooClass {
//    public:
//     void Bar() { ... }
//    private:
//     FooClass() { ... }
//     friend DefaultSingletonTraits<FooClass>;
//
//     DISALLOW_EVIL_CONSTRUCTORS(FooClass);
//   };
//
// Caveats:
// (a) Every call to get(), operator->() and operator*() incurs some overhead
//     (16ns on my P4/2.8GHz) to check whether the object has already been
//     initialized.  You may wish to cache the result of get(); it will not
//     change.
//
// (b) Your factory function must never throw an exception. This class is not
//     exception-safe.
//
template <typename Type,
          typename Traits = DefaultSingletonTraits<Type>,
          typename DifferentiatingType = Type>
class Singleton {
 public:
  // This class is safe to be constructed and copy-constructed since it has no
  // member.

  // Return a pointer to the one true instance of the class.
  static Type* get() {
    // Our AtomicWord doubles as a spinlock, where a value of
    // kBeingCreatedMarker means the spinlock is being held for creation.
    static const base::subtle::AtomicWord kBeingCreatedMarker = 1;

    base::subtle::AtomicWord value = base::subtle::NoBarrier_Load(&instance_);
    if (value != 0 && value != kBeingCreatedMarker)
      return reinterpret_cast<Type*>(value);

    // Object isn't created yet, maybe we will get to create it, let's try...
    if (base::subtle::Acquire_CompareAndSwap(&instance_,
                                             0,
                                             kBeingCreatedMarker) == 0) {
      // instance_ was NULL and is now kBeingCreatedMarker.  Only one thread
      // will ever get here.  Threads might be spinning on us, and they will
      // stop right after we do this store.
      Type* newval = Traits::New();
      base::subtle::Release_Store(
          &instance_, reinterpret_cast<base::subtle::AtomicWord>(newval));

      if (Traits::kRegisterAtExit)
        base::AtExitManager::RegisterCallback(OnExit);

      return newval;
    }

    // We hit a race.  Another thread beat us and either:
    // - Has the object in BeingCreated state
    // - Already has the object created...
    // We know value != NULL.  It could be kBeingCreatedMarker, or a valid ptr.
    // Unless your constructor can be very time consuming, it is very unlikely
    // to hit this race.  When it does, we just spin and yield the thread until
    // the object has been created.
    while (true) {
      value = base::subtle::NoBarrier_Load(&instance_);
      if (value != kBeingCreatedMarker)
        break;
      PlatformThread::YieldCurrentThread();
    }

    return reinterpret_cast<Type*>(value);
  }

  // Shortcuts.
  Type& operator*() {
    return *get();
  }

  Type* operator->() {
    return get();
  }

 private:
  // Adapter function for use with AtExit().  This should be called single
  // threaded, but we might as well take the precautions anyway.
  static void OnExit() {
    // AtExit should only ever be register after the singleton instance was
    // created.  We should only ever get here with a valid instance_ pointer.
    // We skip the DCHECK because we don't want to pull in logging.h :/
    Traits::Delete(reinterpret_cast<Type*>(instance_));
    base::subtle::Release_Store(&instance_, 0);
  }
  static base::subtle::AtomicWord instance_;
};

template <typename Type, typename Traits, typename DifferentiatingType>
base::subtle::AtomicWord Singleton<Type, Traits, DifferentiatingType>::
    instance_ = 0;


#endif  // BASE_SINGLETON_H__
