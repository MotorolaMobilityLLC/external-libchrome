// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/channel.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "mojo/embedder/platform_channel_pair.h"
#include "mojo/system/raw_channel.h"
#include "mojo/system/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace system {
namespace {

enum Tristate {
  TRISTATE_UNKNOWN = -1,
  TRISTATE_FALSE = 0,
  TRISTATE_TRUE = 1
};

Tristate BoolToTristate(bool b) {
  return b ? TRISTATE_TRUE : TRISTATE_FALSE;
}

class ChannelTest : public testing::Test {
 public:
  ChannelTest()
      : io_thread_(test::TestIOThread::kAutoStart),
        init_result_(TRISTATE_UNKNOWN) {}
  virtual ~ChannelTest() {}

  virtual void SetUp() OVERRIDE {
    io_thread_.PostTaskAndWait(
        FROM_HERE,
        base::Bind(&ChannelTest::SetUpOnIOThread, base::Unretained(this)));
  }

  void CreateChannelOnIOThread() {
    CHECK_EQ(base::MessageLoop::current(), io_thread()->message_loop());
    channel_ = new Channel();
  }

  void InitChannelOnIOThread() {
    CHECK_EQ(base::MessageLoop::current(), io_thread()->message_loop());

    CHECK(raw_channel_);
    CHECK(channel_);
    CHECK_EQ(init_result_, TRISTATE_UNKNOWN);

    init_result_ = BoolToTristate(channel_->Init(raw_channel_.Pass()));
  }

  void ShutdownChannelOnIOThread() {
    CHECK_EQ(base::MessageLoop::current(), io_thread()->message_loop());

    CHECK(channel_);
    channel_->Shutdown();
  }

  test::TestIOThread* io_thread() { return &io_thread_; }
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

  test::TestIOThread io_thread_;
  scoped_ptr<RawChannel> raw_channel_;
  embedder::ScopedPlatformHandle other_platform_handle_;
  scoped_refptr<Channel> channel_;

  Tristate init_result_;

  DISALLOW_COPY_AND_ASSIGN(ChannelTest);
};

// ChannelTest.InitShutdown ----------------------------------------------------

TEST_F(ChannelTest, InitShutdown) {
  io_thread()->PostTaskAndWait(
      FROM_HERE,
      base::Bind(&ChannelTest::CreateChannelOnIOThread,
                 base::Unretained(this)));
  ASSERT_TRUE(channel());

  io_thread()->PostTaskAndWait(
      FROM_HERE,
      base::Bind(&ChannelTest::InitChannelOnIOThread,
                 base::Unretained(this)));
  EXPECT_EQ(TRISTATE_TRUE, init_result());

  io_thread()->PostTaskAndWait(
      FROM_HERE,
      base::Bind(&ChannelTest::ShutdownChannelOnIOThread,
                 base::Unretained(this)));

  // Okay to destroy |Channel| on not-the-I/O-thread.
  EXPECT_TRUE(channel()->HasOneRef());
  *mutable_channel() = NULL;
}

// ChannelTest.InitFails -------------------------------------------------------

class MockRawChannelOnInitFails : public RawChannel {
 public:
  MockRawChannelOnInitFails() : on_init_called_(false) {}
  virtual ~MockRawChannelOnInitFails() {}

  virtual IOResult Read(size_t*) OVERRIDE {
    CHECK(false);
    return IO_FAILED;
  }
  virtual IOResult ScheduleRead() OVERRIDE {
    CHECK(false);
    return IO_FAILED;
  }
  virtual IOResult WriteNoLock(size_t*) OVERRIDE {
    CHECK(false);
    return IO_FAILED;
  }
  virtual IOResult ScheduleWriteNoLock() OVERRIDE {
    CHECK(false);
    return IO_FAILED;
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

 private:
  bool on_init_called_;

  DISALLOW_COPY_AND_ASSIGN(MockRawChannelOnInitFails);
};

TEST_F(ChannelTest, InitFails) {
  io_thread()->PostTaskAndWait(
      FROM_HERE,
      base::Bind(&ChannelTest::CreateChannelOnIOThread,
                 base::Unretained(this)));
  ASSERT_TRUE(channel());

  ASSERT_TRUE(raw_channel());
  mutable_raw_channel()->reset(new MockRawChannelOnInitFails());

  io_thread()->PostTaskAndWait(
      FROM_HERE,
      base::Bind(&ChannelTest::InitChannelOnIOThread,
                 base::Unretained(this)));
  EXPECT_EQ(TRISTATE_FALSE, init_result());

  // Should destroy |Channel| with no |Shutdown()| (on not-the-I/O-thread).
  EXPECT_TRUE(channel()->HasOneRef());
  *mutable_channel() = NULL;
}

// TODO(vtl): More.

}  // namespace
}  // namespace system
}  // namespace mojo
