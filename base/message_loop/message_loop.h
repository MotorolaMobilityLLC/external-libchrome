// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_LOOP_H_
#define BASE_MESSAGE_LOOP_MESSAGE_LOOP_H_

#include <queue>
#include <string>

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/incoming_task_queue.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/message_loop/message_loop_proxy_impl.h"
#include "base/message_loop/message_pump.h"
#include "base/observer_list.h"
#include "base/pending_task.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/tracking_info.h"

// TODO(sky): these includes should not be necessary. Nuke them.
#if defined(OS_WIN)
#include "base/message_loop/message_pump_win.h"
#elif defined(OS_IOS)
#include "base/message_loop/message_pump_io_ios.h"
#elif defined(OS_POSIX)
#include "base/message_loop/message_pump_libevent.h"
#if !defined(OS_MACOSX) && !defined(OS_ANDROID)

#if defined(USE_AURA) && defined(USE_X11) && !defined(OS_NACL)
#include "base/message_loop/message_pump_x11.h"
#elif !defined(OS_ANDROID_HOST)
#define USE_GTK_MESSAGE_PUMP
#include "base/message_loop/message_pump_gtk.h"
#if defined(TOOLKIT_GTK)
#include "base/message_loop/message_pump_x11.h"
#endif
#endif

#endif
#endif

namespace base {

class HistogramBase;
class MessagePumpObserver;
class RunLoop;
class ThreadTaskRunnerHandle;
#if defined(OS_ANDROID)
class MessagePumpForUI;
#elif defined(OS_ANDROID_HOST)
typedef MessagePumpLibevent MessagePumpForUI;
#endif
class WaitableEvent;

// A MessageLoop is used to process events for a particular thread.  There is
// at most one MessageLoop instance per thread.
//
// Events include at a minimum Task instances submitted to PostTask and its
// variants.  Depending on the type of message pump used by the MessageLoop
// other events such as UI messages may be processed.  On Windows APC calls (as
// time permits) and signals sent to a registered set of HANDLEs may also be
// processed.
//
// NOTE: Unless otherwise specified, a MessageLoop's methods may only be called
// on the thread where the MessageLoop's Run method executes.
//
// NOTE: MessageLoop has task reentrancy protection.  This means that if a
// task is being processed, a second task cannot start until the first task is
// finished.  Reentrancy can happen when processing a task, and an inner
// message pump is created.  That inner pump then processes native messages
// which could implicitly start an inner task.  Inner message pumps are created
// with dialogs (DialogBox), common dialogs (GetOpenFileName), OLE functions
// (DoDragDrop), printer functions (StartDoc) and *many* others.
//
// Sample workaround when inner task processing is needed:
//   HRESULT hr;
//   {
//     MessageLoop::ScopedNestableTaskAllower allow(MessageLoop::current());
//     hr = DoDragDrop(...); // Implicitly runs a modal message loop.
//   }
//   // Process |hr| (the result returned by DoDragDrop()).
//
// Please be SURE your task is reentrant (nestable) and all global variables
// are stable and accessible before calling SetNestableTasksAllowed(true).
//
class BASE_EXPORT MessageLoop : public MessagePump::Delegate {
 public:
#if defined(OS_WIN)
  typedef MessagePumpObserver Observer;
#elif defined(USE_GTK_MESSAGE_PUMP)
  typedef MessagePumpGdkObserver Observer;
#endif

  // A MessageLoop has a particular type, which indicates the set of
  // asynchronous events it may process in addition to tasks and timers.
  //
  // TYPE_DEFAULT
  //   This type of ML only supports tasks and timers.
  //
  // TYPE_UI
  //   This type of ML also supports native UI events (e.g., Windows messages).
  //   See also MessageLoopForUI.
  //
  // TYPE_GPU
  //   This type of ML also supports native UI events for use in the GPU
  //   process. On Linux this will always be an X11 ML (as compared with the
  //   sometimes-GTK ML in the browser process).
  //
  // TYPE_IO
  //   This type of ML also supports asynchronous IO.  See also
  //   MessageLoopForIO.
  //
  // TYPE_JAVA
  //   This type of ML is backed by a Java message handler which is responsible
  //   for running the tasks added to the ML. This is only for use on Android.
  //   TYPE_JAVA behaves in essence like TYPE_UI, except during construction
  //   where it does not use the main thread specific pump factory.
  //
  // TYPE_CUSTOM
  //   MessagePump was supplied to constructor.
  //
  enum Type {
    TYPE_DEFAULT,
    TYPE_UI,
    TYPE_CUSTOM,
#if defined(TOOLKIT_GTK)
    TYPE_GPU,
#endif
    TYPE_IO,
#if defined(OS_ANDROID)
    TYPE_JAVA,
#endif // defined(OS_ANDROID)
  };

  // Normally, it is not necessary to instantiate a MessageLoop.  Instead, it
  // is typical to make use of the current thread's MessageLoop instance.
  explicit MessageLoop(Type type = TYPE_DEFAULT);
  // Creates a TYPE_CUSTOM MessageLoop with the supplied MessagePump, which must
  // be non-NULL.
  explicit MessageLoop(scoped_ptr<base::MessagePump> pump);
  virtual ~MessageLoop();

  // Returns the MessageLoop object for the current thread, or null if none.
  static MessageLoop* current();

  static void EnableHistogrammer(bool enable_histogrammer);

  typedef scoped_ptr<MessagePump> (MessagePumpFactory)();
  // Uses the given base::MessagePumpForUIFactory to override the default
  // MessagePump implementation for 'TYPE_UI'. Returns true if the factory
  // was successfully registered.
  static bool InitMessagePumpForUIFactory(MessagePumpFactory* factory);

  // Creates the default MessagePump based on |type|. Caller owns return
  // value.
  static scoped_ptr<MessagePump> CreateMessagePumpForType(Type type);
  // A DestructionObserver is notified when the current MessageLoop is being
  // destroyed.  These observers are notified prior to MessageLoop::current()
  // being changed to return NULL.  This gives interested parties the chance to
  // do final cleanup that depends on the MessageLoop.
  //
  // NOTE: Any tasks posted to the MessageLoop during this notification will
  // not be run.  Instead, they will be deleted.
  //
  class BASE_EXPORT DestructionObserver {
   public:
    virtual void WillDestroyCurrentMessageLoop() = 0;

   protected:
    virtual ~DestructionObserver();
  };

  // Add a DestructionObserver, which will start receiving notifications
  // immediately.
  void AddDestructionObserver(DestructionObserver* destruction_observer);

  // Remove a DestructionObserver.  It is safe to call this method while a
  // DestructionObserver is receiving a notification callback.
  void RemoveDestructionObserver(DestructionObserver* destruction_observer);

  // The "PostTask" family of methods call the task's Run method asynchronously
  // from within a message loop at some point in the future.
  //
  // With the PostTask variant, tasks are invoked in FIFO order, inter-mixed
  // with normal UI or IO event processing.  With the PostDelayedTask variant,
  // tasks are called after at least approximately 'delay_ms' have elapsed.
  //
  // The NonNestable variants work similarly except that they promise never to
  // dispatch the task from a nested invocation of MessageLoop::Run.  Instead,
  // such tasks get deferred until the top-most MessageLoop::Run is executing.
  //
  // The MessageLoop takes ownership of the Task, and deletes it after it has
  // been Run().
  //
  // PostTask(from_here, task) is equivalent to
  // PostDelayedTask(from_here, task, 0).
  //
  // NOTE: These methods may be called on any thread.  The Task will be invoked
  // on the thread that executes MessageLoop::Run().
  void PostTask(const tracked_objects::Location& from_here,
                const Closure& task);

  void PostDelayedTask(const tracked_objects::Location& from_here,
                       const Closure& task,
                       TimeDelta delay);

  void PostNonNestableTask(const tracked_objects::Location& from_here,
                           const Closure& task);

  void PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  const Closure& task,
                                  TimeDelta delay);

  // A variant on PostTask that deletes the given object.  This is useful
  // if the object needs to live until the next run of the MessageLoop (for
  // example, deleting a RenderProcessHost from within an IPC callback is not
  // good).
  //
  // NOTE: This method may be called on any thread.  The object will be deleted
  // on the thread that executes MessageLoop::Run().  If this is not the same
  // as the thread that calls PostDelayedTask(FROM_HERE, ), then T MUST inherit
  // from RefCountedThreadSafe<T>!
  template <class T>
  void DeleteSoon(const tracked_objects::Location& from_here, const T* object) {
    base::subtle::DeleteHelperInternal<T, void>::DeleteViaSequencedTaskRunner(
        this, from_here, object);
  }

  // A variant on PostTask that releases the given reference counted object
  // (by calling its Release method).  This is useful if the object needs to
  // live until the next run of the MessageLoop, or if the object needs to be
  // released on a particular thread.
  //
  // NOTE: This method may be called on any thread.  The object will be
  // released (and thus possibly deleted) on the thread that executes
  // MessageLoop::Run().  If this is not the same as the thread that calls
  // PostDelayedTask(FROM_HERE, ), then T MUST inherit from
  // RefCountedThreadSafe<T>!
  template <class T>
  void ReleaseSoon(const tracked_objects::Location& from_here,
                   const T* object) {
    base::subtle::ReleaseHelperInternal<T, void>::ReleaseViaSequencedTaskRunner(
        this, from_here, object);
  }

  // Deprecated: use RunLoop instead.
  // Run the message loop.
  void Run();

  // Deprecated: use RunLoop instead.
  // Process all pending tasks, windows messages, etc., but don't wait/sleep.
  // Return as soon as all items that can be run are taken care of.
  void RunUntilIdle();

  // TODO(jbates) remove this. crbug.com/131220. See QuitWhenIdle().
  void Quit() { QuitWhenIdle(); }

  // Deprecated: use RunLoop instead.
  //
  // Signals the Run method to return when it becomes idle. It will continue to
  // process pending messages and future messages as long as they are enqueued.
  // Warning: if the MessageLoop remains busy, it may never quit. Only use this
  // Quit method when looping procedures (such as web pages) have been shut
  // down.
  //
  // This method may only be called on the same thread that called Run, and Run
  // must still be on the call stack.
  //
  // Use QuitClosure variants if you need to Quit another thread's MessageLoop,
  // but note that doing so is fairly dangerous if the target thread makes
  // nested calls to MessageLoop::Run.  The problem being that you won't know
  // which nested run loop you are quitting, so be careful!
  void QuitWhenIdle();

  // Deprecated: use RunLoop instead.
  //
  // This method is a variant of Quit, that does not wait for pending messages
  // to be processed before returning from Run.
  void QuitNow();

  // TODO(jbates) remove this. crbug.com/131220. See QuitWhenIdleClosure().
  static Closure QuitClosure() { return QuitWhenIdleClosure(); }

  // Deprecated: use RunLoop instead.
  // Construct a Closure that will call QuitWhenIdle(). Useful to schedule an
  // arbitrary MessageLoop to QuitWhenIdle.
  static Closure QuitWhenIdleClosure();

  // Returns true if this loop is |type|. This allows subclasses (especially
  // those in tests) to specialize how they are identified.
  virtual bool IsType(Type type) const;

  // Returns the type passed to the constructor.
  Type type() const { return type_; }

  // Optional call to connect the thread name with this loop.
  void set_thread_name(const std::string& thread_name) {
    DCHECK(thread_name_.empty()) << "Should not rename this thread!";
    thread_name_ = thread_name;
  }
  const std::string& thread_name() const { return thread_name_; }

  // Gets the message loop proxy associated with this message loop.
  scoped_refptr<MessageLoopProxy> message_loop_proxy() {
    return message_loop_proxy_;
  }

  // Enables or disables the recursive task processing. This happens in the case
  // of recursive message loops. Some unwanted message loop may occurs when
  // using common controls or printer functions. By default, recursive task
  // processing is disabled.
  //
  // Please utilize |ScopedNestableTaskAllower| instead of calling these methods
  // directly.  In general nestable message loops are to be avoided.  They are
  // dangerous and difficult to get right, so please use with extreme caution.
  //
  // The specific case where tasks get queued is:
  // - The thread is running a message loop.
  // - It receives a task #1 and execute it.
  // - The task #1 implicitly start a message loop, like a MessageBox in the
  //   unit test. This can also be StartDoc or GetSaveFileName.
  // - The thread receives a task #2 before or while in this second message
  //   loop.
  // - With NestableTasksAllowed set to true, the task #2 will run right away.
  //   Otherwise, it will get executed right after task #1 completes at "thread
  //   message loop level".
  void SetNestableTasksAllowed(bool allowed);
  bool NestableTasksAllowed() const;

  // Enables nestable tasks on |loop| while in scope.
  class ScopedNestableTaskAllower {
   public:
    explicit ScopedNestableTaskAllower(MessageLoop* loop)
        : loop_(loop),
          old_state_(loop_->NestableTasksAllowed()) {
      loop_->SetNestableTasksAllowed(true);
    }
    ~ScopedNestableTaskAllower() {
      loop_->SetNestableTasksAllowed(old_state_);
    }

   private:
    MessageLoop* loop_;
    bool old_state_;
  };

  // Returns true if we are currently running a nested message loop.
  bool IsNested();

  // A TaskObserver is an object that receives task notifications from the
  // MessageLoop.
  //
  // NOTE: A TaskObserver implementation should be extremely fast!
  class BASE_EXPORT TaskObserver {
   public:
    TaskObserver();

    // This method is called before processing a task.
    virtual void WillProcessTask(const PendingTask& pending_task) = 0;

    // This method is called after processing a task.
    virtual void DidProcessTask(const PendingTask& pending_task) = 0;

   protected:
    virtual ~TaskObserver();
  };

  // These functions can only be called on the same thread that |this| is
  // running on.
  void AddTaskObserver(TaskObserver* task_observer);
  void RemoveTaskObserver(TaskObserver* task_observer);

  // When we go into high resolution timer mode, we will stay in hi-res mode
  // for at least 1s.
  static const int kHighResolutionTimerModeLeaseTimeMs = 1000;

#if defined(OS_WIN)
  void set_os_modal_loop(bool os_modal_loop) {
    os_modal_loop_ = os_modal_loop;
  }

  bool os_modal_loop() const {
    return os_modal_loop_;
  }
#endif  // OS_WIN

  // Can only be called from the thread that owns the MessageLoop.
  bool is_running() const;

  // Returns true if the message loop has high resolution timers enabled.
  // Provided for testing.
  bool IsHighResolutionTimerEnabledForTesting();

  // Returns true if the message loop is "idle". Provided for testing.
  bool IsIdleForTesting();

  //----------------------------------------------------------------------------
 protected:

#if defined(OS_WIN)
  MessagePumpWin* pump_win() {
    return static_cast<MessagePumpWin*>(pump_.get());
  }
#elif defined(OS_POSIX) && !defined(OS_IOS)
  MessagePumpLibevent* pump_libevent() {
    return static_cast<MessagePumpLibevent*>(pump_.get());
  }
#if defined(TOOLKIT_GTK)
  friend class MessagePumpX11;
  MessagePumpX11* pump_gpu() {
    DCHECK_EQ(TYPE_GPU, type());
    return static_cast<MessagePumpX11*>(pump_.get());
  }
#endif
#endif

  scoped_ptr<MessagePump> pump_;

 private:
  friend class internal::IncomingTaskQueue;
  friend class RunLoop;

  // Configures various members for the two constructors.
  void Init();

  // Invokes the actual run loop using the message pump.
  void RunHandler();

  // Called to process any delayed non-nestable tasks.
  bool ProcessNextDelayedNonNestableTask();

  // Runs the specified PendingTask.
  void RunTask(const PendingTask& pending_task);

  // Calls RunTask or queues the pending_task on the deferred task list if it
  // cannot be run right now.  Returns true if the task was run.
  bool DeferOrRunPendingTask(const PendingTask& pending_task);

  // Adds the pending task to delayed_work_queue_.
  void AddToDelayedWorkQueue(const PendingTask& pending_task);

  // Delete tasks that haven't run yet without running them.  Used in the
  // destructor to make sure all the task's destructors get called.  Returns
  // true if some work was done.
  bool DeletePendingTasks();

  // Creates a process-wide unique ID to represent this task in trace events.
  // This will be mangled with a Process ID hash to reduce the likelyhood of
  // colliding with MessageLoop pointers on other processes.
  uint64 GetTaskTraceID(const PendingTask& task);

  // Loads tasks from the incoming queue to |work_queue_| if the latter is
  // empty.
  void ReloadWorkQueue();

  // Wakes up the message pump. Can be called on any thread. The caller is
  // responsible for synchronizing ScheduleWork() calls.
  void ScheduleWork(bool was_empty);

  // Start recording histogram info about events and action IF it was enabled
  // and IF the statistics recorder can accept a registration of our histogram.
  void StartHistogrammer();

  // Add occurrence of event to our histogram, so that we can see what is being
  // done in a specific MessageLoop instance (i.e., specific thread).
  // If message_histogram_ is NULL, this is a no-op.
  void HistogramEvent(int event);

  // MessagePump::Delegate methods:
  virtual bool DoWork() OVERRIDE;
  virtual bool DoDelayedWork(TimeTicks* next_delayed_work_time) OVERRIDE;
  virtual bool DoIdleWork() OVERRIDE;
  virtual void GetQueueingInformation(size_t* queue_size,
                                      TimeDelta* queueing_delay) OVERRIDE;

  const Type type_;

  // A list of tasks that need to be processed by this instance.  Note that
  // this queue is only accessed (push/pop) by our current thread.
  TaskQueue work_queue_;

  // Contains delayed tasks, sorted by their 'delayed_run_time' property.
  DelayedTaskQueue delayed_work_queue_;

  // A recent snapshot of Time::Now(), used to check delayed_work_queue_.
  TimeTicks recent_time_;

  // A queue of non-nestable tasks that we had to defer because when it came
  // time to execute them we were in a nested message loop.  They will execute
  // once we're out of nested message loops.
  TaskQueue deferred_non_nestable_work_queue_;

  ObserverList<DestructionObserver> destruction_observers_;

  // A recursion block that prevents accidentally running additional tasks when
  // insider a (accidentally induced?) nested message pump.
  bool nestable_tasks_allowed_;

#if defined(OS_WIN)
  // Should be set to true before calling Windows APIs like TrackPopupMenu, etc
  // which enter a modal message loop.
  bool os_modal_loop_;
#endif

  std::string thread_name_;
  // A profiling histogram showing the counts of various messages and events.
  HistogramBase* message_histogram_;

  RunLoop* run_loop_;

  ObserverList<TaskObserver> task_observers_;

  scoped_refptr<internal::IncomingTaskQueue> incoming_task_queue_;

  // The message loop proxy associated with this message loop.
  scoped_refptr<internal::MessageLoopProxyImpl> message_loop_proxy_;
  scoped_ptr<ThreadTaskRunnerHandle> thread_task_runner_handle_;

  template <class T, class R> friend class base::subtle::DeleteHelperInternal;
  template <class T, class R> friend class base::subtle::ReleaseHelperInternal;

  void DeleteSoonInternal(const tracked_objects::Location& from_here,
                          void(*deleter)(const void*),
                          const void* object);
  void ReleaseSoonInternal(const tracked_objects::Location& from_here,
                           void(*releaser)(const void*),
                           const void* object);

  DISALLOW_COPY_AND_ASSIGN(MessageLoop);
};

//-----------------------------------------------------------------------------
// MessageLoopForUI extends MessageLoop with methods that are particular to a
// MessageLoop instantiated with TYPE_UI.
//
// This class is typically used like so:
//   MessageLoopForUI::current()->...call some method...
//
class BASE_EXPORT MessageLoopForUI : public MessageLoop {
 public:
  MessageLoopForUI() : MessageLoop(TYPE_UI) {
  }

  // Returns the MessageLoopForUI of the current thread.
  static MessageLoopForUI* current() {
    MessageLoop* loop = MessageLoop::current();
    DCHECK(loop);
    DCHECK_EQ(MessageLoop::TYPE_UI, loop->type());
    return static_cast<MessageLoopForUI*>(loop);
  }

  static bool IsCurrent() {
    MessageLoop* loop = MessageLoop::current();
    return loop && loop->type() == MessageLoop::TYPE_UI;
  }

#if defined(OS_IOS)
  // On iOS, the main message loop cannot be Run().  Instead call Attach(),
  // which connects this MessageLoop to the UI thread's CFRunLoop and allows
  // PostTask() to work.
  void Attach();
#endif

#if defined(OS_ANDROID)
  // On Android, the UI message loop is handled by Java side. So Run() should
  // never be called. Instead use Start(), which will forward all the native UI
  // events to the Java message loop.
  void Start();
#endif

#if !defined(OS_NACL) && defined(OS_WIN)
  // Please see message_pump_win for definitions of these methods.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
#endif

#if defined(USE_OZONE) && !defined(OS_NACL)
  // Please see MessagePumpLibevent for definition.
  bool WatchFileDescriptor(
      int fd,
      bool persistent,
      MessagePumpLibevent::Mode mode,
      MessagePumpLibevent::FileDescriptorWatcher* controller,
      MessagePumpLibevent::Watcher* delegate);
#endif

 protected:
#if defined(USE_X11)
  friend class MessagePumpX11;
#endif

#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
  // TODO(rvargas): Make this platform independent.
  MessagePumpForUI* pump_ui() {
    return static_cast<MessagePumpForUI*>(pump_.get());
  }
#endif
};

// Do not add any member variables to MessageLoopForUI!  This is important b/c
// MessageLoopForUI is often allocated via MessageLoop(TYPE_UI).  Any extra
// data that you need should be stored on the MessageLoop's pump_ instance.
COMPILE_ASSERT(sizeof(MessageLoop) == sizeof(MessageLoopForUI),
               MessageLoopForUI_should_not_have_extra_member_variables);

//-----------------------------------------------------------------------------
// MessageLoopForIO extends MessageLoop with methods that are particular to a
// MessageLoop instantiated with TYPE_IO.
//
// This class is typically used like so:
//   MessageLoopForIO::current()->...call some method...
//
class BASE_EXPORT MessageLoopForIO : public MessageLoop {
 public:
#if defined(OS_WIN)
  typedef MessagePumpForIO::IOHandler IOHandler;
  typedef MessagePumpForIO::IOContext IOContext;
  typedef MessagePumpForIO::IOObserver IOObserver;
#elif defined(OS_IOS)
  typedef MessagePumpIOSForIO::Watcher Watcher;
  typedef MessagePumpIOSForIO::FileDescriptorWatcher
      FileDescriptorWatcher;
  typedef MessagePumpIOSForIO::IOObserver IOObserver;

  enum Mode {
    WATCH_READ = MessagePumpIOSForIO::WATCH_READ,
    WATCH_WRITE = MessagePumpIOSForIO::WATCH_WRITE,
    WATCH_READ_WRITE = MessagePumpIOSForIO::WATCH_READ_WRITE
  };
#elif defined(OS_POSIX)
  typedef MessagePumpLibevent::Watcher Watcher;
  typedef MessagePumpLibevent::FileDescriptorWatcher
      FileDescriptorWatcher;
  typedef MessagePumpLibevent::IOObserver IOObserver;

  enum Mode {
    WATCH_READ = MessagePumpLibevent::WATCH_READ,
    WATCH_WRITE = MessagePumpLibevent::WATCH_WRITE,
    WATCH_READ_WRITE = MessagePumpLibevent::WATCH_READ_WRITE
  };

#endif

  MessageLoopForIO() : MessageLoop(TYPE_IO) {
  }

  // Returns the MessageLoopForIO of the current thread.
  static MessageLoopForIO* current() {
    MessageLoop* loop = MessageLoop::current();
    DCHECK_EQ(MessageLoop::TYPE_IO, loop->type());
    return static_cast<MessageLoopForIO*>(loop);
  }

  static bool IsCurrent() {
    MessageLoop* loop = MessageLoop::current();
    return loop && loop->type() == MessageLoop::TYPE_IO;
  }

  void AddIOObserver(IOObserver* io_observer) {
    pump_io()->AddIOObserver(io_observer);
  }

  void RemoveIOObserver(IOObserver* io_observer) {
    pump_io()->RemoveIOObserver(io_observer);
  }

#if defined(OS_WIN)
  // Please see MessagePumpWin for definitions of these methods.
  void RegisterIOHandler(HANDLE file, IOHandler* handler);
  bool RegisterJobObject(HANDLE job, IOHandler* handler);
  bool WaitForIOCompletion(DWORD timeout, IOHandler* filter);

 protected:
  // TODO(rvargas): Make this platform independent.
  MessagePumpForIO* pump_io() {
    return static_cast<MessagePumpForIO*>(pump_.get());
  }

#elif defined(OS_IOS)
  // Please see MessagePumpIOSForIO for definition.
  bool WatchFileDescriptor(int fd,
                           bool persistent,
                           Mode mode,
                           FileDescriptorWatcher *controller,
                           Watcher *delegate);

 private:
  MessagePumpIOSForIO* pump_io() {
    return static_cast<MessagePumpIOSForIO*>(pump_.get());
  }

#elif defined(OS_POSIX)
  // Please see MessagePumpLibevent for definition.
  bool WatchFileDescriptor(int fd,
                           bool persistent,
                           Mode mode,
                           FileDescriptorWatcher* controller,
                           Watcher* delegate);

 private:
  MessagePumpLibevent* pump_io() {
    return static_cast<MessagePumpLibevent*>(pump_.get());
  }
#endif  // defined(OS_POSIX)
};

// Do not add any member variables to MessageLoopForIO!  This is important b/c
// MessageLoopForIO is often allocated via MessageLoop(TYPE_IO).  Any extra
// data that you need should be stored on the MessageLoop's pump_ instance.
COMPILE_ASSERT(sizeof(MessageLoop) == sizeof(MessageLoopForIO),
               MessageLoopForIO_should_not_have_extra_member_variables);

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_LOOP_H_
