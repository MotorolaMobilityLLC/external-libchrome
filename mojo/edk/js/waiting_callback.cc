// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/js/waiting_callback.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "gin/per_context_data.h"

namespace mojo {
namespace edk {
namespace js {

namespace {

v8::Handle<v8::Private> GetHiddenPropertyName(v8::Isolate* isolate) {
  return v8::Private::ForApi(
      isolate, gin::StringToV8(isolate, "::mojo::js::WaitingCallback"));
}

}  // namespace

gin::WrapperInfo WaitingCallback::kWrapperInfo = { gin::kEmbedderNativeGin };

// static
gin::Handle<WaitingCallback> WaitingCallback::Create(
    v8::Isolate* isolate,
    v8::Handle<v8::Function> callback,
    gin::Handle<HandleWrapper> handle_wrapper,
    MojoHandleSignals signals) {
  gin::Handle<WaitingCallback> waiting_callback = gin::CreateHandle(
      isolate, new WaitingCallback(isolate, callback, handle_wrapper));

  waiting_callback->handle_watcher_.Start(
      handle_wrapper->get(), signals, MOJO_DEADLINE_INDEFINITE,
      base::Bind(&WaitingCallback::OnHandleReady,
                 base::Unretained(waiting_callback.get())));
  return waiting_callback;
}

void WaitingCallback::Cancel() {
  if (!handle_watcher_.is_watching())
    return;

  RemoveHandleCloseObserver();
  handle_watcher_.Stop();
}

WaitingCallback::WaitingCallback(v8::Isolate* isolate,
                                 v8::Handle<v8::Function> callback,
                                 gin::Handle<HandleWrapper> handle_wrapper)
    : handle_wrapper_(handle_wrapper.get()),
      weak_factory_(this) {
  handle_wrapper_->AddCloseObserver(this);
  v8::Handle<v8::Context> context = isolate->GetCurrentContext();
  runner_ = gin::PerContextData::From(context)->runner()->GetWeakPtr();
  GetWrapper(isolate)
      ->SetPrivate(context, GetHiddenPropertyName(isolate), callback)
      .FromJust();
}

WaitingCallback::~WaitingCallback() {
  Cancel();
}

void WaitingCallback::RemoveHandleCloseObserver() {
  handle_wrapper_->RemoveCloseObserver(this);
  handle_wrapper_ = nullptr;
}

void WaitingCallback::OnHandleReady(MojoResult result) {
  RemoveHandleCloseObserver();
  CallCallback(result);
}

void WaitingCallback::CallCallback(MojoResult result) {
  DCHECK(!handle_watcher_.is_watching());
  DCHECK(!handle_wrapper_);

  if (!runner_)
    return;

  gin::Runner::Scope scope(runner_.get());
  v8::Isolate* isolate = runner_->GetContextHolder()->isolate();

  v8::Handle<v8::Value> hidden_value =
      GetWrapper(isolate)
          ->GetPrivate(runner_->GetContextHolder()->context(),
                       GetHiddenPropertyName(isolate))
          .ToLocalChecked();
  v8::Handle<v8::Function> callback;
  CHECK(gin::ConvertFromV8(isolate, hidden_value, &callback));

  v8::Handle<v8::Value> args[] = { gin::ConvertToV8(isolate, result) };
  runner_->Call(callback, runner_->global(), 1, args);
}

void WaitingCallback::OnWillCloseHandle() {
  handle_watcher_.Stop();

  // This may be called from GC, so we can't execute Javascript now, call
  // RemoveHandleCloseObserver explicitly, and CallCallback asynchronously.
  RemoveHandleCloseObserver();
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&WaitingCallback::CallCallback, weak_factory_.GetWeakPtr(),
                 MOJO_RESULT_INVALID_ARGUMENT));
}

}  // namespace js
}  // namespace edk
}  // namespace mojo
