// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/channel.h"
#include "base/memory/ptr_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace edk {
namespace {

class TestChannel : public Channel {
 public:
  TestChannel(Channel::Delegate* delegate) : Channel(delegate) {}

  char* GetReadBufferTest(size_t* buffer_capacity) {
    return GetReadBuffer(buffer_capacity);
  }

  bool OnReadCompleteTest(size_t bytes_read, size_t* next_read_size_hint) {
    return OnReadComplete(bytes_read, next_read_size_hint);
  }

  MOCK_METHOD4(GetReadPlatformHandles,
               bool(size_t num_handles,
                    const void* extra_header,
                    size_t extra_header_size,
                    ScopedPlatformHandleVectorPtr* handles));
  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(ShutDownImpl, void());
  MOCK_METHOD0(LeakHandle, void());

  void Write(MessagePtr message) {}

 protected:
  ~TestChannel() override {}
};

// Not using GMock as I don't think it supports movable types.
class MockChannelDelegate : public Channel::Delegate {
 public:
  MockChannelDelegate() {}

  size_t GetReceivedPayloadSize() const { return payload_size_; }

  const void* GetReceivedPayload() const { return payload_.get(); }

 protected:
  void OnChannelMessage(const void* payload,
                        size_t payload_size,
                        ScopedPlatformHandleVectorPtr handles) override {
    payload_.reset(new char[payload_size]);
    memcpy(payload_.get(), payload, payload_size);
    payload_size_ = payload_size;
  }

  // Notify that an error has occured and the Channel will cease operation.
  void OnChannelError() override {}

 private:
  size_t payload_size_ = 0;
  std::unique_ptr<char[]> payload_;
};

Channel::MessagePtr CreateDefaultMessage(bool legacy_message) {
  const size_t payload_size = 100;
  Channel::MessagePtr message = base::MakeUnique<Channel::Message>(
      payload_size, 0,
      legacy_message ? Channel::Message::MessageType::NORMAL_LEGACY
                     : Channel::Message::MessageType::NORMAL);
  char* payload = static_cast<char*>(message->mutable_payload());
  for (size_t i = 0; i < payload_size; i++) {
    payload[i] = static_cast<char>(i);
  }
  return message;
}

void TestMemoryEqual(const void* data1,
                     size_t data1_size,
                     const void* data2,
                     size_t data2_size) {
  ASSERT_EQ(data1_size, data2_size);
  const unsigned char* data1_char = static_cast<const unsigned char*>(data1);
  const unsigned char* data2_char = static_cast<const unsigned char*>(data2);
  for (size_t i = 0; i < data1_size; i++) {
    // ASSERT so we don't log tons of errors if the data is different.
    ASSERT_EQ(data1_char[i], data2_char[i]);
  }
}

void TestMessagesAreEqual(Channel::Message* message1,
                          Channel::Message* message2,
                          bool legacy_messages) {
  // If any of the message is null, this is probably not what you wanted to
  // test.
  ASSERT_NE(nullptr, message1);
  ASSERT_NE(nullptr, message2);

  ASSERT_EQ(message1->payload_size(), message2->payload_size());
  EXPECT_EQ(message1->has_handles(), message2->has_handles());

  TestMemoryEqual(message1->payload(), message1->payload_size(),
                  message2->payload(), message2->payload_size());

  if (legacy_messages)
    return;

  ASSERT_EQ(message1->extra_header_size(), message2->extra_header_size());
  TestMemoryEqual(message1->extra_header(), message1->extra_header_size(),
                  message2->extra_header(), message2->extra_header_size());
}

TEST(ChannelTest, LegacyMessageDeserialization) {
  Channel::MessagePtr message = CreateDefaultMessage(true /* legacy_message */);
  Channel::MessagePtr deserialized_message =
      Channel::Message::Deserialize(message->data(), message->data_num_bytes());
  TestMessagesAreEqual(message.get(), deserialized_message.get(),
                       true /* legacy_message */);
}

TEST(ChannelTest, NonLegacyMessageDeserialization) {
  Channel::MessagePtr message =
      CreateDefaultMessage(false /* legacy_message */);
  Channel::MessagePtr deserialized_message =
      Channel::Message::Deserialize(message->data(), message->data_num_bytes());
  TestMessagesAreEqual(message.get(), deserialized_message.get(),
                       false /* legacy_message */);
}

TEST(ChannelTest, OnReadLegacyMessage) {
  size_t buffer_size = 100 * 1024;
  Channel::MessagePtr message = CreateDefaultMessage(true /* legacy_message */);

  MockChannelDelegate channel_delegate;
  scoped_refptr<TestChannel> channel = new TestChannel(&channel_delegate);
  char* read_buffer = channel->GetReadBufferTest(&buffer_size);
  ASSERT_LT(message->data_num_bytes(),
            buffer_size);  // Bad test. Increase buffer
                           // size.
  memcpy(read_buffer, message->data(), message->data_num_bytes());

  size_t next_read_size_hint = 0;
  EXPECT_TRUE(channel->OnReadCompleteTest(message->data_num_bytes(),
                                          &next_read_size_hint));

  TestMemoryEqual(message->payload(), message->payload_size(),
                  channel_delegate.GetReceivedPayload(),
                  channel_delegate.GetReceivedPayloadSize());
}

TEST(ChannelTest, OnReadNonLegacyMessage) {
  size_t buffer_size = 100 * 1024;
  Channel::MessagePtr message =
      CreateDefaultMessage(false /* legacy_message */);

  MockChannelDelegate channel_delegate;
  scoped_refptr<TestChannel> channel = new TestChannel(&channel_delegate);
  char* read_buffer = channel->GetReadBufferTest(&buffer_size);
  ASSERT_LT(message->data_num_bytes(),
            buffer_size);  // Bad test. Increase buffer
                           // size.
  memcpy(read_buffer, message->data(), message->data_num_bytes());

  size_t next_read_size_hint = 0;
  EXPECT_TRUE(channel->OnReadCompleteTest(message->data_num_bytes(),
                                          &next_read_size_hint));

  TestMemoryEqual(message->payload(), message->payload_size(),
                  channel_delegate.GetReceivedPayload(),
                  channel_delegate.GetReceivedPayloadSize());
}

}  // namespace
}  // namespace edk
}  // namespace mojo
