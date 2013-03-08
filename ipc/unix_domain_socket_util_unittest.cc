// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/socket.h>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "ipc/unix_domain_socket_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SocketAcceptor : public MessageLoopForIO::Watcher {
 public:
  SocketAcceptor(int fd, base::MessageLoopProxy* target_thread)
      : server_fd_(-1),
        started_watching_event_(false, false),
        accepted_event_(false, false) {
    target_thread->PostTask(FROM_HERE,
        base::Bind(&SocketAcceptor::StartWatching, base::Unretained(this), fd));
  }

  virtual ~SocketAcceptor() {
    watcher_.StopWatchingFileDescriptor();
  }

  int server_fd() const { return server_fd_; }

  void WaitUntilReady() {
    started_watching_event_.Wait();
  }
  void WaitForAccept() {
    accepted_event_.Wait();
  }

 private:
  void StartWatching(int fd) {
    MessageLoopForIO::current()->WatchFileDescriptor(
        fd,
        true,
        MessageLoopForIO::WATCH_READ,
        &watcher_,
        this);
    started_watching_event_.Signal();
  }
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE {
    ASSERT_EQ(-1, server_fd_);
    IPC::ServerAcceptConnection(fd, &server_fd_);
    watcher_.StopWatchingFileDescriptor();
    accepted_event_.Signal();
  }
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE {}

  int server_fd_;
  MessageLoopForIO::FileDescriptorWatcher watcher_;
  base::WaitableEvent started_watching_event_;
  base::WaitableEvent accepted_event_;

  DISALLOW_COPY_AND_ASSIGN(SocketAcceptor);
};

const base::FilePath GetChannelDir() {
#if defined(OS_ANDROID)
  base::FilePath tmp_dir;
  PathService::Get(base::DIR_CACHE, &tmp_dir);
  return tmp_dir;
#else
  return base::FilePath("/var/tmp");
#endif
}

class TestUnixSocketConnection {
 public:
  TestUnixSocketConnection()
      : worker_("WorkerThread"),
        server_listen_fd_(-1),
        server_fd_(-1),
        client_fd_(-1) {
    socket_name_ = GetChannelDir().Append("TestSocket");
    base::Thread::Options options;
    options.message_loop_type = MessageLoop::TYPE_IO;
    worker_.StartWithOptions(options);
  }

  bool CreateServerSocket() {
    IPC::CreateServerUnixDomainSocket(socket_name_, &server_listen_fd_);
    if (server_listen_fd_ < 0)
      return false;
    struct stat socket_stat;
    stat(socket_name_.value().c_str(), &socket_stat);
    EXPECT_TRUE(S_ISSOCK(socket_stat.st_mode));
    acceptor_.reset(new SocketAcceptor(server_listen_fd_,
                                       worker_.message_loop_proxy()));
    acceptor_->WaitUntilReady();
    return true;
  }

  bool CreateClientSocket() {
    DCHECK(server_listen_fd_ >= 0);
    IPC::CreateClientUnixDomainSocket(socket_name_, &client_fd_);
    if (client_fd_ < 0)
      return false;
    acceptor_->WaitForAccept();
    server_fd_ = acceptor_->server_fd();
    return server_fd_ >= 0;
  }

  virtual ~TestUnixSocketConnection() {
    if (client_fd_ >= 0)
      close(client_fd_);
    if (server_fd_ >= 0)
      close(server_fd_);
    if (server_listen_fd_ >= 0) {
      close(server_listen_fd_);
      unlink(socket_name_.value().c_str());
    }
  }

  int client_fd() const { return client_fd_; }
  int server_fd() const { return server_fd_; }

 private:
  base::Thread worker_;
  base::FilePath socket_name_;
  int server_listen_fd_;
  int server_fd_;
  int client_fd_;
  scoped_ptr<SocketAcceptor> acceptor_;
};

// Ensure that IPC::CreateServerUnixDomainSocket creates a socket that
// IPC::CreateClientUnixDomainSocket can successfully connect to.
TEST(UnixDomainSocketUtil, Connect) {
  TestUnixSocketConnection connection;
  ASSERT_TRUE(connection.CreateServerSocket());
  ASSERT_TRUE(connection.CreateClientSocket());
}

// Ensure that messages can be sent across the resulting socket.
TEST(UnixDomainSocketUtil, SendReceive) {
  TestUnixSocketConnection connection;
  ASSERT_TRUE(connection.CreateServerSocket());
  ASSERT_TRUE(connection.CreateClientSocket());

  const char buffer[] = "Hello, server!";
  size_t buf_len = sizeof(buffer);
  size_t sent_bytes =
      HANDLE_EINTR(send(connection.client_fd(), buffer, buf_len, 0));
  ASSERT_EQ(buf_len, sent_bytes);
  char recv_buf[sizeof(buffer)];
  size_t received_bytes =
      HANDLE_EINTR(recv(connection.server_fd(), recv_buf, buf_len, 0));
  ASSERT_EQ(buf_len, received_bytes);
  ASSERT_EQ(0, memcmp(recv_buf, buffer, buf_len));
}

}  // namespace
