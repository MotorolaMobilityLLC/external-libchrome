// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/channel.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/test/test_io_thread.h"
#include "mojo/embedder/platform_channel_pair.h"
#include "mojo/embedder/simple_platform_support.h"
#include "mojo/system/channel_endpoint.h"
#include "mojo/system/message_in_transit.h"
#include "mojo/system/message_pipe.h"
#include "mojo/system/raw_channel.h"
#include "mojo/system/test_utils.h"
#include "mojo/system/waiter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace system {
namespace {

enum Tristate { TRISTATE_UNKNOWN = -1, TRISTATE_FALSE = 0, TRISTATE_TRUE = 1 };

Tristate BoolToTristate(bool b) {
  return b ? TRISTATE_TRUE : TRISTATE_FALSE;
}

class ChannelTest : public testing::Test {
 public:
  ChannelTest()
      : io_thread_(base::TestIOThread::kAutoStart),
        init_result_(TRISTATE_UNKNOWN) {}
  virtual ~ChannelTest() {}

  virtual void SetUp() OVERRIDE {
    io_thread_.PostTaskAndWait(
        FROM_HERE,
        base::Bind(&ChannelTest::SetUpOnIOThread, base::Unretained(this)));
  }

  void CreateChannelOnIOThread() {
    CHECK_EQ(base::MessageLoop::current(), io_thread()->message_loop());
    channel_ = new Channel(&platform_support_);
  }

  void InitChannelOnIOThread() {
    CHECK_EQ(base::MessageLoop::current(), io_thread()->message_loop());

    CHECK(raw_channel_);
    CHECK(channel_.get());
    CHECK_EQ(init_result_, TRISTATE_UNKNOWN);

    init_result_ = BoolToTristate(channel_->Init(raw_channel_.Pass()));
  }

  void ShutdownChannelOnIOThread() {
    CHECK_EQ(base::MessageLoop::current(), io_thread()->message_loop());

    CHECK(channel_.get());
    channel_->Shutdown();
  }

  base::TestIOThread* io_thread() { return &io_thread_; }
  RawChannel* raw_channel() { return raw_channel_.get(); }
  scoped_ptr<RawChannel>* mutable_raw_channel() { return &raw_channel_; }
  Channel* channel() { return channel_.get(); }
  scoped_refptr<Channel>* mutable_channel() { return &channel_; }
  Tristate init_result() const { return init_result_; }

 private:
  void SetUpOnIOThread() {
    CHECK_EQ(base::MessageLoop::current(), io_thread()->message_loop());

    embedder::PlatformChannelPair channel_pair;
    raw_channel_ = RawChannel::Create(channel_pair.PassServerHandle()).Pass();
    other_platform_handle_ = channel_pair.PassClientHandle();
  }

  embedder::SimplePlatformSupport platform_support_;
  base::TestIOThread io_thread_;
  scoped_ptr<RawChannel> raw_channel_;
  embedder::ScopedPlatformHandle other_platform_handle_;
  scoped_refptr<Channel> channel_;

  Tristate init_result_;

  DISALLOW_COPY_AND_ASSIGN(ChannelTest);
};

// ChannelTest.InitShutdown ----------------------------------------------------

TEST_F(ChannelTest, InitShutdown) {
  io_thread()->PostTaskAndWait(FROM_HERE,
                               base::Bind(&ChannelTest::CreateChannelOnIOThread,
                                          base::Unretained(this)));
  ASSERT_TRUE(channel());

  io_thread()->PostTaskAndWait(
      FROM_HERE,
      base::Bind(&ChannelTest::InitChannelOnIOThread, base::Unretained(this)));
  EXPECT_EQ(TRISTATE_TRUE, init_result());

  io_thread()->PostTaskAndWait(
      FROM_HERE,
      base::Bind(&ChannelTest::ShutdownChannelOnIOThread,
                 base::Unretained(this)));

  // Okay to destroy |Channel| on not-the-I/O-thread.
  EXPECT_TRUE(channel()->HasOneRef());
  *mutable_channel() = nullptr;
}

// ChannelTest.InitFails -------------------------------------------------------

class MockRawChannelOnInitFails : public RawChannel {
 public:
  MockRawChannelOnInitFails() : on_init_called_(false) {}
  virtual ~MockRawChannelOnInitFails() {}

  // |RawChannel| public methods:
  virtual size_t GetSerializedPlatformHandleSize() const OVERRIDE { return 0; }

 private:
  // |RawChannel| protected methods:
  virtual IOResult Read(size_t*) OVERRIDE {
    CHECK(false);
    return IO_FAILED_UNKNOWN;
  }
  virtual IOResult ScheduleRead() OVERRIDE {
    CHECK(false);
    return IO_FAILED_UNKNOWN;
  }
  virtual embedder::ScopedPlatformHandleVectorPtr GetReadPlatformHandles(
      size_t,
      const void*) OVERRIDE {
    CHECK(false);
    return embedder::ScopedPlatformHandleVectorPtr();
  }
  virtual IOResult WriteNoLock(size_t*, size_t*) OVERRIDE {
    CHECK(false);
    return IO_FAILED_UNKNOWN;
  }
  virtual IOResult ScheduleWriteNoLock() OVERRIDE {
    CHECK(false);
    return IO_FAILED_UNKNOWN;
  }
  virtual bool OnInit() OVERRIDE {
    EXPECT_FALSE(on_init_called_);
    on_init_called_ = true;
    return false;
  }
  virtual void OnShutdownNoLock(scoped_ptr<ReadBuffer>,
                                scoped_ptr<WriteBuffer>) OVERRIDE {
    CHECK(false);
  }

  bool on_init_called_;

  DISALLOW_COPY_AND_ASSIGN(MockRawChannelOnInitFails);
};

TEST_F(ChannelTest, InitFails) {
  io_thread()->PostTaskAndWait(FROM_HERE,
                               base::Bind(&ChannelTest::CreateChannelOnIOThread,
                                          base::Unretained(this)));
  ASSERT_TRUE(channel());

  ASSERT_TRUE(raw_channel());
  mutable_raw_channel()->reset(new MockRawChannelOnInitFails());

  io_thread()->PostTaskAndWait(
      FROM_HERE,
      base::Bind(&ChannelTest::InitChannelOnIOThread, base::Unretained(this)));
  EXPECT_EQ(TRISTATE_FALSE, init_result());

  // Should destroy |Channel| with no |Shutdown()| (on not-the-I/O-thread).
  EXPECT_TRUE(channel()->HasOneRef());
  *mutable_channel() = nullptr;
}

// ChannelTest.CloseBeforeRun --------------------------------------------------

TEST_F(ChannelTest, CloseBeforeRun) {
  io_thread()->PostTaskAndWait(FROM_HERE,
                               base::Bind(&ChannelTest::CreateChannelOnIOThread,
                                          base::Unretained(this)));
  ASSERT_TRUE(channel());

  io_thread()->PostTaskAndWait(
      FROM_HERE,
      base::Bind(&ChannelTest::InitChannelOnIOThread, base::Unretained(this)));
  EXPECT_EQ(TRISTATE_TRUE, init_result());

  scoped_refptr<ChannelEndpoint> channel_endpoint;
  scoped_refptr<MessagePipe> mp(
      MessagePipe::CreateLocalProxy(&channel_endpoint));

  MessageInTransit::EndpointId local_id =
      channel()->AttachEndpoint(channel_endpoint);
  EXPECT_EQ(Channel::kBootstrapEndpointId, local_id);

  mp->Close(0);

  // TODO(vtl): Currently, the |Close()| above won't detach (since it thinks
  // we're still expecting a "run" message from the other side), so the
  // |RunMessagePipeEndpoint()| below will return true. We need to refactor
  // |AttachEndpoint()| to indicate whether |Run...()| will necessarily be
  // called or not. (Then, in the case that it may not be called, this will
  // return false.)
  EXPECT_TRUE(channel()->RunMessagePipeEndpoint(local_id,
                                                Channel::kBootstrapEndpointId));

  io_thread()->PostTaskAndWait(
      FROM_HERE,
      base::Bind(&ChannelTest::ShutdownChannelOnIOThread,
                 base::Unretained(this)));

  EXPECT_TRUE(channel()->HasOneRef());
}

// ChannelTest.ShutdownAfterAttachAndRun ---------------------------------------

TEST_F(ChannelTest, ShutdownAfterAttach) {
  io_thread()->PostTaskAndWait(FROM_HERE,
                               base::Bind(&ChannelTest::CreateChannelOnIOThread,
                                          base::Unretained(this)));
  ASSERT_TRUE(channel());

  io_thread()->PostTaskAndWait(
      FROM_HERE,
      base::Bind(&ChannelTest::InitChannelOnIOThread, base::Unretained(this)));
  EXPECT_EQ(TRISTATE_TRUE, init_result());

  scoped_refptr<ChannelEndpoint> channel_endpoint;
  scoped_refptr<MessagePipe> mp(
      MessagePipe::CreateLocalProxy(&channel_endpoint));

  MessageInTransit::EndpointId local_id =
      channel()->AttachEndpoint(channel_endpoint);
  EXPECT_EQ(Channel::kBootstrapEndpointId, local_id);

  // TODO(vtl): Currently, we always "expect" a |RunMessagePipeEndpoint()| after
  // an |AttachEndpoint()| (which is actually incorrect). We need to refactor
  // |AttachEndpoint()| to indicate whether |Run...()| will necessarily be
  // called or not. (Then, in the case that it may not be called, we should test
  // a |Shutdown()| without the |Run...()|.)
  EXPECT_TRUE(channel()->RunMessagePipeEndpoint(local_id,
                                                Channel::kBootstrapEndpointId));

  Waiter waiter;
  waiter.Init();
  ASSERT_EQ(
      MOJO_RESULT_OK,
      mp->AddWaiter(0, &waiter, MOJO_HANDLE_SIGNAL_READABLE, 123, nullptr));

  // Don't wait for the shutdown to run ...
  io_thread()->PostTask(FROM_HERE,
                        base::Bind(&ChannelTest::ShutdownChannelOnIOThread,
                                   base::Unretained(this)));

  // ... since this |Wait()| should fail once the channel is shut down.
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            waiter.Wait(MOJO_DEADLINE_INDEFINITE, nullptr));
  HandleSignalsState hss;
  mp->RemoveWaiter(0, &waiter, &hss);
  EXPECT_EQ(0u, hss.satisfied_signals);
  EXPECT_EQ(0u, hss.satisfiable_signals);

  mp->Close(0);

  EXPECT_TRUE(channel()->HasOneRef());
}

// ChannelTest.WaitAfterAttachRunAndShutdown -----------------------------------

TEST_F(ChannelTest, WaitAfterAttachRunAndShutdown) {
  io_thread()->PostTaskAndWait(FROM_HERE,
                               base::Bind(&ChannelTest::CreateChannelOnIOThread,
                                          base::Unretained(this)));
  ASSERT_TRUE(channel());

  io_thread()->PostTaskAndWait(
      FROM_HERE,
      base::Bind(&ChannelTest::InitChannelOnIOThread, base::Unretained(this)));
  EXPECT_EQ(TRISTATE_TRUE, init_result());

  scoped_refptr<ChannelEndpoint> channel_endpoint;
  scoped_refptr<MessagePipe> mp(
      MessagePipe::CreateLocalProxy(&channel_endpoint));

  MessageInTransit::EndpointId local_id =
      channel()->AttachEndpoint(channel_endpoint);
  EXPECT_EQ(Channel::kBootstrapEndpointId, local_id);

  EXPECT_TRUE(channel()->RunMessagePipeEndpoint(local_id,
                                                Channel::kBootstrapEndpointId));

  io_thread()->PostTaskAndWait(
      FROM_HERE,
      base::Bind(&ChannelTest::ShutdownChannelOnIOThread,
                 base::Unretained(this)));

  Waiter waiter;
  waiter.Init();
  HandleSignalsState hss;
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            mp->AddWaiter(0, &waiter, MOJO_HANDLE_SIGNAL_READABLE, 123, &hss));
  EXPECT_EQ(0u, hss.satisfied_signals);
  EXPECT_EQ(0u, hss.satisfiable_signals);

  mp->Close(0);

  EXPECT_TRUE(channel()->HasOneRef());
}

// TODO(vtl): More. ------------------------------------------------------------

}  // namespace
}  // namespace system
}  // namespace mojo
