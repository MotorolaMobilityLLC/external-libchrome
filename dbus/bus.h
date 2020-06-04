
// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_BUS_H_
#define DBUS_BUS_H_
#pragma once

#include <set>
#include <string>
#include <dbus/dbus.h>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/tracked_objects.h"

class MessageLoop;

namespace base {
class Thread;
}

namespace dbus {

class ExportedObject;
class ObjectProxy;

// Bus is used to establish a connection with D-Bus, create object
// proxies, and export objects.
//
// For asynchronous operations such as an asynchronous method call, the
// bus object will use a message loop to monitor the underlying file
// descriptor used for D-Bus communication. By default, the bus will use
// the current thread's MessageLoopForIO. If |dbus_thread| option is
// specified, the bus will use the D-Bus thread's message loop.
//
// THREADING
//
// In the D-Bus library, we use the two threads:
//
// - The origin thread: the thread that created the Bus object.
// - The D-Bus thread: the thread supplifed by |dbus_thread| option.
//
// The origin thread is usually Chrome's UI thread. The D-Bus thread is
// usually a dedicated thread for the D-Bus library.
//
// BLOCKING CALLS
//
// Functions that issue blocking calls are marked "BLOCKING CALL" and
// these functions should be called in the D-Bus thread (if
// supplied). AssertOnDBusThread() is placed in these functions.
//
// Note that it's hard to tell if a libdbus function is actually blocking
// or not (ex. dbus_bus_request_name() internally calls
// dbus_connection_send_with_reply_and_block(), which is a blocking
// call). To err on the side, we consider all libdbus functions that deal
// with the connection to dbus-damoen to be blocking.
//
// EXAMPLE USAGE:
//
// Synchronous method call:
//
//   dbus::Bus::Options options;
//   // Set up the bus options here.
//   ...
//   dbus::Bus bus(options);
//
//   dbus::ObjectProxy* object_proxy =
//       bus.GetObjectProxy(service_name, object_path);
//
//   dbus::MethodCall method_call(interface_name, method_name);
//   dbus::Response response;
//   bool success =
//       object_proxy.CallMethodAndBlock(&method_call, timeout_ms, &response);
//
// Asynchronous method call:
//
//   void OnResponse(dbus::Response* response) {
//     // response is NULL if the method call failed.
//     if (!response)
//       return;
//   }
//
//   ...
//   object_proxy.CallMethod(&method_call, timeout_ms,
//                           base::Bind(&OnResponse));
//
// Exporting a method:
//
//   Response* Echo(dbus::MethodCall* method_call) {
//     // Do something with method_call.
//     Response* response = Response::FromMethodCall(method_call);
//     // Build response here.
//     return response;
//   }
//
//   void OnExported(const std::string& interface_name,
//                   const std::string& object_path,
//                   bool success) {
//     // success is true if the method was exported successfully.
//   }
//
//   ...
//   dbus::ExportedObject* exported_object =
//       bus.GetExportedObject(service_name, object_path);
//   exported_object.ExportMethod(interface_name, method_name,
//                                base::Bind(&Echo),
//                                base::Bind(&OnExported));
//
// WHY IS THIS A REF COUNTED OBJECT?
//
// Bus is a ref counted object, to ensure that |this| of the object is
// alive when callbacks referencing |this| are called. However, after
// Shutdown() is called, |connection_| can be NULL. Hence, calbacks should
// not rely on that |connection_| is alive.
class Bus : public base::RefCountedThreadSafe<Bus> {
 public:
  // Specifies the bus type. SESSION is used to communicate with per-user
  // services like GNOME applications. SYSTEM is used to communicate with
  // system-wide services like NetworkManager.
  enum BusType {
    SESSION = DBUS_BUS_SESSION,
    SYSTEM  = DBUS_BUS_SYSTEM,
  };

  // Specifies the connection type. PRIVATE should usually be used unless
  // you are sure that SHARED is safe for you, which is unlikely the case
  // in Chrome.
  //
  // PRIVATE gives you a private connection, that won't be shared with
  // other Bus objects.
  //
  // SHARED gives you a connection shared among other Bus objects, which
  // is unsafe if the connection is shared with multiple threads.
  enum ConnectionType {
    PRIVATE,
    SHARED,
  };

  // Options used to create a Bus object.
  struct Options {
    Options();
    ~Options();

    BusType bus_type;  // SESSION by default.
    ConnectionType connection_type;  // PRIVATE by default.
    // If the thread is set, the bus object will use the message loop
    // attached to the thread to process asynchronous operations.
    //
    // The thread should meet the following requirements:
    // 1) Already running.
    // 2) Has a MessageLoopForIO.
    // 3) Outlives the bus.
    base::Thread* dbus_thread;  // NULL by default.
  };

  // Called when shutdown is done. Used for Shutdown().
  typedef base::Callback<void ()> OnShutdownCallback;

  // Creates a Bus object. The actual connection will be established when
  // Connect() is called.
  explicit Bus(const Options& options);

  // Gets the object proxy for the given service name and the object path.
  // The caller must not delete the returned object. The bus will own the
  // object. Never returns NULL.
  //
  // The object proxy is used to call remote methods.
  //
  // |service_name| looks like "org.freedesktop.NetworkManager", and
  // |object_path| looks like "/org/freedesktop/NetworkManager/Devices/0".
  //
  // Must be called in the origin thread.
  virtual ObjectProxy* GetObjectProxy(const std::string& service_name,
                                      const std::string& object_path);

  // Gets the exported object for the given service name and the object
  // path. The caller must not delete the returned object. The bus will
  // own the object. Never returns NULL.
  //
  // The exported object is used to export objects to other D-Bus clients.
  //
  // Must be called in the origin thread.
  virtual ExportedObject* GetExportedObject(const std::string& service_name,
                                            const std::string& object_path);

  // Shuts down the bus and blocks until it's done. More specifically, this
  // function does the following:
  //
  // - Unregisters the object paths
  // - Releases the service names
  // - Closes the connection to dbus-daemon.
  //
  // BLOCKING CALL.
  virtual void ShutdownAndBlock();

  // Shuts down the bus in the D-Bus thread. |callback| will be called in
  // the origin thread.
  //
  // Must be called in the origin thread.
  virtual void Shutdown(OnShutdownCallback callback);

  //
  // The public functions below are not intended to be used in client
  // code. These are used to implement ObjectProxy and ExportedObject.
  //

  // Connects the bus to the dbus-daemon.
  // Returns true on success, or the bus is already connected.
  //
  // BLOCKING CALL.
  virtual bool Connect();

  // Requests the ownership of the given service name.
  // Returns true on success, or the the service name is already obtained.
  //
  // BLOCKING CALL.
  virtual bool RequestOwnership(const std::string& service_name);

  // Releases the ownership of the given service name.
  // Returns true on success.
  //
  // BLOCKING CALL.
  virtual bool ReleaseOwnership(const std::string& service_name);

  // Sets up async operations.
  // Returns true on success, or it's already set up.
  // This function needs to be called before starting async operations.
  //
  // BLOCKING CALL.
  virtual bool SetUpAsyncOperations();

  // Sends a message to the bus and blocks until the response is
  // received. Used to implement synchronous method calls.
  //
  // BLOCKING CALL.
  virtual DBusMessage* SendWithReplyAndBlock(DBusMessage* request,
                                             int timeout_ms,
                                             DBusError* error);

  // Requests to send a message to the bus.
  //
  // BLOCKING CALL.
  virtual void SendWithReply(DBusMessage* request,
                             DBusPendingCall** pending_call,
                             int timeout_ms);

  // Tries to register the object path.
  //
  // BLOCKING CALL.
  virtual bool TryRegisterObjectPath(const std::string& object_path,
                                     const DBusObjectPathVTable* vtable,
                                     void* user_data,
                                     DBusError* error);

  // Unregister the object path.
  //
  // BLOCKING CALL.
  virtual void UnregisterObjectPath(const std::string& object_path);

  // Posts the task to the message loop of the thread that created the bus.
  virtual void PostTaskToOriginThread(
      const tracked_objects::Location& from_here,
      const base::Closure& task);

  // Posts the task to the message loop of the D-Bus thread. If D-Bus
  // thread is not supplied, the message loop of the origin thread will be
  // used.
  virtual void PostTaskToDBusThread(
      const tracked_objects::Location& from_here,
      const base::Closure& task);

  // Posts the delayed task to the message loop of the D-Bus thread. If
  // D-Bus thread is not supplied, the message loop of the origin thread
  // will be used.
  virtual void PostDelayedTaskToDBusThread(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      int delay_ms);

  // Returns true if the bus has the D-Bus thread.
  virtual bool HasDBusThread();

  // Check whether the current thread is on the origin thread (the thread
  // that created the bus). If not, DCHECK will fail.
  virtual void AssertOnOriginThread();

  // Check whether the current thread is on the D-Bus thread. If not,
  // DCHECK will fail. If the D-Bus thread is not supplied, it calls
  // AssertOnOriginThread().
  virtual void AssertOnDBusThread();

 private:
  friend class base::RefCountedThreadSafe<Bus>;
  virtual ~Bus();

  // Helper function used for Shutdown().
  void ShutdownInternal(OnShutdownCallback callback);

  // Processes the all incoming data to the connection, if any.
  //
  // BLOCKING CALL.
  void ProcessAllIncomingDataIfAny();

  // Called when a watch object is added. Used to start monitoring the
  // file descriptor used for D-Bus communication.
  dbus_bool_t OnAddWatch(DBusWatch* raw_watch);

  // Called when a watch object is removed.
  void OnRemoveWatch(DBusWatch* raw_watch);

  // Called when the "enabled" status of |raw_watch| is toggled.
  void OnToggleWatch(DBusWatch* raw_watch);

  // Called when a timeout object is added. Used to start monitoring
  // timeout for method calls.
  dbus_bool_t OnAddTimeout(DBusTimeout* raw_timeout);

  // Called when a timeout object is removed.
  void OnRemoveTimeout(DBusTimeout* raw_timeout);

  // Called when the "enabled" status of |raw_timeout| is toggled.
  void OnToggleTimeout(DBusTimeout* raw_timeout);

  // Called when the dispatch status (i.e. if any incoming data is
  // available) is changed.
  void OnDispatchStatusChanged(DBusConnection* connection,
                               DBusDispatchStatus status);

  // Callback helper functions. Redirects to the corresponding member function.
  static dbus_bool_t OnAddWatchThunk(DBusWatch* raw_watch, void* data);
  static void OnRemoveWatchThunk(DBusWatch* raw_watch, void* data);
  static void OnToggleWatchThunk(DBusWatch* raw_watch, void* data);
  static dbus_bool_t OnAddTimeoutThunk(DBusTimeout* raw_timeout, void* data);
  static void OnRemoveTimeoutThunk(DBusTimeout* raw_timeout, void* data);
  static void OnToggleTimeoutThunk(DBusTimeout* raw_timeout, void* data);
  static void OnDispatchStatusChangedThunk(DBusConnection* connection,
                                           DBusDispatchStatus status,
                                           void* data);
  const BusType bus_type_;
  const ConnectionType connection_type_;
  base::Thread* dbus_thread_;
  DBusConnection* connection_;

  MessageLoop* origin_loop_;
  base::PlatformThreadId origin_thread_id_;
  base::PlatformThreadId dbus_thread_id_;

  std::set<std::string> owned_service_names_;
  std::vector<scoped_refptr<dbus::ObjectProxy> > object_proxies_;
  std::vector<scoped_refptr<dbus::ExportedObject> > exported_objects_;

  bool async_operations_are_set_up_;

  // Counters to make sure that OnAddWatch()/OnRemoveWatch() and
  // OnAddTimeout()/OnRemoveTimeou() are balanced.
  int num_pending_watches_;
  int num_pending_timeouts_;

  DISALLOW_COPY_AND_ASSIGN(Bus);
};

}  // namespace dbus

#endif  // DBUS_BUS_H_
