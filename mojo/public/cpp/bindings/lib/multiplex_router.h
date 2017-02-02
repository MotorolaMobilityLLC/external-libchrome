// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_MULTIPLEX_ROUTER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_MULTIPLEX_ROUTER_H_

#include <stdint.h>

#include <deque>
#include <map>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/associated_group_controller.h"
#include "mojo/public/cpp/bindings/bindings_export.h"
#include "mojo/public/cpp/bindings/connector.h"
#include "mojo/public/cpp/bindings/filter_chain.h"
#include "mojo/public/cpp/bindings/interface_id.h"
#include "mojo/public/cpp/bindings/message_header_validator.h"
#include "mojo/public/cpp/bindings/pipe_control_message_handler.h"
#include "mojo/public/cpp/bindings/pipe_control_message_handler_delegate.h"
#include "mojo/public/cpp/bindings/pipe_control_message_proxy.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace mojo {

namespace internal {

// MultiplexRouter supports routing messages for multiple interfaces over a
// single message pipe.
//
// It is created on the thread where the master interface of the message pipe
// lives. Although it is ref-counted, it is guarateed to be destructed on the
// same thread.
// Some public methods are only allowed to be called on the creating thread;
// while the others are safe to call from any threads. Please see the method
// comments for more details.
//
// NOTE: CloseMessagePipe() or PassMessagePipe() MUST be called on |runner|'s
// thread before this object is destroyed.
class MOJO_CPP_BINDINGS_EXPORT MultiplexRouter
    : NON_EXPORTED_BASE(public MessageReceiver),
      public AssociatedGroupController,
      NON_EXPORTED_BASE(public PipeControlMessageHandlerDelegate) {
 public:
  enum Config {
    // There is only the master interface running on this router. Please note
    // that because of interface versioning, the other side of the message pipe
    // may use a newer master interface definition which passes associated
    // interfaces. In that case, this router may still receive pipe control
    // messages or messages targetting associated interfaces.
    SINGLE_INTERFACE,
    // Similar to the mode above, there is only the master interface running on
    // this router. Besides, the master interface has sync methods.
    SINGLE_INTERFACE_WITH_SYNC_METHODS,
    // There may be associated interfaces running on this router.
    MULTI_INTERFACE
  };

  // If |set_interface_id_namespace_bit| is true, the interface IDs generated by
  // this router will have the highest bit set.
  MultiplexRouter(ScopedMessagePipeHandle message_pipe,
                  Config config,
                  bool set_interface_id_namespace_bit,
                  scoped_refptr<base::SingleThreadTaskRunner> runner);

  // Sets the master interface name for this router. Only used when reporting
  // message header or control message validation errors.
  // |name| must be a string literal.
  void SetMasterInterfaceName(const char* name);

  // ---------------------------------------------------------------------------
  // The following public methods are safe to call from any threads.

  // AssociatedGroupController implementation:
  void CreateEndpointHandlePair(
      ScopedInterfaceEndpointHandle* local_endpoint,
      ScopedInterfaceEndpointHandle* remote_endpoint) override;
  ScopedInterfaceEndpointHandle CreateLocalEndpointHandle(
      InterfaceId id) override;
  void CloseEndpointHandle(
      InterfaceId id,
      bool is_local,
      const base::Optional<DisconnectReason>& reason) override;
  InterfaceEndpointController* AttachEndpointClient(
      const ScopedInterfaceEndpointHandle& handle,
      InterfaceEndpointClient* endpoint_client,
      scoped_refptr<base::SingleThreadTaskRunner> runner) override;
  void DetachEndpointClient(
      const ScopedInterfaceEndpointHandle& handle) override;
  void RaiseError() override;

  // ---------------------------------------------------------------------------
  // The following public methods are called on the creating thread.

  // Please note that this method shouldn't be called unless it results from an
  // explicit request of the user of bindings (e.g., the user sets an
  // InterfacePtr to null or closes a Binding).
  void CloseMessagePipe();

  // Extracts the underlying message pipe.
  ScopedMessagePipeHandle PassMessagePipe() {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(!HasAssociatedEndpoints());
    return connector_.PassMessagePipe();
  }

  // Blocks the current thread until the first incoming message, or |deadline|.
  bool WaitForIncomingMessage(MojoDeadline deadline) {
    DCHECK(thread_checker_.CalledOnValidThread());
    return connector_.WaitForIncomingMessage(deadline);
  }

  // See Binding for details of pause/resume.
  void PauseIncomingMethodCallProcessing();
  void ResumeIncomingMethodCallProcessing();

  // Whether there are any associated interfaces running currently.
  bool HasAssociatedEndpoints() const;

  // Sets this object to testing mode.
  // In testing mode, the object doesn't disconnect the underlying message pipe
  // when it receives unexpected or invalid messages.
  void EnableTestingMode();

  // Is the router bound to a message pipe handle?
  bool is_valid() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return connector_.is_valid();
  }

  // TODO(yzshen): consider removing this getter.
  MessagePipeHandle handle() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return connector_.handle();
  }

  bool SimulateReceivingMessageForTesting(Message* message) {
    return filters_.Accept(message);
  }

 private:
  class InterfaceEndpoint;
  class MessageWrapper;
  struct Task;

  ~MultiplexRouter() override;

  // MessageReceiver implementation:
  bool Accept(Message* message) override;

  // PipeControlMessageHandlerDelegate implementation:
  bool OnPeerAssociatedEndpointClosed(
      InterfaceId id,
      const base::Optional<DisconnectReason>& reason) override;
  bool OnAssociatedEndpointClosedBeforeSent(InterfaceId id) override;

  void OnPipeConnectionError();

  // Specifies whether we are allowed to directly call into
  // InterfaceEndpointClient (given that we are already on the same thread as
  // the client).
  enum ClientCallBehavior {
    // Don't call any InterfaceEndpointClient methods directly.
    NO_DIRECT_CLIENT_CALLS,
    // Only call InterfaceEndpointClient::HandleIncomingMessage directly to
    // handle sync messages.
    ALLOW_DIRECT_CLIENT_CALLS_FOR_SYNC_MESSAGES,
    // Allow to call any InterfaceEndpointClient methods directly.
    ALLOW_DIRECT_CLIENT_CALLS
  };

  // Processes enqueued tasks (incoming messages and error notifications).
  // |current_task_runner| is only used when |client_call_behavior| is
  // ALLOW_DIRECT_CLIENT_CALLS to determine whether we are on the right task
  // runner to make client calls for async messages or connection error
  // notifications.
  //
  // Note: Because calling into InterfaceEndpointClient may lead to destruction
  // of this object, if direct calls are allowed, the caller needs to hold on to
  // a ref outside of |lock_| before calling this method.
  void ProcessTasks(ClientCallBehavior client_call_behavior,
                    base::SingleThreadTaskRunner* current_task_runner);

  // Processes the first queued sync message for the endpoint corresponding to
  // |id|; returns whether there are more sync messages for that endpoint in the
  // queue.
  //
  // This method is only used by enpoints during sync watching. Therefore, not
  // all sync messages are handled by it.
  bool ProcessFirstSyncMessageForEndpoint(InterfaceId id);

  // Returns true to indicate that |task|/|message| has been processed.
  bool ProcessNotifyErrorTask(
      Task* task,
      ClientCallBehavior client_call_behavior,
      base::SingleThreadTaskRunner* current_task_runner);
  bool ProcessIncomingMessage(
      Message* message,
      ClientCallBehavior client_call_behavior,
      base::SingleThreadTaskRunner* current_task_runner);

  void MaybePostToProcessTasks(base::SingleThreadTaskRunner* task_runner);
  void LockAndCallProcessTasks();

  // Updates the state of |endpoint|. If both the endpoint and its peer have
  // been closed, removes it from |endpoints_|.
  // NOTE: The method may invalidate |endpoint|.
  enum EndpointStateUpdateType { ENDPOINT_CLOSED, PEER_ENDPOINT_CLOSED };
  void UpdateEndpointStateMayRemove(InterfaceEndpoint* endpoint,
                                    EndpointStateUpdateType type);

  void RaiseErrorInNonTestingMode();

  InterfaceEndpoint* FindOrInsertEndpoint(InterfaceId id, bool* inserted);
  InterfaceEndpoint* FindEndpoint(InterfaceId id);

  void AssertLockAcquired();

  // Whether to set the namespace bit when generating interface IDs. Please see
  // comments of kInterfaceIdNamespaceMask.
  const bool set_interface_id_namespace_bit_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Owned by |filters_| below.
  MessageHeaderValidator* header_validator_;

  FilterChain filters_;
  Connector connector_;

  base::ThreadChecker thread_checker_;

  // Protects the following members.
  // Sets to nullptr in Config::SINGLE_INTERFACE* mode.
  std::unique_ptr<base::Lock> lock_;
  PipeControlMessageHandler control_message_handler_;

  // NOTE: It is unsafe to call into this object while holding |lock_|.
  PipeControlMessageProxy control_message_proxy_;

  std::map<InterfaceId, scoped_refptr<InterfaceEndpoint>> endpoints_;
  uint32_t next_interface_id_value_;

  std::deque<std::unique_ptr<Task>> tasks_;
  // It refers to tasks in |tasks_| and doesn't own any of them.
  std::map<InterfaceId, std::deque<Task*>> sync_message_tasks_;

  bool posted_to_process_tasks_;
  scoped_refptr<base::SingleThreadTaskRunner> posted_to_task_runner_;

  bool encountered_error_;

  bool paused_;

  bool testing_mode_;

  DISALLOW_COPY_AND_ASSIGN(MultiplexRouter);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_MULTIPLEX_ROUTER_H_
