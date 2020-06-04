// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Scopers help you manage ownership of a pointer, helping you easily manage the
// a pointer within a scope, and automatically destroying the pointer at the
// end of a scope.  There are two main classes you will use, which correspond
// to the operators new/delete and new[]/delete[].
//
// Example usage (scoped_ptr):
//   {
//     scoped_ptr<Foo> foo(new Foo("wee"));
//   }  // foo goes out of scope, releasing the pointer with it.
//
//   {
//     scoped_ptr<Foo> foo;          // No pointer managed.
//     foo.reset(new Foo("wee"));    // Now a pointer is managed.
//     foo.reset(new Foo("wee2"));   // Foo("wee") was destroyed.
//     foo.reset(new Foo("wee3"));   // Foo("wee2") was destroyed.
//     foo->Method();                // Foo::Method() called.
//     foo.get()->Method();          // Foo::Method() called.
//     SomeFunc(foo.release());      // SomeFunc takes ownership, foo no longer
//                                   // manages a pointer.
//     foo.reset(new Foo("wee4"));   // foo manages a pointer again.
//     foo.reset();                  // Foo("wee4") destroyed, foo no longer
//                                   // manages a pointer.
//   }  // foo wasn't managing a pointer, so nothing was destroyed.
//
// Example usage (scoped_array):
//   {
//     scoped_array<Foo> foo(new Foo[100]);
//     foo.get()->Method();  // Foo::Method on the 0th element.
//     foo[10].Method();     // Foo::Method on the 10th element.
//   }
//
// These scopers also implement part of the functionality of C++11 unique_ptr
// in that they are "movable but not copyable."  You can use the scopers in
// the parameter and return types of functions to signify ownership transfer
// in to and out of a function.  When calling a function that has a scoper
// as the argument type, it must be called with the result of an analogous
// scoper's Pass() function or another function that generates a temporary;
// passing by copy will NOT work.  Here is an example using scoped_ptr:
//
//   void TakesOwnership(scoped_ptr<Foo> arg) {
//     // Do something with arg
//   }
//   scoped_ptr<Foo> CreateFoo() {
//     // No need for calling Pass() because we are constructing a temporary
//     // for the return value.
//     return scoped_ptr<Foo>(new Foo("new"));
//   }
//   scoped_ptr<Foo> PassThru(scoped_ptr<Foo> arg) {
//     return arg.Pass();
//   }
//
//   {
//     scoped_ptr<Foo> ptr(new Foo("yay"));  // ptr manages Foo("yay)"
//     TakesOwnership(ptr.Pass());           // ptr no longer owns Foo("yay").
//     scoped_ptr<Foo> ptr2 = CreateFoo();   // ptr2 owns the return Foo.
//     scoped_ptr<Foo> ptr3 =                // ptr3 now owns what was in ptr2.
//         PassThru(ptr2.Pass());            // ptr2 is correspondingly NULL.
//   }
//
// Notice that if you do not call Pass() when returning from PassThru(), or
// when invoking TakesOwnership(), the code will not compile because scopers
// are not copyable; they only implement move semantics which require calling
// the Pass() function to signify a destructive transfer of state. CreateFoo()
// is different though because we are constructing a temporary on the return
// line and thus can avoid needing to call Pass().

#ifndef BASE_MEMORY_SCOPED_PTR_H_
#define BASE_MEMORY_SCOPED_PTR_H_
#pragma once

// This is an implementation designed to match the anticipated future TR2
// implementation of the scoped_ptr class, and its closely-related brethren,
// scoped_array, scoped_ptr_malloc.

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#include "base/compiler_specific.h"

// Macro with the boilerplate C++03 move emulation for a class.
//
// In C++11, this is done via rvalue references.  Here, we use C++03 move
// emulation to fake an rvalue reference.  For a more thorough explanation
// of the technique, see:
//
//   http://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Move_Constructor
//
#define CPP_03_MOVE_EMULATION(scoper, field) \
 private: \
  struct RValue { \
    explicit RValue(scoper& obj) : obj_(obj) {} \
    scoper& obj_; \
  }; \
 public: \
  operator RValue() { return RValue(*this); } \
  scoper(RValue proxy) : field(proxy.obj_.release()) { } \
  scoper& operator=(RValue proxy) { \
    swap(proxy.obj_); \
    return *this; \
  } \
  scoper Pass() { return scoper(RValue(*this)); }

// A scoped_ptr<T> is like a T*, except that the destructor of scoped_ptr<T>
// automatically deletes the pointer it holds (if any).
// That is, scoped_ptr<T> owns the T object that it points to.
// Like a T*, a scoped_ptr<T> may hold either NULL or a pointer to a T object.
// Also like T*, scoped_ptr<T> is thread-compatible, and once you
// dereference it, you get the thread safety guarantees of T.
//
// The size of a scoped_ptr is small:
// sizeof(scoped_ptr<C>) == sizeof(C*)
template <class C>
class scoped_ptr {
 public:

  // The element type
  typedef C element_type;

  // Constructor.  Defaults to initializing with NULL.
  // There is no way to create an uninitialized scoped_ptr.
  // The input parameter must be allocated with new.
  explicit scoped_ptr(C* p = NULL) : ptr_(p) { }

  // Destructor.  If there is a C object, delete it.
  // We don't need to test ptr_ == NULL because C++ does that for us.
  ~scoped_ptr() {
    enum { type_must_be_complete = sizeof(C) };
    delete ptr_;
  }

  // Reset.  Deletes the current owned object, if any.
  // Then takes ownership of a new object, if given.
  // this->reset(this->get()) works.
  void reset(C* p = NULL) {
    if (p != ptr_) {
      enum { type_must_be_complete = sizeof(C) };
      delete ptr_;
      ptr_ = p;
    }
  }

  // Accessors to get the owned object.
  // operator* and operator-> will assert() if there is no current object.
  C& operator*() const {
    assert(ptr_ != NULL);
    return *ptr_;
  }
  C* operator->() const  {
    assert(ptr_ != NULL);
    return ptr_;
  }
  C* get() const { return ptr_; }

  // Comparison operators.
  // These return whether two scoped_ptr refer to the same object, not just to
  // two different but equal objects.
  bool operator==(C* p) const { return ptr_ == p; }
  bool operator!=(C* p) const { return ptr_ != p; }

  // Swap two scoped pointers.
  void swap(scoped_ptr& p2) {
    C* tmp = ptr_;
    ptr_ = p2.ptr_;
    p2.ptr_ = tmp;
  }

  // Release a pointer.
  // The return value is the current pointer held by this object.
  // If this object holds a NULL pointer, the return value is NULL.
  // After this operation, this object will hold a NULL pointer,
  // and will not own the object any more.
  C* release() WARN_UNUSED_RESULT {
    C* retVal = ptr_;
    ptr_ = NULL;
    return retVal;
  }

  CPP_03_MOVE_EMULATION(scoped_ptr, ptr_);

 private:
  C* ptr_;

  // Forbid comparison of scoped_ptr types.  If C2 != C, it totally doesn't
  // make sense, and if C2 == C, it still doesn't make sense because you should
  // never have the same object owned by two different scoped_ptrs.
  template <class C2> bool operator==(scoped_ptr<C2> const& p2) const;
  template <class C2> bool operator!=(scoped_ptr<C2> const& p2) const;

  // Disallow evil constructors.  Note that MUST NOT take a const& because we
  // are implementing move semantics.  See the CPP_03_MOVE_EMULATION macro.
  scoped_ptr(scoped_ptr&);
  void operator=(scoped_ptr&);
};

// Free functions
template <class C>
void swap(scoped_ptr<C>& p1, scoped_ptr<C>& p2) {
  p1.swap(p2);
}

template <class C>
bool operator==(C* p1, const scoped_ptr<C>& p2) {
  return p1 == p2.get();
}

template <class C>
bool operator!=(C* p1, const scoped_ptr<C>& p2) {
  return p1 != p2.get();
}

// scoped_array<C> is like scoped_ptr<C>, except that the caller must allocate
// with new [] and the destructor deletes objects with delete [].
//
// As with scoped_ptr<C>, a scoped_array<C> either points to an object
// or is NULL.  A scoped_array<C> owns the object that it points to.
// scoped_array<T> is thread-compatible, and once you index into it,
// the returned objects have only the thread safety guarantees of T.
//
// Size: sizeof(scoped_array<C>) == sizeof(C*)
template <class C>
class scoped_array {
 public:

  // The element type
  typedef C element_type;

  // Constructor.  Defaults to initializing with NULL.
  // There is no way to create an uninitialized scoped_array.
  // The input parameter must be allocated with new [].
  explicit scoped_array(C* p = NULL) : array_(p) { }

  // Destructor.  If there is a C object, delete it.
  // We don't need to test ptr_ == NULL because C++ does that for us.
  ~scoped_array() {
    enum { type_must_be_complete = sizeof(C) };
    delete[] array_;
  }

  // Reset.  Deletes the current owned object, if any.
  // Then takes ownership of a new object, if given.
  // this->reset(this->get()) works.
  void reset(C* p = NULL) {
    if (p != array_) {
      enum { type_must_be_complete = sizeof(C) };
      delete[] array_;
      array_ = p;
    }
  }

  // Get one element of the current object.
  // Will assert() if there is no current object, or index i is negative.
  C& operator[](ptrdiff_t i) const {
    assert(i >= 0);
    assert(array_ != NULL);
    return array_[i];
  }

  // Get a pointer to the zeroth element of the current object.
  // If there is no current object, return NULL.
  C* get() const {
    return array_;
  }

  // Comparison operators.
  // These return whether two scoped_array refer to the same object, not just to
  // two different but equal objects.
  bool operator==(C* p) const { return array_ == p; }
  bool operator!=(C* p) const { return array_ != p; }

  // Swap two scoped arrays.
  void swap(scoped_array& p2) {
    C* tmp = array_;
    array_ = p2.array_;
    p2.array_ = tmp;
  }

  // Release an array.
  // The return value is the current pointer held by this object.
  // If this object holds a NULL pointer, the return value is NULL.
  // After this operation, this object will hold a NULL pointer,
  // and will not own the object any more.
  C* release() WARN_UNUSED_RESULT {
    C* retVal = array_;
    array_ = NULL;
    return retVal;
  }

  CPP_03_MOVE_EMULATION(scoped_array, array_);

 private:
  C* array_;

  // Forbid comparison of different scoped_array types.
  template <class C2> bool operator==(scoped_array<C2> const& p2) const;
  template <class C2> bool operator!=(scoped_array<C2> const& p2) const;

  // Disallow evil constructors.  Note that MUST NOT take a const& because we
  // are implementing move semantics.  See the CPP_03_MOVE_EMULATION macro.
  scoped_array(scoped_array&);
  void operator=(scoped_array&);
};

// Free functions
template <class C>
void swap(scoped_array<C>& p1, scoped_array<C>& p2) {
  p1.swap(p2);
}

template <class C>
bool operator==(C* p1, const scoped_array<C>& p2) {
  return p1 == p2.get();
}

template <class C>
bool operator!=(C* p1, const scoped_array<C>& p2) {
  return p1 != p2.get();
}

// This class wraps the c library function free() in a class that can be
// passed as a template argument to scoped_ptr_malloc below.
class ScopedPtrMallocFree {
 public:
  inline void operator()(void* x) const {
    free(x);
  }
};

// scoped_ptr_malloc<> is similar to scoped_ptr<>, but it accepts a
// second template argument, the functor used to free the object.

template<class C, class FreeProc = ScopedPtrMallocFree>
class scoped_ptr_malloc {
 public:

  // The element type
  typedef C element_type;

  // Constructor.  Defaults to initializing with NULL.
  // There is no way to create an uninitialized scoped_ptr.
  // The input parameter must be allocated with an allocator that matches the
  // Free functor.  For the default Free functor, this is malloc, calloc, or
  // realloc.
  explicit scoped_ptr_malloc(C* p = NULL): ptr_(p) {}

  // Destructor.  If there is a C object, call the Free functor.
  ~scoped_ptr_malloc() {
    reset();
  }

  // Reset.  Calls the Free functor on the current owned object, if any.
  // Then takes ownership of a new object, if given.
  // this->reset(this->get()) works.
  void reset(C* p = NULL) {
    if (ptr_ != p) {
      FreeProc free_proc;
      free_proc(ptr_);
      ptr_ = p;
    }
  }

  // Get the current object.
  // operator* and operator-> will cause an assert() failure if there is
  // no current object.
  C& operator*() const {
    assert(ptr_ != NULL);
    return *ptr_;
  }

  C* operator->() const {
    assert(ptr_ != NULL);
    return ptr_;
  }

  C* get() const {
    return ptr_;
  }

  // Comparison operators.
  // These return whether a scoped_ptr_malloc and a plain pointer refer
  // to the same object, not just to two different but equal objects.
  // For compatibility with the boost-derived implementation, these
  // take non-const arguments.
  bool operator==(C* p) const {
    return ptr_ == p;
  }

  bool operator!=(C* p) const {
    return ptr_ != p;
  }

  // Swap two scoped pointers.
  void swap(scoped_ptr_malloc & b) {
    C* tmp = b.ptr_;
    b.ptr_ = ptr_;
    ptr_ = tmp;
  }

  // Release a pointer.
  // The return value is the current pointer held by this object.
  // If this object holds a NULL pointer, the return value is NULL.
  // After this operation, this object will hold a NULL pointer,
  // and will not own the object any more.
  C* release() WARN_UNUSED_RESULT {
    C* tmp = ptr_;
    ptr_ = NULL;
    return tmp;
  }

  CPP_03_MOVE_EMULATION(scoped_ptr_malloc, ptr_);

 private:
  C* ptr_;

  // no reason to use these: each scoped_ptr_malloc should have its own object
  template <class C2, class GP>
  bool operator==(scoped_ptr_malloc<C2, GP> const& p) const;
  template <class C2, class GP>
  bool operator!=(scoped_ptr_malloc<C2, GP> const& p) const;

  // Disallow evil constructors.  Note that MUST NOT take a const& because we
  // are implementing move semantics.  See the CPP_03_MOVE_EMULATION macro.
  scoped_ptr_malloc(scoped_ptr_malloc&);
  void operator=(scoped_ptr_malloc&);
};

#undef CPP_03_MOVE_EMULATION

template<class C, class FP> inline
void swap(scoped_ptr_malloc<C, FP>& a, scoped_ptr_malloc<C, FP>& b) {
  a.swap(b);
}

template<class C, class FP> inline
bool operator==(C* p, const scoped_ptr_malloc<C, FP>& b) {
  return p == b.get();
}

template<class C, class FP> inline
bool operator!=(C* p, const scoped_ptr_malloc<C, FP>& b) {
  return p != b.get();
}

#endif  // BASE_MEMORY_SCOPED_PTR_H_
