// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/apps/js/bindings/gl/context.h"

#include <GLES2/gl2.h>

#include "gin/arguments.h"
#include "gin/array_buffer.h"
#include "gin/object_template_builder.h"
#include "gin/per_context_data.h"
#include "mojo/public/gles2/gles2.h"

namespace gin {
template<>
struct Converter<GLboolean> {
  static bool FromV8(v8::Isolate* isolate, v8::Handle<v8::Value> val,
                     GLboolean* out) {
    bool bool_val = false;
    if (!Converter<bool>::FromV8(isolate, val, &bool_val))
      return false;
    *out = static_cast<GLboolean>(bool_val);
    return true;
  }
};
}

namespace mojo {
namespace js {
namespace gl {

gin::WrapperInfo Context::kWrapperInfo = { gin::kEmbedderNativeGin };

gin::Handle<Context> Context::Create(
    v8::Isolate* isolate,
    mojo::Handle handle,
    v8::Handle<v8::Function> did_create_callback) {
  return gin::CreateHandle(isolate,
                           new Context(isolate, handle, did_create_callback));
}

void Context::BufferData(GLenum target, const gin::ArrayBufferView& buffer,
                         GLenum usage) {
  glBufferData(target, static_cast<GLsizeiptr>(buffer.num_bytes()),
               buffer.bytes(), usage);
}

void Context::CompileShader(const gin::Arguments& args, GLuint shader) {
  glCompileShader(shader);
  GLint compiled = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (!compiled) {
    args.ThrowTypeError(std::string("Could not compile shader: ") +
                        GetShaderInfoLog(shader));
  }
}

GLuint Context::CreateBuffer() {
  GLuint result = 0;
  glGenBuffers(1, &result);
  return result;
}

void Context::DrawElements(GLenum mode, GLsizei count, GLenum type,
                           uint64_t indices) {
  // This looks scary, but it's what WebGL does too:
  // http://www.khronos.org/registry/webgl/specs/latest/1.0/#5.1
  glDrawElements(mode, count, type, reinterpret_cast<void*>(indices));
}

GLint Context::GetAttribLocation(GLuint program, const std::string& name) {
  return glGetAttribLocation(program, name.c_str());
}

std::string Context::GetProgramInfoLog(GLuint program) {
  GLint info_log_length = 0;
  glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length);
  std::string info_log(info_log_length, 0);
  glGetProgramInfoLog(program, info_log_length, NULL, &info_log.at(0));
  return info_log;
}

std::string Context::GetShaderInfoLog(GLuint shader) {
  GLint info_log_length = 0;
  glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_length);
  std::string info_log(info_log_length, 0);
  glGetShaderInfoLog(shader, info_log_length, NULL, &info_log.at(0));
  return info_log;
}

GLint Context::GetUniformLocation(GLuint program, const std::string& name) {
  return glGetUniformLocation(program, name.c_str());
}

void Context::ShaderSource(GLuint shader, const std::string& source) {
  const char* source_chars = source.c_str();
  glShaderSource(shader, 1, &source_chars, NULL);
}

void Context::UniformMatrix4fv(GLint location, GLboolean transpose,
                               const gin::ArrayBufferView& buffer) {
  glUniformMatrix4fv(location, 1, transpose,
                     static_cast<float*>(buffer.bytes()));
}

void Context::VertexAttribPointer(GLuint index, GLint size, GLenum type,
                                  GLboolean normalized, GLsizei stride,
                                  uint64_t offset) {
  glVertexAttribPointer(index, size, type, normalized, stride,
                        reinterpret_cast<void*>(offset));
}

gin::ObjectTemplateBuilder Context::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::ObjectTemplateBuilder(isolate)
      .SetValue("ARRAY_BUFFER", GL_ARRAY_BUFFER)
      .SetValue("COLOR_BUFFER_BIT", GL_COLOR_BUFFER_BIT)
      .SetValue("ELEMENT_ARRAY_BUFFER", GL_ELEMENT_ARRAY_BUFFER)
      .SetValue("FLOAT", GL_FLOAT)
      .SetValue("FRAGMENT_SHADER", GL_FRAGMENT_SHADER)
      .SetValue("STATIC_DRAW", GL_STATIC_DRAW)
      .SetValue("TRIANGLES", GL_TRIANGLES)
      .SetValue("UNSIGNED_SHORT", GL_UNSIGNED_SHORT)
      .SetValue("VERTEX_SHADER", GL_VERTEX_SHADER)
      .SetMethod("attachShader", glAttachShader)
      .SetMethod("bindBuffer", glBindBuffer)
      .SetMethod("bufferData", BufferData)
      .SetMethod("clear", glClear)
      .SetMethod("clearColor", glClearColor)
      .SetMethod("compileShader", CompileShader)
      .SetMethod("createBuffer", CreateBuffer)
      .SetMethod("createProgram", glCreateProgram)
      .SetMethod("createShader", glCreateShader)
      .SetMethod("deleteShader", glDeleteShader)
      .SetMethod("drawElements", DrawElements)
      .SetMethod("enableVertexAttribArray", glEnableVertexAttribArray)
      .SetMethod("getAttribLocation", GetAttribLocation)
      .SetMethod("getProgramInfoLog", GetProgramInfoLog)
      .SetMethod("getShaderInfoLog", GetShaderInfoLog)
      .SetMethod("getUniformLocation", GetUniformLocation)
      .SetMethod("linkProgram", glLinkProgram)
      .SetMethod("shaderSource", ShaderSource)
      .SetMethod("swapBuffers", MojoGLES2SwapBuffers)
      .SetMethod("uniformMatrix4fv", UniformMatrix4fv)
      .SetMethod("useProgram", glUseProgram)
      .SetMethod("vertexAttribPointer", VertexAttribPointer)
      .SetMethod("viewport", glViewport);
}

Context::Context(v8::Isolate* isolate,
                 mojo::Handle handle,
                 v8::Handle<v8::Function> did_create_callback) {
  v8::Handle<v8::Context> context = isolate->GetCurrentContext();
  runner_ = gin::PerContextData::From(context)->runner()->GetWeakPtr();
  did_create_callback_.Reset(isolate, did_create_callback);
  context_ = MojoGLES2CreateContext(
      handle.value(),
      &DidCreateContextThunk,
      &ContextLostThunk,
      NULL,
      this);
}

Context::~Context() {
  MojoGLES2DestroyContext(context_);
}

void Context::DidCreateContext(uint32_t width, uint32_t height) {
  // TODO(aa): When we want to support multiple contexts, we should add
  // Context::MakeCurrent() for developers to switch between them.
  MojoGLES2MakeCurrent(context_);
  if (!runner_)
    return;
  gin::Runner::Scope scope(runner_.get());
  v8::Isolate* isolate = runner_->isolate();

  v8::Handle<v8::Function> callback = v8::Local<v8::Function>::New(
      isolate, did_create_callback_);

  v8::Handle<v8::Value> args[] = {
    gin::ConvertToV8(isolate, width),
    gin::ConvertToV8(isolate, height),
  };
  runner_->Call(callback, runner_->global(), 2, args);
}

void Context::DidCreateContextThunk(
    void* closure,
    uint32_t width,
    uint32_t height) {
  static_cast<Context*>(closure)->DidCreateContext(width, height);
}

void Context::ContextLost() {
}

void Context::ContextLostThunk(void* closure) {
  static_cast<Context*>(closure)->ContextLost();
}

}  // namespace gl
}  // namespace js
}  // namespace mojo
