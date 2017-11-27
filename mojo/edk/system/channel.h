// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_CHANNEL_H_
#define MOJO_EDK_SYSTEM_CHANNEL_H_

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/process/process_handle.h"
#include "base/task_runner.h"
#include "build/build_config.h"
#include "mojo/edk/embedder/connection_params.h"
#include "mojo/edk/embedder/platform_handle_vector.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"

namespace mojo {
namespace edk {

const size_t kChannelMessageAlignment = 8;

constexpr bool IsAlignedForChannelMessage(size_t n) {
  return n % kChannelMessageAlignment == 0;
}

// Channel provides a thread-safe interface to read and write arbitrary
// delimited messages over an underlying I/O channel, optionally transferring
// one or more platform handles in the process.
class MOJO_SYSTEM_IMPL_EXPORT Channel
    : public base::RefCountedThreadSafe<Channel> {
 public:
  struct Message;

  using MessagePtr = std::unique_ptr<Message>;

  // A message to be written to a channel.
  struct MOJO_SYSTEM_IMPL_EXPORT Message {
    enum class MessageType : uint16_t {
      // An old format normal message, that uses the LegacyHeader.
      // Only used on Android and ChromeOS.
      // TODO(jcivelli): remove legacy support when Arc++ has updated to Mojo
      //                 with normal versioned messages. crbug.com/695645
      NORMAL_LEGACY = 0,
#if defined(OS_MACOSX)
      // A control message containing handles to echo back.
      HANDLES_SENT,
      // A control message containing handles that can now be closed.
      HANDLES_SENT_ACK,
#endif
      // A normal message that uses Header and can contain extra header values.
      NORMAL,
    };

#pragma pack(push, 1)
    // Old message wire format for ChromeOS and Android, used by NORMAL_LEGACY
    // messages.
    struct LegacyHeader {
      // Message size in bytes, including the header.
      uint32_t num_bytes;

      // Number of attached handles.
      uint16_t num_handles;

      MessageType message_type;
    };

    // Header used by NORMAL messages.
    // To preserve backward compatibility with LegacyHeader, the num_bytes and
    // message_type field must be at the same offset as in LegacyHeader.
    struct Header {
      // Message size in bytes, including the header.
      uint32_t num_bytes;

      // Total size of header, including extra header data (i.e. HANDLEs on
      // windows).
      uint16_t num_header_bytes;

      MessageType message_type;

      // Number of attached handles. May be less than the reserved handle
      // storage size in this message on platforms that serialise handles as
      // data (i.e. HANDLEs on Windows, Mach ports on OSX).
      uint16_t num_handles;

      char padding[6];
    };

#if defined(OS_MACOSX) && !defined(OS_IOS)
    struct MachPortsEntry {
      // Index of Mach port in the original vector of PlatformHandles.
      uint16_t index;

      // Mach port name.
      uint32_t mach_port;
      static_assert(sizeof(mach_port_t) <= sizeof(uint32_t),
                    "mach_port_t must be no larger than uint32_t");
    };
    static_assert(sizeof(MachPortsEntry) == 6,
                  "sizeof(MachPortsEntry) must be 6 bytes");

    // Structure of the extra header field when present on OSX.
    struct MachPortsExtraHeader {
      // Actual number of Mach ports encoded in the extra header.
      uint16_t num_ports;

      // Array of encoded Mach ports. If |num_ports| > 0, |entries[0]| through
      // to |entries[num_ports-1]| inclusive are valid.
      MachPortsEntry entries[0];
    };
    static_assert(sizeof(MachPortsExtraHeader) == 2,
                  "sizeof(MachPortsExtraHeader) must be 2 bytes");
#elif defined(OS_FUCHSIA)
    struct HandleInfoEntry {
      // The FDIO type associated with one or more handles, or zero for handles
      // that do not belong to FDIO.
      uint8_t type;
      // Zero for non-FDIO handles, otherwise the number of handles to consume
      // to generate an FDIO file-descriptor wrapper.
      uint8_t count;
    };
#elif defined(OS_WIN)
    struct HandleEntry {
      // The windows HANDLE. HANDLEs are guaranteed to fit inside 32-bits.
      // See: https://msdn.microsoft.com/en-us/library/aa384203(VS.85).aspx
      uint32_t handle;
    };
    static_assert(sizeof(HandleEntry) == 4,
                  "sizeof(HandleEntry) must be 4 bytes");
#endif
#pragma pack(pop)

    // Allocates and owns a buffer for message data with enough capacity for
    // |payload_size| bytes plus a header, plus |max_handles| platform handles.
    Message(size_t payload_size, size_t max_handles);
    Message(size_t payload_size, size_t max_handles, MessageType message_type);
    Message(size_t capacity, size_t payload_size, size_t max_handles);
    Message(size_t capacity,
            size_t max_handles,
            size_t payload_size,
            MessageType message_type);
    ~Message();

    // Constructs a Message from serialized message data.
    static MessagePtr Deserialize(const void* data, size_t data_num_bytes);

    const void* data() const { return data_; }
    size_t data_num_bytes() const { return size_; }

    // The current capacity of the message buffer, not counting internal header
    // data.
    size_t capacity() const;

    // Extends the portion of the total message capacity which contains
    // meaningful payload data. Storage capacity which falls outside of this
    // range is not transmitted when the message is sent.
    //
    // If the message's current capacity is not large enough to accommodate the
    // new payload size, it will be reallocated accordingly.
    void ExtendPayload(size_t new_payload_size);

    const void* extra_header() const;
    void* mutable_extra_header();
    size_t extra_header_size() const;

    void* mutable_payload();
    const void* payload() const;
    size_t payload_size() const;

    size_t num_handles() const;
    bool has_handles() const;
#if defined(OS_MACOSX) && !defined(OS_IOS)
    bool has_mach_ports() const;
#endif

    bool is_legacy_message() const;
    LegacyHeader* legacy_header() const;
    Header* header() const;

    // Note: SetHandles() and TakeHandles() invalidate any previous value of
    // handles().
    void SetHandles(ScopedPlatformHandleVectorPtr new_handles);
    ScopedPlatformHandleVectorPtr TakeHandles();
    // Version of TakeHandles that returns a vector of platform handles suitable
    // for transfer over an underlying OS mechanism. i.e. file descriptors over
    // a unix domain socket. Any handle that cannot be transferred this way,
    // such as Mach ports, will be removed.
    ScopedPlatformHandleVectorPtr TakeHandlesForTransport();

#if defined(OS_WIN)
    // Prepares the handles in this message for use in a different process.
    // Upon calling this the handles should belong to |from_process|; after the
    // call they'll belong to |to_process|. The source handles are always
    // closed by this call. Returns false iff one or more handles failed
    // duplication.
    static bool RewriteHandles(base::ProcessHandle from_process,
                               base::ProcessHandle to_process,
                               PlatformHandleVector* handles);
#endif

    void SetVersionForTest(uint16_t version_number);

   private:
    // The message data buffer.
    char* data_ = nullptr;

    // The capacity of the buffer at |data_|.
    size_t capacity_ = 0;

    // The size of the message. This is the portion of |data_| that should
    // be transmitted if the message is written to a channel. Includes all
    // headers and user payload.
    size_t size_ = 0;

    // Maximum number of handles which may be attached to this message.
    size_t max_handles_ = 0;

    ScopedPlatformHandleVectorPtr handle_vector_;

#if defined(OS_WIN)
    // On Windows, handles are serialised into the extra header section.
    HandleEntry* handles_ = nullptr;
#elif defined(OS_MACOSX) && !defined(OS_IOS)
    // On OSX, handles are serialised into the extra header section.
    MachPortsExtraHeader* mach_ports_header_ = nullptr;
#endif

    DISALLOW_COPY_AND_ASSIGN(Message);
  };

  // Error types which may be reported by a Channel instance to its delegate.
  enum class Error {
    // The remote end of the channel has been closed, either explicitly or
    // because the process which hosted it is gone.
    kDisconnected,

    // For connection-oriented channels (e.g. named pipes), an unexpected error
    // occurred during channel connection.
    kConnectionFailed,

    // Some incoming data failed validation, implying either a buggy or
    // compromised sender.
    kReceivedMalformedData,
  };

  // Delegate methods are called from the I/O task runner with which the Channel
  // was created (see Channel::Create).
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Notify of a received message. |payload| is not owned and must not be
    // retained; it will be null if |payload_size| is 0. |handles| are
    // transferred to the callee.
    virtual void OnChannelMessage(const void* payload,
                                  size_t payload_size,
                                  ScopedPlatformHandleVectorPtr handles) = 0;

    // Notify that an error has occured and the Channel will cease operation.
    virtual void OnChannelError(Error error) = 0;
  };

  // Creates a new Channel around a |platform_handle|, taking ownership of the
  // handle. All I/O on the handle will be performed on |io_task_runner|.
  // Note that ShutDown() MUST be called on the Channel some time before
  // |delegate| is destroyed.
  static scoped_refptr<Channel> Create(
      Delegate* delegate,
      ConnectionParams connection_params,
      scoped_refptr<base::TaskRunner> io_task_runner);

  // Request that the channel be shut down. This should always be called before
  // releasing the last reference to a Channel to ensure that it's cleaned up
  // on its I/O task runner's thread.
  //
  // Delegate methods will no longer be invoked after this call.
  void ShutDown();

  // Begin processing I/O events. Delegate methods must only be invoked after
  // this call.
  virtual void Start() = 0;

  // Stop processing I/O events.
  virtual void ShutDownImpl() = 0;

  // Queues an outgoing message on the Channel. This message will either
  // eventually be written or will fail to write and trigger
  // Delegate::OnChannelError.
  virtual void Write(MessagePtr message) = 0;

  // Causes the platform handle to leak when this channel is shut down instead
  // of closing it.
  virtual void LeakHandle() = 0;

 protected:
  explicit Channel(Delegate* delegate);
  virtual ~Channel();

  // Called by the implementation when it wants somewhere to stick data.
  // |*buffer_capacity| may be set by the caller to indicate the desired buffer
  // size. If 0, a sane default size will be used instead.
  //
  // Returns the address of a buffer which can be written to, and indicates its
  // actual capacity in |*buffer_capacity|.
  char* GetReadBuffer(size_t* buffer_capacity);

  // Called by the implementation when new data is available in the read
  // buffer. Returns false to indicate an error. Upon success,
  // |*next_read_size_hint| will be set to a recommended size for the next
  // read done by the implementation.
  bool OnReadComplete(size_t bytes_read, size_t* next_read_size_hint);

  // Called by the implementation when something goes horribly wrong. It is NOT
  // OK to call this synchronously from any public interface methods.
  void OnError(Error error);

  // Retrieves the set of platform handles read for a given message.
  // |extra_header| and |extra_header_size| correspond to the extra header data.
  // Depending on the Channel implementation, this body may encode platform
  // handles, or handles may be stored and managed elsewhere by the
  // implementation.
  //
  // Returns |false| on unrecoverable error (i.e. the Channel should be closed).
  // Returns |true| otherwise. Note that it is possible on some platforms for an
  // insufficient number of handles to be available when this call is made, but
  // this is not necessarily an error condition. In such cases this returns
  // |true| but |*handles| will also be reset to null.
  virtual bool GetReadPlatformHandles(
      size_t num_handles,
      const void* extra_header,
      size_t extra_header_size,
      ScopedPlatformHandleVectorPtr* handles) = 0;

  // Handles a received control message. Returns |true| if the message is
  // accepted, or |false| otherwise.
  virtual bool OnControlMessage(Message::MessageType message_type,
                                const void* payload,
                                size_t payload_size,
                                ScopedPlatformHandleVectorPtr handles);

 private:
  friend class base::RefCountedThreadSafe<Channel>;

  class ReadBuffer;

  Delegate* delegate_;
  const std::unique_ptr<ReadBuffer> read_buffer_;

  DISALLOW_COPY_AND_ASSIGN(Channel);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_CHANNEL_H_
