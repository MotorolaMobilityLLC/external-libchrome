// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojom/sample_service.h"

#include "mojo/public/bindings/lib/message_builder.h"
#include "mojom/sample_service.h"

namespace sample {
namespace internal {
namespace {
const uint32_t kService_Frobinate_Name = 1;

#pragma pack(push, 1)
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-private-field"
#endif
class Service_Frobinate_Params {
 public:
  static Service_Frobinate_Params* New(mojo::Buffer* buf) {
    return new (buf->Allocate(sizeof(Service_Frobinate_Params)))
        Service_Frobinate_Params();
  }

  void set_foo(Foo_Data* foo) { foo_.ptr = foo; }
  void set_baz(bool baz) { baz_ = baz; }
  void set_port(mojo::Handle port) { port_ = port; }

  const Foo_Data* foo() const { return foo_.ptr; }
  bool baz() const { return baz_; }
  mojo::Handle port() const {
    // NOTE: port is an optional field!
    return _header_.num_fields >= 3 ? port_ : mojo::Handle();
  }

 private:
  friend class mojo::internal::ObjectTraits<Service_Frobinate_Params>;

  Service_Frobinate_Params() {
    _header_.num_bytes = sizeof(*this);
    _header_.num_fields = 3;
  }

  mojo::internal::StructHeader _header_;
  mojo::internal::StructPointer<Foo_Data> foo_;
  uint8_t baz_ : 1;
  uint8_t _pad0_[3];
  mojo::Handle port_;
};
MOJO_COMPILE_ASSERT(sizeof(Service_Frobinate_Params) == 24,
                    bad_sizeof_Service_Frobinate_Params);

const uint32_t kServiceClient_DidFrobinate_Name = 0;

class ServiceClient_DidFrobinate_Params {
 public:
  static ServiceClient_DidFrobinate_Params* New(mojo::Buffer* buf) {
    return new (buf->Allocate(sizeof(ServiceClient_DidFrobinate_Params)))
        ServiceClient_DidFrobinate_Params();
  }

  void set_result(int32_t result) { result_ = result; }

  int32_t result() const { return result_; }

 private:
  friend class mojo::internal::ObjectTraits<ServiceClient_DidFrobinate_Params>;

  ServiceClient_DidFrobinate_Params() {
    _header_.num_bytes = sizeof(*this);
    _header_.num_fields = 3;
  }

  mojo::internal::StructHeader _header_;
  int32_t result_;
  uint8_t _pad0_[4];
};
MOJO_COMPILE_ASSERT(sizeof(ServiceClient_DidFrobinate_Params) == 16,
                    bad_sizeof_ServiceClient_DidFrobinate_Params);

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#pragma pack(pop)

}  // namespace

// static
Bar_Data* Bar_Data::New(mojo::Buffer* buf) {
  return new (buf->Allocate(sizeof(Bar_Data))) Bar_Data();
}

Bar_Data::Bar_Data() {
  _header_.num_bytes = sizeof(*this);
  _header_.num_fields = 3;
}

// static
Foo_Data* Foo_Data::New(mojo::Buffer* buf) {
  return new (buf->Allocate(sizeof(Foo_Data))) Foo_Data();
}

Foo_Data::Foo_Data() {
  _header_.num_bytes = sizeof(*this);
  _header_.num_fields = 10;
}

}  // namespace internal

Bar::Builder::Builder(mojo::Buffer* buf)
    : data_(Data::New(buf)) {
}

Bar Bar::Builder::Finish() {
  Data* data = NULL;
  std::swap(data, data_);
  return Bar(data);
}

Foo::Builder::Builder(mojo::Buffer* buf)
    : data_(Data::New(buf)) {
}

Foo Foo::Builder::Finish() {
  Data* data = NULL;
  std::swap(data, data_);
  return Foo(data);
}

ServiceProxy::ServiceProxy(mojo::MessageReceiver* receiver)
    : receiver_(receiver) {
}

void ServiceProxy::Frobinate(const Foo& foo, bool baz, mojo::Handle port) {
  size_t payload_size =
      mojo::internal::Align(sizeof(internal::Service_Frobinate_Params));
  payload_size +=
      mojo::internal::ComputeSizeOf(mojo::internal::Unwrap(foo));

  mojo::MessageBuilder builder(internal::kService_Frobinate_Name, payload_size);

  // We now go about allocating the anonymous Frobinate_Params struct.  It
  // holds the parameters to the Frobinate message.
  //
  // Notice how foo is cloned. This causes a copy of foo to be generated
  // within the same buffer as the Frobinate_Params struct. That's what we
  // need in order to generate a contiguous blob of message data.

  internal::Service_Frobinate_Params* params =
      internal::Service_Frobinate_Params::New(builder.buffer());
  params->set_foo(
      mojo::internal::Clone(mojo::internal::Unwrap(foo), builder.buffer()));
  params->set_baz(baz);
  params->set_port(port);

  // NOTE: If foo happened to be a graph with cycles, then Clone would not
  // have returned.

  // Next step is to encode pointers and handles so that messages become
  // hermetic. Pointers become offsets and handles becomes indices into the
  // handles array.
  mojo::Message message;
  mojo::internal::EncodePointersAndHandles(params, &message.handles);

  // Finally, we get the generated message data, and forward it to the
  // receiver.
  message.data = builder.Finish();

  receiver_->Accept(&message);
}

bool ServiceStub::Accept(mojo::Message* message) {
  switch (message->data->header.name) {
    case internal::kService_Frobinate_Name: {
      internal::Service_Frobinate_Params* params =
          reinterpret_cast<internal::Service_Frobinate_Params*>(
              message->data->payload);

      if (!mojo::internal::DecodePointersAndHandles(params, *message))
        return false;

      Frobinate(mojo::internal::Wrap(params->foo()),
                params->baz(),
                params->port());
      break;
    }
  }
  return true;
}

ServiceClientProxy::ServiceClientProxy(mojo::MessageReceiver* receiver)
    : receiver_(receiver) {
}

void ServiceClientProxy::DidFrobinate(int32_t result) {
  size_t payload_size = mojo::internal::Align(
      sizeof(internal::ServiceClient_DidFrobinate_Params));

  mojo::MessageBuilder builder(internal::kServiceClient_DidFrobinate_Name,
                               payload_size);

  internal::ServiceClient_DidFrobinate_Params* params =
      internal::ServiceClient_DidFrobinate_Params::New(builder.buffer());

  params->set_result(result);

  mojo::Message message;
  mojo::internal::EncodePointersAndHandles(params, &message.handles);

  message.data = builder.Finish();

  receiver_->Accept(&message);
}


bool ServiceClientStub::Accept(mojo::Message* message) {
  switch (message->data->header.name) {
    case internal::kServiceClient_DidFrobinate_Name: {
      internal::ServiceClient_DidFrobinate_Params* params =
          reinterpret_cast<internal::ServiceClient_DidFrobinate_Params*>(
              message->data->payload);

      if (!mojo::internal::DecodePointersAndHandles(params, *message))
        return false;
      DidFrobinate(params->result());
      break;
    }

  }
  return true;
}

}  // namespace sample

namespace mojo {
namespace internal {

// static
size_t ObjectTraits<sample::internal::Bar_Data>::ComputeSizeOf(
    const sample::internal::Bar_Data* bar) {
  return sizeof(*bar);
}

// static
sample::internal::Bar_Data* ObjectTraits<sample::internal::Bar_Data>::Clone(
    const sample::internal::Bar_Data* bar, Buffer* buf) {
  sample::internal::Bar_Data* clone = sample::internal::Bar_Data::New(buf);
  memcpy(clone, bar, sizeof(*bar));
  return clone;
}

// static
void ObjectTraits<sample::internal::Bar_Data>::EncodePointersAndHandles(
    sample::internal::Bar_Data* bar, std::vector<Handle>* handles) {
}

// static
bool ObjectTraits<sample::internal::Bar_Data>::DecodePointersAndHandles(
    sample::internal::Bar_Data* bar, const Message& message) {
  return true;
}

// static
size_t ObjectTraits<sample::internal::Foo_Data>::ComputeSizeOf(
    const sample::internal::Foo_Data* foo) {
  return sizeof(*foo) +
      mojo::internal::ComputeSizeOf(foo->bar()) +
      mojo::internal::ComputeSizeOf(foo->data()) +
      mojo::internal::ComputeSizeOf(foo->extra_bars()) +
      mojo::internal::ComputeSizeOf(foo->name()) +
      mojo::internal::ComputeSizeOf(foo->files());
}

// static
sample::internal::Foo_Data* ObjectTraits<sample::internal::Foo_Data>::Clone(
    const sample::internal::Foo_Data* foo, Buffer* buf) {
  sample::internal::Foo_Data* clone = sample::internal::Foo_Data::New(buf);
  memcpy(clone, foo, sizeof(*foo));

  clone->set_bar(mojo::internal::Clone(foo->bar(), buf));
  clone->set_data(mojo::internal::Clone(foo->data(), buf));
  clone->set_extra_bars(mojo::internal::Clone(foo->extra_bars(), buf));
  clone->set_name(mojo::internal::Clone(foo->name(), buf));
  clone->set_files(mojo::internal::Clone(foo->files(), buf));

  return clone;
}

// static
void ObjectTraits<sample::internal::Foo_Data>::EncodePointersAndHandles(
    sample::internal::Foo_Data* foo, std::vector<Handle>* handles) {
  Encode(&foo->bar_, handles);
  Encode(&foo->data_, handles);
  Encode(&foo->extra_bars_, handles);
  Encode(&foo->name_, handles);
  Encode(&foo->files_, handles);
}

// static
bool ObjectTraits<sample::internal::Foo_Data>::DecodePointersAndHandles(
    sample::internal::Foo_Data* foo, const Message& message) {
  if (!Decode(&foo->bar_, message))
    return false;
  if (!Decode(&foo->data_, message))
    return false;
  if (foo->_header_.num_fields >= 8) {
    if (!Decode(&foo->extra_bars_, message))
      return false;
  }
  if (foo->_header_.num_fields >= 9) {
    if (!Decode(&foo->name_, message))
      return false;
  }
  if (foo->_header_.num_fields >= 10) {
    if (!Decode(&foo->files_, message))
      return false;
  }

  // TODO: validate
  return true;
}

template <>
class ObjectTraits<sample::internal::Service_Frobinate_Params> {
 public:
  static void EncodePointersAndHandles(
      sample::internal::Service_Frobinate_Params* params,
      std::vector<Handle>* handles) {
    Encode(&params->foo_, handles);
    EncodeHandle(&params->port_, handles);
  }

  static bool DecodePointersAndHandles(
      sample::internal::Service_Frobinate_Params* params,
      const Message& message){
    if (!Decode(&params->foo_, message))
      return false;
    if (params->_header_.num_fields >= 3) {
      if (!DecodeHandle(&params->port_, message.handles))
        return false;
    }

    // TODO: validate
    return true;
  }
};

template <>
class ObjectTraits<sample::internal::ServiceClient_DidFrobinate_Params> {
 public:
  static void EncodePointersAndHandles(
      sample::internal::ServiceClient_DidFrobinate_Params* params,
      std::vector<Handle>* handles) {
  }

  static bool DecodePointersAndHandles(
      sample::internal::ServiceClient_DidFrobinate_Params* params,
      const Message& message) {
    return true;
  }
};

}  // namespace internal
}  // namespace mojo
