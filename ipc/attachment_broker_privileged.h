// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_ATTACHMENT_BROKER_PRIVILEGED_H_
#define IPC_ATTACHMENT_BROKER_PRIVILEGED_H_

#include <vector>

#include "ipc/attachment_broker.h"
#include "ipc/ipc_export.h"

namespace IPC {

class Channel;

// This abstract subclass of AttachmentBroker is intended for use in a
// privileged process . When unprivileged processes want to send attachments,
// the attachments get routed through the privileged process, and more
// specifically, an instance of this class.
class IPC_EXPORT AttachmentBrokerPrivileged : public IPC::AttachmentBroker {
 public:
  AttachmentBrokerPrivileged();
  ~AttachmentBrokerPrivileged() override;

  // Each unprivileged process should have one IPC channel on which it
  // communicates attachment information with the broker process. In the broker
  // process, these channels must be registered and deregistered with the
  // Attachment Broker as they are created and destroyed.
  void RegisterCommunicationChannel(Channel* channel);
  void DeregisterCommunicationChannel(Channel* channel);

 protected:
  // Returns nullptr if no channel is found.
  Channel* GetChannelWithProcessId(base::ProcessId id);

 private:
  std::vector<Channel*> channels_;
  DISALLOW_COPY_AND_ASSIGN(AttachmentBrokerPrivileged);
};

}  // namespace IPC

#endif  // IPC_ATTACHMENT_BROKER_PRIVILEGED_H_
