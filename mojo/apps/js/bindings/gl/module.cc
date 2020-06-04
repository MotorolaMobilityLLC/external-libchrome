// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/apps/js/bindings/gl/module.h"

#include "base/logging.h"
#include "gin/arguments.h"
#include "gin/object_template_builder.h"
#include "gin/per_isolate_data.h"
#include "gin/wrappable.h"
#include "mojo/apps/js/bindings/gl/context.h"
#include "mojo/apps/js/bindings/handle.h"

namespace mojo {
namespace js {
namespace gl {

const char* kModuleName = "mojo/apps/js/bindings/gl";

namespace {

gin::WrapperInfo kWrapperInfo = { gin::kEmbedderNativeGin };

gin::Handle<Context> CreateContext(
    const gin::Arguments& args,
    mojo::Handle handle,
    v8::Handle<v8::Function> did_create_callback) {
  return Context::Create(args.isolate(), handle, did_create_callback);
}

}  // namespace

v8::Local<v8::Value> GetModule(v8::Isolate* isolate) {
  gin::PerIsolateData* data = gin::PerIsolateData::From(isolate);
  v8::Local<v8::ObjectTemplate> templ = data->GetObjectTemplate(&kWrapperInfo);

  if (templ.IsEmpty()) {
    templ = gin::ObjectTemplateBuilder(isolate)
        .SetMethod("Context", CreateContext)
        .Build();
    data->SetObjectTemplate(&kWrapperInfo, templ);
  }

  return templ->NewInstance();
}

}  // namespace gl
}  // namespace js
}  // namespace mojo
