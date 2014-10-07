// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_TEST_CHANNEL_LISTENER_H_
#define IPC_IPC_TEST_CHANNEL_LISTENER_H_

#include "ipc/ipc_listener.h"

namespace IPC {

class Sender;

// A generic listener that expects messages of a certain type (see
// OnMessageReceived()), and either sends a generic response or quits after the
// 50th message (or on channel error).
class TestChannelListener : public Listener {
 public:
  static const size_t kLongMessageStringNumBytes = 50000;
  static void SendOneMessage(Sender* sender, const char* text);

  TestChannelListener() : sender_(NULL), messages_left_(50) {}
  virtual ~TestChannelListener() {}

  virtual bool OnMessageReceived(const Message& message) override;
  virtual void OnChannelError() override;

  void Init(Sender* s) {
    sender_ = s;
  }

  bool HasSentAll() const { return 0 == messages_left_; }

 protected:
  void SendNextMessage();

 private:
  Sender* sender_;
  int messages_left_;
};

}

#endif // IPC_IPC_TEST_CHANNEL_LISTENER_H_
