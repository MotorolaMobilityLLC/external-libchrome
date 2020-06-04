// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(vtl): The POSIX-specific bits have been factored out. Apply this test to
// non-POSIX once we have a non-POSIX implementation.

#include <stdint.h>
#include <string.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "mojo/system/channel.h"
#include "mojo/system/embedder/platform_channel_pair.h"
#include "mojo/system/embedder/scoped_platform_handle.h"
#include "mojo/system/local_message_pipe_endpoint.h"
#include "mojo/system/message_pipe.h"
#include "mojo/system/proxy_message_pipe_endpoint.h"
#include "mojo/system/test_utils.h"
#include "mojo/system/waiter.h"

namespace mojo {
namespace system {
namespace {

class RemoteMessagePipeTest : public test::TestWithIOThreadBase {
 public:
  RemoteMessagePipeTest() {}
  virtual ~RemoteMessagePipeTest() {}

  virtual void SetUp() OVERRIDE {
    test::TestWithIOThreadBase::SetUp();
    test::PostTaskAndWait(io_thread_task_runner(),
                          FROM_HERE,
                          base::Bind(&RemoteMessagePipeTest::SetUpOnIOThread,
                                     base::Unretained(this)));
  }

  virtual void TearDown() OVERRIDE {
    test::PostTaskAndWait(io_thread_task_runner(),
                          FROM_HERE,
                          base::Bind(&RemoteMessagePipeTest::TearDownOnIOThread,
                                     base::Unretained(this)));
    test::TestWithIOThreadBase::TearDown();
  }

  // This connects MP 0, port 1 and MP 1, port 0 (leaving MP 0, port 0 and MP 1,
  // port 1 as the user-visible endpoints) to channel 0 and 1, respectively. MP
  // 0, port 1 and MP 1, port 0 must have |ProxyMessagePipeEndpoint|s.
  void ConnectMessagePipes(scoped_refptr<MessagePipe> mp0,
                           scoped_refptr<MessagePipe> mp1) {
    test::PostTaskAndWait(
        io_thread_task_runner(),
        FROM_HERE,
        base::Bind(&RemoteMessagePipeTest::ConnectMessagePipesOnIOThread,
                   base::Unretained(this), mp0, mp1));
  }

  // This connects |mp|'s port |channel_index ^ 1| to channel |channel_index|.
  // It assumes/requires that this is the bootstrap case, i.e., that the
  // endpoint IDs are both/will both be |Channel::kBootstrapEndpointId|. This
  // returns *without* waiting for it to finish connecting.
  void BootstrapMessagePipeNoWait(unsigned channel_index,
                                  scoped_refptr<MessagePipe> mp) {
    io_thread_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&RemoteMessagePipeTest::BootstrapMessagePipeOnIOThread,
                   base::Unretained(this), channel_index, mp));
  }

 private:
  void SetUpOnIOThread() {
    CHECK_EQ(base::MessageLoop::current(), io_thread_message_loop());

    embedder::PlatformChannelPair channel_pair;
    platform_handles_[0] = channel_pair.PassServerHandle();
    platform_handles_[1] = channel_pair.PassClientHandle();
  }

  void CreateAndInitChannel(unsigned channel_index) {
    CHECK_EQ(base::MessageLoop::current(), io_thread_message_loop());
    CHECK(channel_index == 0 || channel_index == 1);
    CHECK(!channels_[channel_index].get());

    channels_[channel_index] = new Channel();
    CHECK(channels_[channel_index]->Init(
        platform_handles_[channel_index].Pass()));
  }

  void ConnectMessagePipesOnIOThread(scoped_refptr<MessagePipe> mp0,
                                     scoped_refptr<MessagePipe> mp1) {
    CHECK_EQ(base::MessageLoop::current(), io_thread_message_loop());

    if (!channels_[0].get())
      CreateAndInitChannel(0);
    if (!channels_[1].get())
      CreateAndInitChannel(1);

    MessageInTransit::EndpointId local_id0 =
        channels_[0]->AttachMessagePipeEndpoint(mp0, 1);
    MessageInTransit::EndpointId local_id1 =
        channels_[1]->AttachMessagePipeEndpoint(mp1, 0);

    channels_[0]->RunMessagePipeEndpoint(local_id0, local_id1);
    channels_[1]->RunMessagePipeEndpoint(local_id1, local_id0);
  }

  void BootstrapMessagePipeOnIOThread(unsigned channel_index,
                                      scoped_refptr<MessagePipe> mp) {
    CHECK_EQ(base::MessageLoop::current(), io_thread_message_loop());
    CHECK(channel_index == 0 || channel_index == 1);

    unsigned port = channel_index ^ 1u;

    // Important: If we don't boot
    CreateAndInitChannel(channel_index);
    CHECK_EQ(channels_[channel_index]->AttachMessagePipeEndpoint(mp, port),
             Channel::kBootstrapEndpointId);
    channels_[channel_index]->RunMessagePipeEndpoint(
        Channel::kBootstrapEndpointId, Channel::kBootstrapEndpointId);
  }

  void TearDownOnIOThread() {
    if (channels_[0].get()) {
      channels_[0]->Shutdown();
      channels_[0] = NULL;
    }
    if (channels_[1].get()) {
      channels_[1]->Shutdown();
      channels_[1] = NULL;
    }
  }

  embedder::ScopedPlatformHandle platform_handles_[2];
  scoped_refptr<Channel> channels_[2];

  DISALLOW_COPY_AND_ASSIGN(RemoteMessagePipeTest);
};

TEST_F(RemoteMessagePipeTest, Basic) {
  const char hello[] = "hello";
  const char world[] = "world!!!1!!!1!";
  char buffer[100] = { 0 };
  uint32_t buffer_size = static_cast<uint32_t>(sizeof(buffer));
  Waiter waiter;

  // Connect message pipes. MP 0, port 1 will be attached to channel 0 and
  // connected to MP 1, port 0, which will be attached to channel 1. This leaves
  // MP 0, port 0 and MP 1, port 1 as the "user-facing" endpoints.

  scoped_refptr<MessagePipe> mp0(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint())));
  scoped_refptr<MessagePipe> mp1(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint())));
  ConnectMessagePipes(mp0, mp1);

  // Write in one direction: MP 0, port 0 -> ... -> MP 1, port 1.

  // Prepare to wait on MP 1, port 1. (Add the waiter now. Otherwise, if we do
  // it later, it might already be readable.)
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            mp1->AddWaiter(1, &waiter, MOJO_WAIT_FLAG_READABLE, 123));

  // Write to MP 0, port 0.
  EXPECT_EQ(MOJO_RESULT_OK,
            mp0->WriteMessage(0,
                              hello, sizeof(hello),
                              NULL,
                              MOJO_WRITE_MESSAGE_FLAG_NONE));

  // Wait.
  EXPECT_EQ(123, waiter.Wait(MOJO_DEADLINE_INDEFINITE));
  mp1->RemoveWaiter(1, &waiter);

  // Read from MP 1, port 1.
  EXPECT_EQ(MOJO_RESULT_OK,
            mp1->ReadMessage(1,
                             buffer, &buffer_size,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(sizeof(hello), static_cast<size_t>(buffer_size));
  EXPECT_EQ(0, strcmp(buffer, hello));

  // Write in the other direction: MP 1, port 1 -> ... -> MP 0, port 0.

  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            mp0->AddWaiter(0, &waiter, MOJO_WAIT_FLAG_READABLE, 456));

  EXPECT_EQ(MOJO_RESULT_OK,
            mp1->WriteMessage(1,
                              world, sizeof(world),
                              NULL,
                              MOJO_WRITE_MESSAGE_FLAG_NONE));

  EXPECT_EQ(456, waiter.Wait(MOJO_DEADLINE_INDEFINITE));
  mp0->RemoveWaiter(0, &waiter);

  buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OK,
            mp0->ReadMessage(0,
                             buffer, &buffer_size,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(sizeof(world), static_cast<size_t>(buffer_size));
  EXPECT_EQ(0, strcmp(buffer, world));

  // Close MP 0, port 0.
  mp0->Close(0);

  // Try to wait for MP 1, port 1 to become readable. This will eventually fail
  // when it realizes that MP 0, port 0 has been closed. (It may also fail
  // immediately.)
  waiter.Init();
  MojoResult result = mp1->AddWaiter(1, &waiter, MOJO_WAIT_FLAG_READABLE, 789);
  if (result == MOJO_RESULT_OK) {
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              waiter.Wait(MOJO_DEADLINE_INDEFINITE));
    mp1->RemoveWaiter(1, &waiter);
  } else {
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, result);
  }

  // And MP 1, port 1.
  mp1->Close(1);
}

TEST_F(RemoteMessagePipeTest, Multiplex) {
  const char hello[] = "hello";
  const char world[] = "world!!!1!!!1!";
  char buffer[100] = { 0 };
  uint32_t buffer_size = static_cast<uint32_t>(sizeof(buffer));
  Waiter waiter;

  // Connect message pipes as in the |Basic| test.

  scoped_refptr<MessagePipe> mp0(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint())));
  scoped_refptr<MessagePipe> mp1(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint())));
  ConnectMessagePipes(mp0, mp1);

  // Now put another message pipe on the channel.

  scoped_refptr<MessagePipe> mp2(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint())));
  scoped_refptr<MessagePipe> mp3(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint())));
  ConnectMessagePipes(mp2, mp3);

  // Write: MP 2, port 0 -> MP 3, port 1.

  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            mp3->AddWaiter(1, &waiter, MOJO_WAIT_FLAG_READABLE, 789));

  EXPECT_EQ(MOJO_RESULT_OK,
            mp2->WriteMessage(0,
                              hello, sizeof(hello),
                              NULL,
                              MOJO_WRITE_MESSAGE_FLAG_NONE));

  EXPECT_EQ(789, waiter.Wait(MOJO_DEADLINE_INDEFINITE));
  mp3->RemoveWaiter(1, &waiter);

  // Make sure there's nothing on MP 0, port 0 or MP 1, port 1 or MP 2, port 0.
  buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT,
            mp0->ReadMessage(0,
                             buffer, &buffer_size,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_NONE));
  buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT,
            mp1->ReadMessage(1,
                             buffer, &buffer_size,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_NONE));
  buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT,
            mp2->ReadMessage(0,
                             buffer, &buffer_size,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_NONE));

  // Read from MP 3, port 1.
  buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OK,
            mp3->ReadMessage(1,
                             buffer, &buffer_size,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(sizeof(hello), static_cast<size_t>(buffer_size));
  EXPECT_EQ(0, strcmp(buffer, hello));

  // Write: MP 0, port 0 -> MP 1, port 1 again.

  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            mp1->AddWaiter(1, &waiter, MOJO_WAIT_FLAG_READABLE, 123));

  EXPECT_EQ(MOJO_RESULT_OK,
            mp0->WriteMessage(0,
                              world, sizeof(world),
                              NULL,
                              MOJO_WRITE_MESSAGE_FLAG_NONE));

  EXPECT_EQ(123, waiter.Wait(MOJO_DEADLINE_INDEFINITE));
  mp1->RemoveWaiter(1, &waiter);

  // Make sure there's nothing on the other ports.
  buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT,
            mp0->ReadMessage(0,
                             buffer, &buffer_size,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_NONE));
  buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT,
            mp2->ReadMessage(0,
                             buffer, &buffer_size,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_NONE));
  buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT,
            mp3->ReadMessage(1,
                             buffer, &buffer_size,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_NONE));

  buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OK,
            mp1->ReadMessage(1,
                             buffer, &buffer_size,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(sizeof(world), static_cast<size_t>(buffer_size));
  EXPECT_EQ(0, strcmp(buffer, world));
}

TEST_F(RemoteMessagePipeTest, CloseBeforeConnect) {
  const char hello[] = "hello";
  char buffer[100] = { 0 };
  uint32_t buffer_size = static_cast<uint32_t>(sizeof(buffer));
  Waiter waiter;

  // Connect message pipes. MP 0, port 1 will be attached to channel 0 and
  // connected to MP 1, port 0, which will be attached to channel 1. This leaves
  // MP 0, port 0 and MP 1, port 1 as the "user-facing" endpoints.

  scoped_refptr<MessagePipe> mp0(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint())));

  // Write to MP 0, port 0.
  EXPECT_EQ(MOJO_RESULT_OK,
            mp0->WriteMessage(0,
                              hello, sizeof(hello),
                              NULL,
                              MOJO_WRITE_MESSAGE_FLAG_NONE));

  BootstrapMessagePipeNoWait(0, mp0);


  // Close MP 0, port 0 before channel 1 is even connected.
  mp0->Close(0);

  scoped_refptr<MessagePipe> mp1(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint())));

  // Prepare to wait on MP 1, port 1. (Add the waiter now. Otherwise, if we do
  // it later, it might already be readable.)
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            mp1->AddWaiter(1, &waiter, MOJO_WAIT_FLAG_READABLE, 123));

  BootstrapMessagePipeNoWait(1, mp1);

  // Wait.
  EXPECT_EQ(123, waiter.Wait(MOJO_DEADLINE_INDEFINITE));
  mp1->RemoveWaiter(1, &waiter);

  // Read from MP 1, port 1.
  EXPECT_EQ(MOJO_RESULT_OK,
            mp1->ReadMessage(1,
                             buffer, &buffer_size,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(sizeof(hello), static_cast<size_t>(buffer_size));
  EXPECT_EQ(0, strcmp(buffer, hello));

  // And MP 1, port 1.
  mp1->Close(1);
}

}  // namespace
}  // namespace system
}  // namespace mojo
