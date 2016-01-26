// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "mojo/edk/test/mojo_test_base.h"
#include "mojo/public/c/system/types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace edk {
namespace {

using SharedBufferTest = test::MojoTestBase;

TEST_F(SharedBufferTest, CreateSharedBuffer) {
  const std::string message = "hello";
  MojoHandle h = CreateBuffer(message.size());
  WriteToBuffer(h, 0, message);
  ExpectBufferContents(h, 0, message);
}

TEST_F(SharedBufferTest, DuplicateSharedBuffer) {
  const std::string message = "hello";
  MojoHandle h = CreateBuffer(message.size());
  WriteToBuffer(h, 0, message);

  MojoHandle dupe = DuplicateBuffer(h);
  ExpectBufferContents(dupe, 0, message);
}

TEST_F(SharedBufferTest, PassSharedBufferLocal) {
  const std::string message = "hello";
  MojoHandle h = CreateBuffer(message.size());
  WriteToBuffer(h, 0, message);

  MojoHandle dupe = DuplicateBuffer(h);
  MojoHandle p0, p1;
  CreateMessagePipe(&p0, &p1);

  WriteMessageWithHandles(p0, "...", &dupe, 1);
  EXPECT_EQ("...", ReadMessageWithHandles(p1, &dupe, 1));

  ExpectBufferContents(dupe, 0, message);
}

// Reads a single message with a shared buffer handle, maps the buffer, copies
// the message contents into it, then exits.
DEFINE_TEST_CLIENT_TEST_WITH_PIPE(CopyToBufferClient, SharedBufferTest, h) {
  MojoHandle b;
  std::string message = ReadMessageWithHandles(h, &b, 1);
  WriteToBuffer(b, 0, message);

  EXPECT_EQ("quit", ReadMessage(h));
}

TEST_F(SharedBufferTest, PassSharedBufferCrossProcess) {
  const std::string message = "hello";
  MojoHandle b = CreateBuffer(message.size());

  RUN_CHILD_ON_PIPE(CopyToBufferClient, h)
    MojoHandle dupe = DuplicateBuffer(b);
    WriteMessageWithHandles(h, message, &dupe, 1);
    WriteMessage(h, "quit");
  END_CHILD()

  ExpectBufferContents(b, 0, message);
}

// Creates a new buffer, maps it, writes a message contents to it, unmaps it,
// and finally passes it back to the parent.
DEFINE_TEST_CLIENT_TEST_WITH_PIPE(CreateBufferClient, SharedBufferTest, h) {
  std::string message = ReadMessage(h);
  MojoHandle b = CreateBuffer(message.size());
  WriteToBuffer(b, 0, message);
  WriteMessageWithHandles(h, "have a buffer", &b, 1);

  EXPECT_EQ("quit", ReadMessage(h));
}

TEST_F(SharedBufferTest, PassSharedBufferFromChild) {
  const std::string message = "hello";
  MojoHandle b;
  RUN_CHILD_ON_PIPE(CreateBufferClient, h)
    WriteMessage(h, message);
    ReadMessageWithHandles(h, &b, 1);
    WriteMessage(h, "quit");
  END_CHILD()

  ExpectBufferContents(b, 0, message);
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(CreateAndPassBuffer, SharedBufferTest, h) {
  // Receive a pipe handle over the primordial pipe. This will be connected to
  // another child process.
  MojoHandle other_child;
  std::string message = ReadMessageWithHandles(h, &other_child, 1);

  // Create a new shared buffer.
  MojoHandle b = CreateBuffer(message.size());

  // Send a copy of the buffer to the parent and the other child.
  MojoHandle dupe = DuplicateBuffer(b);
  WriteMessageWithHandles(h, "", &b, 1);
  WriteMessageWithHandles(other_child, "", &dupe, 1);

  EXPECT_EQ("quit", ReadMessage(h));
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(ReceiveAndEditBuffer, SharedBufferTest, h) {
  // Receive a pipe handle over the primordial pipe. This will be connected to
  // another child process (running CreateAndPassBuffer).
  MojoHandle other_child;
  std::string message = ReadMessageWithHandles(h, &other_child, 1);

  // Receive a shared buffer from the other child.
  MojoHandle b;
  ReadMessageWithHandles(other_child, &b, 1);

  // Write the message from the parent into the buffer and exit.
  WriteToBuffer(b, 0, message);

  EXPECT_EQ("quit", ReadMessage(h));
}

TEST_F(SharedBufferTest, PassSharedBufferFromChildToChild) {
  const std::string message = "hello";
  MojoHandle p0, p1;
  CreateMessagePipe(&p0, &p1);

  MojoHandle b;
  RUN_CHILD_ON_PIPE(CreateAndPassBuffer, h0)
    RUN_CHILD_ON_PIPE(ReceiveAndEditBuffer, h1)
      // Send one end of the pipe to each child. The first child will create
      // and pass a buffer to the second child and back to us. The second child
      // will write our message into the buffer.
      WriteMessageWithHandles(h0, message, &p0, 1);
      WriteMessageWithHandles(h1, message, &p1, 1);

      // Receive the buffer back from the first child.
      ReadMessageWithHandles(h0, &b, 1);

      WriteMessage(h1, "quit");
      WriteMessage(h0, "quit");
    END_CHILD()
  END_CHILD()

  // The second child should have written this message.
  ExpectBufferContents(b, 0, message);
}

}  // namespace
}  // namespace edk
}  // namespace mojo
