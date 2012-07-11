// This file was GENERATED by command:
//     pump.py bind.h.pump
// DO NOT EDIT BY HAND!!!


// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_BIND_H_
#define BASE_BIND_H_

#include "base/bind_internal.h"
#include "base/callback_internal.h"

// See base/callback.h for how to use these functions. If reading the
// implementation, before proceeding further, you should read the top
// comment of base/bind_internal.h for a definition of common terms and
// concepts.
//
// IMPLEMENTATION NOTE
// Though Bind()'s result is meant to be stored in a Callback<> type, it
// cannot actually return the exact type without requiring a large amount
// of extra template specializations. The problem is that in order to
// discern the correct specialization of Callback<>, Bind would need to
// unwrap the function signature to determine the signature's arity, and
// whether or not it is a method.
//
// Each unique combination of (arity, function_type, num_prebound) where
// function_type is one of {function, method, const_method} would require
// one specialization.  We eventually have to do a similar number of
// specializations anyways in the implementation (see the Invoker<>,
// classes).  However, it is avoidable in Bind if we return the result
// via an indirection like we do below.
//
// TODO(ajwong): We might be able to avoid this now, but need to test.
//
// It is possible to move most of the COMPILE_ASSERT asserts into BindState<>,
// but it feels a little nicer to have the asserts here so people do not
// need to crack open bind_internal.h.  On the other hand, it makes Bind()
// harder to read.

namespace base {

template <typename Functor>
base::Callback<
    typename internal::BindState<
        typename internal::FunctorTraits<Functor>::RunnableType,
        typename internal::FunctorTraits<Functor>::RunType,
        void()>
            ::UnboundRunType>
Bind(Functor functor) {
  // Typedefs for how to store and run the functor.
  typedef typename internal::FunctorTraits<Functor>::RunnableType RunnableType;
  typedef typename internal::FunctorTraits<Functor>::RunType RunType;

  // Use RunnableType::RunType instead of RunType above because our
  // checks should below for bound references need to know what the actual
  // functor is going to interpret the argument as.
  typedef internal::FunctionTraits<typename RunnableType::RunType>
      BoundFunctorTraits;

  typedef internal::BindState<RunnableType, RunType, void()> BindState;


  return Callback<typename BindState::UnboundRunType>(
      new BindState(internal::MakeRunnable(functor)));
}

template <typename Functor, typename P1>
base::Callback<
    typename internal::BindState<
        typename internal::FunctorTraits<Functor>::RunnableType,
        typename internal::FunctorTraits<Functor>::RunType,
        void(typename internal::CallbackParamTraits<P1>::StorageType)>
            ::UnboundRunType>
Bind(Functor functor, const P1& p1) {
  // Typedefs for how to store and run the functor.
  typedef typename internal::FunctorTraits<Functor>::RunnableType RunnableType;
  typedef typename internal::FunctorTraits<Functor>::RunType RunType;

  // Use RunnableType::RunType instead of RunType above because our
  // checks should below for bound references need to know what the actual
  // functor is going to interpret the argument as.
  typedef internal::FunctionTraits<typename RunnableType::RunType>
      BoundFunctorTraits;

  // Do not allow binding a non-const reference parameter. Non-const reference
  // parameters are disallowed by the Google style guide.  Also, binding a
  // non-const reference parameter can make for subtle bugs because the
  // invoked function will receive a reference to the stored copy of the
  // argument and not the original.
  COMPILE_ASSERT(
      !(is_non_const_reference<typename BoundFunctorTraits::A1Type>::value ),
      do_not_bind_functions_with_nonconst_ref);

  // For methods, we need to be careful for parameter 1.  We do not require
  // a scoped_refptr because BindState<> itself takes care of AddRef() for
  // methods. We also disallow binding of an array as the method's target
  // object.
  COMPILE_ASSERT(
      internal::HasIsMethodTag<RunnableType>::value ||
          !internal::NeedsScopedRefptrButGetsRawPtr<P1>::value,
      p1_is_refcounted_type_and_needs_scoped_refptr);
  COMPILE_ASSERT(!internal::HasIsMethodTag<RunnableType>::value ||
                     !is_array<P1>::value,
                 first_bound_argument_to_method_cannot_be_array);
  typedef internal::BindState<RunnableType, RunType,
      void(typename internal::CallbackParamTraits<P1>::StorageType)> BindState;


  return Callback<typename BindState::UnboundRunType>(
      new BindState(internal::MakeRunnable(functor), p1));
}

template <typename Functor, typename P1, typename P2>
base::Callback<
    typename internal::BindState<
        typename internal::FunctorTraits<Functor>::RunnableType,
        typename internal::FunctorTraits<Functor>::RunType,
        void(typename internal::CallbackParamTraits<P1>::StorageType,
            typename internal::CallbackParamTraits<P2>::StorageType)>
            ::UnboundRunType>
Bind(Functor functor, const P1& p1, const P2& p2) {
  // Typedefs for how to store and run the functor.
  typedef typename internal::FunctorTraits<Functor>::RunnableType RunnableType;
  typedef typename internal::FunctorTraits<Functor>::RunType RunType;

  // Use RunnableType::RunType instead of RunType above because our
  // checks should below for bound references need to know what the actual
  // functor is going to interpret the argument as.
  typedef internal::FunctionTraits<typename RunnableType::RunType>
      BoundFunctorTraits;

  // Do not allow binding a non-const reference parameter. Non-const reference
  // parameters are disallowed by the Google style guide.  Also, binding a
  // non-const reference parameter can make for subtle bugs because the
  // invoked function will receive a reference to the stored copy of the
  // argument and not the original.
  COMPILE_ASSERT(
      !(is_non_const_reference<typename BoundFunctorTraits::A1Type>::value ||
          is_non_const_reference<typename BoundFunctorTraits::A2Type>::value ),
      do_not_bind_functions_with_nonconst_ref);

  // For methods, we need to be careful for parameter 1.  We do not require
  // a scoped_refptr because BindState<> itself takes care of AddRef() for
  // methods. We also disallow binding of an array as the method's target
  // object.
  COMPILE_ASSERT(
      internal::HasIsMethodTag<RunnableType>::value ||
          !internal::NeedsScopedRefptrButGetsRawPtr<P1>::value,
      p1_is_refcounted_type_and_needs_scoped_refptr);
  COMPILE_ASSERT(!internal::HasIsMethodTag<RunnableType>::value ||
                     !is_array<P1>::value,
                 first_bound_argument_to_method_cannot_be_array);
  COMPILE_ASSERT(!internal::NeedsScopedRefptrButGetsRawPtr<P2>::value,
                 p2_is_refcounted_type_and_needs_scoped_refptr);
  typedef internal::BindState<RunnableType, RunType,
      void(typename internal::CallbackParamTraits<P1>::StorageType,
      typename internal::CallbackParamTraits<P2>::StorageType)> BindState;


  return Callback<typename BindState::UnboundRunType>(
      new BindState(internal::MakeRunnable(functor), p1, p2));
}

template <typename Functor, typename P1, typename P2, typename P3>
base::Callback<
    typename internal::BindState<
        typename internal::FunctorTraits<Functor>::RunnableType,
        typename internal::FunctorTraits<Functor>::RunType,
        void(typename internal::CallbackParamTraits<P1>::StorageType,
            typename internal::CallbackParamTraits<P2>::StorageType,
            typename internal::CallbackParamTraits<P3>::StorageType)>
            ::UnboundRunType>
Bind(Functor functor, const P1& p1, const P2& p2, const P3& p3) {
  // Typedefs for how to store and run the functor.
  typedef typename internal::FunctorTraits<Functor>::RunnableType RunnableType;
  typedef typename internal::FunctorTraits<Functor>::RunType RunType;

  // Use RunnableType::RunType instead of RunType above because our
  // checks should below for bound references need to know what the actual
  // functor is going to interpret the argument as.
  typedef internal::FunctionTraits<typename RunnableType::RunType>
      BoundFunctorTraits;

  // Do not allow binding a non-const reference parameter. Non-const reference
  // parameters are disallowed by the Google style guide.  Also, binding a
  // non-const reference parameter can make for subtle bugs because the
  // invoked function will receive a reference to the stored copy of the
  // argument and not the original.
  COMPILE_ASSERT(
      !(is_non_const_reference<typename BoundFunctorTraits::A1Type>::value ||
          is_non_const_reference<typename BoundFunctorTraits::A2Type>::value ||
          is_non_const_reference<typename BoundFunctorTraits::A3Type>::value ),
      do_not_bind_functions_with_nonconst_ref);

  // For methods, we need to be careful for parameter 1.  We do not require
  // a scoped_refptr because BindState<> itself takes care of AddRef() for
  // methods. We also disallow binding of an array as the method's target
  // object.
  COMPILE_ASSERT(
      internal::HasIsMethodTag<RunnableType>::value ||
          !internal::NeedsScopedRefptrButGetsRawPtr<P1>::value,
      p1_is_refcounted_type_and_needs_scoped_refptr);
  COMPILE_ASSERT(!internal::HasIsMethodTag<RunnableType>::value ||
                     !is_array<P1>::value,
                 first_bound_argument_to_method_cannot_be_array);
  COMPILE_ASSERT(!internal::NeedsScopedRefptrButGetsRawPtr<P2>::value,
                 p2_is_refcounted_type_and_needs_scoped_refptr);
  COMPILE_ASSERT(!internal::NeedsScopedRefptrButGetsRawPtr<P3>::value,
                 p3_is_refcounted_type_and_needs_scoped_refptr);
  typedef internal::BindState<RunnableType, RunType,
      void(typename internal::CallbackParamTraits<P1>::StorageType,
      typename internal::CallbackParamTraits<P2>::StorageType,
      typename internal::CallbackParamTraits<P3>::StorageType)> BindState;


  return Callback<typename BindState::UnboundRunType>(
      new BindState(internal::MakeRunnable(functor), p1, p2, p3));
}

template <typename Functor, typename P1, typename P2, typename P3, typename P4>
base::Callback<
    typename internal::BindState<
        typename internal::FunctorTraits<Functor>::RunnableType,
        typename internal::FunctorTraits<Functor>::RunType,
        void(typename internal::CallbackParamTraits<P1>::StorageType,
            typename internal::CallbackParamTraits<P2>::StorageType,
            typename internal::CallbackParamTraits<P3>::StorageType,
            typename internal::CallbackParamTraits<P4>::StorageType)>
            ::UnboundRunType>
Bind(Functor functor, const P1& p1, const P2& p2, const P3& p3, const P4& p4) {
  // Typedefs for how to store and run the functor.
  typedef typename internal::FunctorTraits<Functor>::RunnableType RunnableType;
  typedef typename internal::FunctorTraits<Functor>::RunType RunType;

  // Use RunnableType::RunType instead of RunType above because our
  // checks should below for bound references need to know what the actual
  // functor is going to interpret the argument as.
  typedef internal::FunctionTraits<typename RunnableType::RunType>
      BoundFunctorTraits;

  // Do not allow binding a non-const reference parameter. Non-const reference
  // parameters are disallowed by the Google style guide.  Also, binding a
  // non-const reference parameter can make for subtle bugs because the
  // invoked function will receive a reference to the stored copy of the
  // argument and not the original.
  COMPILE_ASSERT(
      !(is_non_const_reference<typename BoundFunctorTraits::A1Type>::value ||
          is_non_const_reference<typename BoundFunctorTraits::A2Type>::value ||
          is_non_const_reference<typename BoundFunctorTraits::A3Type>::value ||
          is_non_const_reference<typename BoundFunctorTraits::A4Type>::value ),
      do_not_bind_functions_with_nonconst_ref);

  // For methods, we need to be careful for parameter 1.  We do not require
  // a scoped_refptr because BindState<> itself takes care of AddRef() for
  // methods. We also disallow binding of an array as the method's target
  // object.
  COMPILE_ASSERT(
      internal::HasIsMethodTag<RunnableType>::value ||
          !internal::NeedsScopedRefptrButGetsRawPtr<P1>::value,
      p1_is_refcounted_type_and_needs_scoped_refptr);
  COMPILE_ASSERT(!internal::HasIsMethodTag<RunnableType>::value ||
                     !is_array<P1>::value,
                 first_bound_argument_to_method_cannot_be_array);
  COMPILE_ASSERT(!internal::NeedsScopedRefptrButGetsRawPtr<P2>::value,
                 p2_is_refcounted_type_and_needs_scoped_refptr);
  COMPILE_ASSERT(!internal::NeedsScopedRefptrButGetsRawPtr<P3>::value,
                 p3_is_refcounted_type_and_needs_scoped_refptr);
  COMPILE_ASSERT(!internal::NeedsScopedRefptrButGetsRawPtr<P4>::value,
                 p4_is_refcounted_type_and_needs_scoped_refptr);
  typedef internal::BindState<RunnableType, RunType,
      void(typename internal::CallbackParamTraits<P1>::StorageType,
      typename internal::CallbackParamTraits<P2>::StorageType,
      typename internal::CallbackParamTraits<P3>::StorageType,
      typename internal::CallbackParamTraits<P4>::StorageType)> BindState;


  return Callback<typename BindState::UnboundRunType>(
      new BindState(internal::MakeRunnable(functor), p1, p2, p3, p4));
}

template <typename Functor, typename P1, typename P2, typename P3, typename P4,
    typename P5>
base::Callback<
    typename internal::BindState<
        typename internal::FunctorTraits<Functor>::RunnableType,
        typename internal::FunctorTraits<Functor>::RunType,
        void(typename internal::CallbackParamTraits<P1>::StorageType,
            typename internal::CallbackParamTraits<P2>::StorageType,
            typename internal::CallbackParamTraits<P3>::StorageType,
            typename internal::CallbackParamTraits<P4>::StorageType,
            typename internal::CallbackParamTraits<P5>::StorageType)>
            ::UnboundRunType>
Bind(Functor functor, const P1& p1, const P2& p2, const P3& p3, const P4& p4,
    const P5& p5) {
  // Typedefs for how to store and run the functor.
  typedef typename internal::FunctorTraits<Functor>::RunnableType RunnableType;
  typedef typename internal::FunctorTraits<Functor>::RunType RunType;

  // Use RunnableType::RunType instead of RunType above because our
  // checks should below for bound references need to know what the actual
  // functor is going to interpret the argument as.
  typedef internal::FunctionTraits<typename RunnableType::RunType>
      BoundFunctorTraits;

  // Do not allow binding a non-const reference parameter. Non-const reference
  // parameters are disallowed by the Google style guide.  Also, binding a
  // non-const reference parameter can make for subtle bugs because the
  // invoked function will receive a reference to the stored copy of the
  // argument and not the original.
  COMPILE_ASSERT(
      !(is_non_const_reference<typename BoundFunctorTraits::A1Type>::value ||
          is_non_const_reference<typename BoundFunctorTraits::A2Type>::value ||
          is_non_const_reference<typename BoundFunctorTraits::A3Type>::value ||
          is_non_const_reference<typename BoundFunctorTraits::A4Type>::value ||
          is_non_const_reference<typename BoundFunctorTraits::A5Type>::value ),
      do_not_bind_functions_with_nonconst_ref);

  // For methods, we need to be careful for parameter 1.  We do not require
  // a scoped_refptr because BindState<> itself takes care of AddRef() for
  // methods. We also disallow binding of an array as the method's target
  // object.
  COMPILE_ASSERT(
      internal::HasIsMethodTag<RunnableType>::value ||
          !internal::NeedsScopedRefptrButGetsRawPtr<P1>::value,
      p1_is_refcounted_type_and_needs_scoped_refptr);
  COMPILE_ASSERT(!internal::HasIsMethodTag<RunnableType>::value ||
                     !is_array<P1>::value,
                 first_bound_argument_to_method_cannot_be_array);
  COMPILE_ASSERT(!internal::NeedsScopedRefptrButGetsRawPtr<P2>::value,
                 p2_is_refcounted_type_and_needs_scoped_refptr);
  COMPILE_ASSERT(!internal::NeedsScopedRefptrButGetsRawPtr<P3>::value,
                 p3_is_refcounted_type_and_needs_scoped_refptr);
  COMPILE_ASSERT(!internal::NeedsScopedRefptrButGetsRawPtr<P4>::value,
                 p4_is_refcounted_type_and_needs_scoped_refptr);
  COMPILE_ASSERT(!internal::NeedsScopedRefptrButGetsRawPtr<P5>::value,
                 p5_is_refcounted_type_and_needs_scoped_refptr);
  typedef internal::BindState<RunnableType, RunType,
      void(typename internal::CallbackParamTraits<P1>::StorageType,
      typename internal::CallbackParamTraits<P2>::StorageType,
      typename internal::CallbackParamTraits<P3>::StorageType,
      typename internal::CallbackParamTraits<P4>::StorageType,
      typename internal::CallbackParamTraits<P5>::StorageType)> BindState;


  return Callback<typename BindState::UnboundRunType>(
      new BindState(internal::MakeRunnable(functor), p1, p2, p3, p4, p5));
}

template <typename Functor, typename P1, typename P2, typename P3, typename P4,
    typename P5, typename P6>
base::Callback<
    typename internal::BindState<
        typename internal::FunctorTraits<Functor>::RunnableType,
        typename internal::FunctorTraits<Functor>::RunType,
        void(typename internal::CallbackParamTraits<P1>::StorageType,
            typename internal::CallbackParamTraits<P2>::StorageType,
            typename internal::CallbackParamTraits<P3>::StorageType,
            typename internal::CallbackParamTraits<P4>::StorageType,
            typename internal::CallbackParamTraits<P5>::StorageType,
            typename internal::CallbackParamTraits<P6>::StorageType)>
            ::UnboundRunType>
Bind(Functor functor, const P1& p1, const P2& p2, const P3& p3, const P4& p4,
    const P5& p5, const P6& p6) {
  // Typedefs for how to store and run the functor.
  typedef typename internal::FunctorTraits<Functor>::RunnableType RunnableType;
  typedef typename internal::FunctorTraits<Functor>::RunType RunType;

  // Use RunnableType::RunType instead of RunType above because our
  // checks should below for bound references need to know what the actual
  // functor is going to interpret the argument as.
  typedef internal::FunctionTraits<typename RunnableType::RunType>
      BoundFunctorTraits;

  // Do not allow binding a non-const reference parameter. Non-const reference
  // parameters are disallowed by the Google style guide.  Also, binding a
  // non-const reference parameter can make for subtle bugs because the
  // invoked function will receive a reference to the stored copy of the
  // argument and not the original.
  COMPILE_ASSERT(
      !(is_non_const_reference<typename BoundFunctorTraits::A1Type>::value ||
          is_non_const_reference<typename BoundFunctorTraits::A2Type>::value ||
          is_non_const_reference<typename BoundFunctorTraits::A3Type>::value ||
          is_non_const_reference<typename BoundFunctorTraits::A4Type>::value ||
          is_non_const_reference<typename BoundFunctorTraits::A5Type>::value ||
          is_non_const_reference<typename BoundFunctorTraits::A6Type>::value ),
      do_not_bind_functions_with_nonconst_ref);

  // For methods, we need to be careful for parameter 1.  We do not require
  // a scoped_refptr because BindState<> itself takes care of AddRef() for
  // methods. We also disallow binding of an array as the method's target
  // object.
  COMPILE_ASSERT(
      internal::HasIsMethodTag<RunnableType>::value ||
          !internal::NeedsScopedRefptrButGetsRawPtr<P1>::value,
      p1_is_refcounted_type_and_needs_scoped_refptr);
  COMPILE_ASSERT(!internal::HasIsMethodTag<RunnableType>::value ||
                     !is_array<P1>::value,
                 first_bound_argument_to_method_cannot_be_array);
  COMPILE_ASSERT(!internal::NeedsScopedRefptrButGetsRawPtr<P2>::value,
                 p2_is_refcounted_type_and_needs_scoped_refptr);
  COMPILE_ASSERT(!internal::NeedsScopedRefptrButGetsRawPtr<P3>::value,
                 p3_is_refcounted_type_and_needs_scoped_refptr);
  COMPILE_ASSERT(!internal::NeedsScopedRefptrButGetsRawPtr<P4>::value,
                 p4_is_refcounted_type_and_needs_scoped_refptr);
  COMPILE_ASSERT(!internal::NeedsScopedRefptrButGetsRawPtr<P5>::value,
                 p5_is_refcounted_type_and_needs_scoped_refptr);
  COMPILE_ASSERT(!internal::NeedsScopedRefptrButGetsRawPtr<P6>::value,
                 p6_is_refcounted_type_and_needs_scoped_refptr);
  typedef internal::BindState<RunnableType, RunType,
      void(typename internal::CallbackParamTraits<P1>::StorageType,
      typename internal::CallbackParamTraits<P2>::StorageType,
      typename internal::CallbackParamTraits<P3>::StorageType,
      typename internal::CallbackParamTraits<P4>::StorageType,
      typename internal::CallbackParamTraits<P5>::StorageType,
      typename internal::CallbackParamTraits<P6>::StorageType)> BindState;


  return Callback<typename BindState::UnboundRunType>(
      new BindState(internal::MakeRunnable(functor), p1, p2, p3, p4, p5, p6));
}

template <typename Functor, typename P1, typename P2, typename P3, typename P4,
    typename P5, typename P6, typename P7>
base::Callback<
    typename internal::BindState<
        typename internal::FunctorTraits<Functor>::RunnableType,
        typename internal::FunctorTraits<Functor>::RunType,
        void(typename internal::CallbackParamTraits<P1>::StorageType,
            typename internal::CallbackParamTraits<P2>::StorageType,
            typename internal::CallbackParamTraits<P3>::StorageType,
            typename internal::CallbackParamTraits<P4>::StorageType,
            typename internal::CallbackParamTraits<P5>::StorageType,
            typename internal::CallbackParamTraits<P6>::StorageType,
            typename internal::CallbackParamTraits<P7>::StorageType)>
            ::UnboundRunType>
Bind(Functor functor, const P1& p1, const P2& p2, const P3& p3, const P4& p4,
    const P5& p5, const P6& p6, const P7& p7) {
  // Typedefs for how to store and run the functor.
  typedef typename internal::FunctorTraits<Functor>::RunnableType RunnableType;
  typedef typename internal::FunctorTraits<Functor>::RunType RunType;

  // Use RunnableType::RunType instead of RunType above because our
  // checks should below for bound references need to know what the actual
  // functor is going to interpret the argument as.
  typedef internal::FunctionTraits<typename RunnableType::RunType>
      BoundFunctorTraits;

  // Do not allow binding a non-const reference parameter. Non-const reference
  // parameters are disallowed by the Google style guide.  Also, binding a
  // non-const reference parameter can make for subtle bugs because the
  // invoked function will receive a reference to the stored copy of the
  // argument and not the original.
  COMPILE_ASSERT(
      !(is_non_const_reference<typename BoundFunctorTraits::A1Type>::value ||
          is_non_const_reference<typename BoundFunctorTraits::A2Type>::value ||
          is_non_const_reference<typename BoundFunctorTraits::A3Type>::value ||
          is_non_const_reference<typename BoundFunctorTraits::A4Type>::value ||
          is_non_const_reference<typename BoundFunctorTraits::A5Type>::value ||
          is_non_const_reference<typename BoundFunctorTraits::A6Type>::value ||
          is_non_const_reference<typename BoundFunctorTraits::A7Type>::value ),
      do_not_bind_functions_with_nonconst_ref);

  // For methods, we need to be careful for parameter 1.  We do not require
  // a scoped_refptr because BindState<> itself takes care of AddRef() for
  // methods. We also disallow binding of an array as the method's target
  // object.
  COMPILE_ASSERT(
      internal::HasIsMethodTag<RunnableType>::value ||
          !internal::NeedsScopedRefptrButGetsRawPtr<P1>::value,
      p1_is_refcounted_type_and_needs_scoped_refptr);
  COMPILE_ASSERT(!internal::HasIsMethodTag<RunnableType>::value ||
                     !is_array<P1>::value,
                 first_bound_argument_to_method_cannot_be_array);
  COMPILE_ASSERT(!internal::NeedsScopedRefptrButGetsRawPtr<P2>::value,
                 p2_is_refcounted_type_and_needs_scoped_refptr);
  COMPILE_ASSERT(!internal::NeedsScopedRefptrButGetsRawPtr<P3>::value,
                 p3_is_refcounted_type_and_needs_scoped_refptr);
  COMPILE_ASSERT(!internal::NeedsScopedRefptrButGetsRawPtr<P4>::value,
                 p4_is_refcounted_type_and_needs_scoped_refptr);
  COMPILE_ASSERT(!internal::NeedsScopedRefptrButGetsRawPtr<P5>::value,
                 p5_is_refcounted_type_and_needs_scoped_refptr);
  COMPILE_ASSERT(!internal::NeedsScopedRefptrButGetsRawPtr<P6>::value,
                 p6_is_refcounted_type_and_needs_scoped_refptr);
  COMPILE_ASSERT(!internal::NeedsScopedRefptrButGetsRawPtr<P7>::value,
                 p7_is_refcounted_type_and_needs_scoped_refptr);
  typedef internal::BindState<RunnableType, RunType,
      void(typename internal::CallbackParamTraits<P1>::StorageType,
      typename internal::CallbackParamTraits<P2>::StorageType,
      typename internal::CallbackParamTraits<P3>::StorageType,
      typename internal::CallbackParamTraits<P4>::StorageType,
      typename internal::CallbackParamTraits<P5>::StorageType,
      typename internal::CallbackParamTraits<P6>::StorageType,
      typename internal::CallbackParamTraits<P7>::StorageType)> BindState;


  return Callback<typename BindState::UnboundRunType>(
      new BindState(internal::MakeRunnable(functor), p1, p2, p3, p4, p5, p6,
          p7));
}

}  // namespace base

#endif  // BASE_BIND_H_
