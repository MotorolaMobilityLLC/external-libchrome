// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_INVITATION_H_
#define MOJO_PUBLIC_CPP_SYSTEM_INVITATION_H_

#include <cstdint>
#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/strings/string_piece.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_channel_server_endpoint.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/system_export.h"

namespace mojo {

// A callback which may be provided when sending an invitation to another
// process. In the event of any validation errors regarding messages from that
// process (reported via MojoNotifyBadMessage etc and related helpers), the
// callback will be invoked.
using ProcessErrorCallback = base::RepeatingCallback<void(const std::string&)>;

// A strongly-typed representation of a |MojoHandle| for an invitation.
class InvitationHandle : public Handle {
 public:
  InvitationHandle() {}
  explicit InvitationHandle(MojoHandle value) : Handle(value) {}

  // Copying and assignment allowed.
};

static_assert(sizeof(InvitationHandle) == sizeof(Handle),
              "Bad size for C++ InvitationHandle");

using ScopedInvitationHandle = ScopedHandleBase<InvitationHandle>;
static_assert(sizeof(ScopedInvitationHandle) == sizeof(InvitationHandle),
              "Bad size for C++ ScopedInvitationHandle");

// An OutgoingInvitation is used to invite another process to join the calling
// process's IPC network.
//
// Typical use involves constructing a |PlatformChannel| and using one end to
// send the invitation (see |Send()| below) while passing the other to a child
// process.
//
// This may also be used with the server endpoint of a |NamedPlatformChannel|.
class MOJO_CPP_SYSTEM_EXPORT OutgoingInvitation {
 public:
  OutgoingInvitation();
  OutgoingInvitation(OutgoingInvitation&& other);
  ~OutgoingInvitation();

  OutgoingInvitation& operator=(OutgoingInvitation&& other);

  // Creates a new message pipe, attaching one end to this invitation and
  // returning the other end to the caller. The invitee can extract the
  // attached endpoint (see |IncomingInvitation|) thus establishing end-to-end
  // Mojo communication.
  //
  // |name| is an arbitrary value that must be used by the invitee to extract
  // the corresponding attached endpoint.
  ScopedMessagePipeHandle AttachMessagePipe(base::StringPiece name);

  // Same as above but allows use of an integer name for convenience.
  ScopedMessagePipeHandle AttachMessagePipe(uint64_t name);

  // Extracts an attached pipe. Note that this is not typically useful, but it
  // is potentially necessary in cases where a caller wants to, e.g., abort
  // launching another process and recover a pipe endpoint they had previously
  // attached.
  ScopedMessagePipeHandle ExtractMessagePipe(base::StringPiece name);

  // Same as above but allows use of an integer name for convenience.
  ScopedMessagePipeHandle ExtractMessagePipe(uint64_t name);

  // Sends |invitation| to another process via |channel_endpoint|, which should
  // correspond to the local endpoint taken from a |PlatformChannel|.
  //
  // |process_handle| is a handle to the destination process if known. If not
  // provided, IPC may be limited on some platforms (namely Mac and Windows) due
  // to an inability to transfer system handles across the boundary.
  static void Send(OutgoingInvitation invitation,
                   base::ProcessHandle target_process,
                   PlatformChannelEndpoint channel_endpoint,
                   const ProcessErrorCallback& error_callback = {});

  // Similar to above, but sends |invitation| via |server_endpoint|, which
  // should correspond to a |PlatformChannelServerEndpoint| taken from a
  // |NamedPlatformChannel|.
  static void Send(OutgoingInvitation invitation,
                   base::ProcessHandle target_process,
                   PlatformChannelServerEndpoint server_endpoint,
                   const ProcessErrorCallback& error_callback = {});

 private:
  ScopedInvitationHandle handle_;

  DISALLOW_COPY_AND_ASSIGN(OutgoingInvitation);
};

// An IncomingInvitation can be accepted by an invited process by calling
// |IncomingInvitation::Accept()|. Once accepted, the invitation can be used
// to extract attached message pipes by name.
class MOJO_CPP_SYSTEM_EXPORT IncomingInvitation {
 public:
  IncomingInvitation();
  IncomingInvitation(IncomingInvitation&& other);
  explicit IncomingInvitation(ScopedInvitationHandle handle);
  ~IncomingInvitation();

  IncomingInvitation& operator=(IncomingInvitation&& other);

  // Accepts an incoming invitation from |channel_endpoint|. If the invitation
  // was sent using one end of a |PlatformChannel|, |channel_endpoint| should be
  // the other end of that channel. If the invitation was sent using a
  // |PlatformChannelServerEndpoint|, then |channel_endpoint| should be created
  // by |NamedPlatformChannel::ConnectToServer|.
  static IncomingInvitation Accept(PlatformChannelEndpoint channel_endpoint);

  // Extracts an attached message pipe from this invitation. This may succeed
  // even if no such pipe was attached, though the extracted pipe will
  // eventually observe peer closure.
  ScopedMessagePipeHandle ExtractMessagePipe(base::StringPiece name);

  // Same as above but allows use of an integer name for convenience.
  ScopedMessagePipeHandle ExtractMessagePipe(uint64_t name);

 private:
  ScopedInvitationHandle handle_;

  DISALLOW_COPY_AND_ASSIGN(IncomingInvitation);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_INVITATION_H_
