// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_pump_x.h"

#include <X11/extensions/XInput2.h>

#include "base/basictypes.h"
#include "base/message_loop.h"

#if defined(TOOLKIT_USES_GTK)
#include <gdk/gdkx.h>
#endif

namespace {

gboolean XSourcePrepare(GSource* source, gint* timeout_ms) {
  if (XPending(base::MessagePumpX::GetDefaultXDisplay()))
    *timeout_ms = 0;
  else
    *timeout_ms = -1;
  return FALSE;
}

gboolean XSourceCheck(GSource* source) {
  return XPending(base::MessagePumpX::GetDefaultXDisplay());
}

gboolean XSourceDispatch(GSource* source,
                         GSourceFunc unused_func,
                         gpointer unused_data) {
  // TODO(sad): When GTK event proecssing is completely removed, the event
  // processing and dispatching should be done here (i.e. XNextEvent,
  // ProcessXEvent etc.)
  return TRUE;
}

GSourceFuncs XSourceFuncs = {
  XSourcePrepare,
  XSourceCheck,
  XSourceDispatch,
  NULL
};

// The opcode used for checking events.
int xiopcode = -1;

#if defined(TOOLKIT_USES_GTK)
gboolean PlaceholderDispatch(GSource* source,
                             GSourceFunc cb,
                             gpointer data) {
  return TRUE;
}
#else
// If the GTK/GDK event processing is not present, the message-pump opens a
// connection to the display and owns it.
Display* g_xdisplay = NULL;
#endif  // defined(TOOLKIT_USES_GTK)

void InitializeXInput2(void) {
  Display* display = base::MessagePumpX::GetDefaultXDisplay();
  if (!display)
    return;

  int event, err;

  if (!XQueryExtension(display, "XInputExtension", &xiopcode, &event, &err)) {
    VLOG(1) << "X Input extension not available.";
    xiopcode = -1;
    return;
  }

#if defined(USE_XI2_MT)
  // USE_XI2_MT also defines the required XI2 minor minimum version.
  int major = 2, minor = USE_XI2_MT;
#else
  int major = 2, minor = 0;
#endif
  if (XIQueryVersion(display, &major, &minor) == BadRequest) {
    VLOG(1) << "XInput2 not supported in the server.";
    xiopcode = -1;
    return;
  }
#if defined(USE_XI2_MT)
  if (major < 2 || (major == 2 && minor < USE_XI2_MT)) {
    VLOG(1) << "XI version on server is " << major << "." << minor << ". "
            << "But 2." << USE_XI2_MT << " is required.";
    xiopcode = -1;
    return;
  }
#endif
}

}  // namespace

namespace base {

MessagePumpX::MessagePumpX() : MessagePumpGlib(),
#if defined(TOOLKIT_USES_GTK)
    gdksource_(NULL),
    dispatching_event_(false),
    capture_x_events_(0),
    capture_gdk_events_(0),
#endif
    x_source_(NULL) {
  InitializeXInput2();
#if defined(TOOLKIT_USES_GTK)
  gdk_window_add_filter(NULL, &GdkEventFilter, this);
  gdk_event_handler_set(&EventDispatcherX, this, NULL);
  InitializeEventsToCapture();
#else
  InitXSource();
#endif
}

MessagePumpX::~MessagePumpX() {
#if defined(TOOLKIT_USES_GTK)
  gdk_window_remove_filter(NULL, &GdkEventFilter, this);
  gdk_event_handler_set(reinterpret_cast<GdkEventFunc>(gtk_main_do_event),
                        this, NULL);
#else
  g_source_destroy(x_source_);
  g_source_unref(x_source_);
  XCloseDisplay(g_xdisplay);
  g_xdisplay = NULL;
#endif
}

// static
Display* MessagePumpX::GetDefaultXDisplay() {
#if defined(TOOLKIT_USES_GTK)
  static GdkDisplay* display = gdk_display_get_default();
  return display ? GDK_DISPLAY_XDISPLAY(display) : NULL;
#else
  if (!g_xdisplay)
    g_xdisplay = XOpenDisplay(NULL);
  return g_xdisplay;
#endif
}

// static
bool MessagePumpX::HasXInput2() {
  return xiopcode != -1;
}

void MessagePumpX::InitXSource() {
  DCHECK(!x_source_);
  GPollFD* x_poll = new GPollFD();
  Display* display = GetDefaultXDisplay();
  CHECK(display) << "Unable to get connection to X server";
  x_poll->fd = ConnectionNumber(display);
  x_poll->events = G_IO_IN;

  x_source_ = g_source_new(&XSourceFuncs, sizeof(GSource));
  g_source_add_poll(x_source_, x_poll);
  g_source_set_can_recurse(x_source_, FALSE);
  g_source_attach(x_source_, g_main_context_default());
}

bool MessagePumpX::ShouldCaptureXEvent(XEvent* xev) {
#if defined(TOOLKIT_USES_GTK)
  return capture_x_events_[xev->type] &&
      (xev->type != GenericEvent || xev->xcookie.extension == xiopcode);
#else
  // When not using GTK, we always handle all events ourselves, and always have
  // to remove it from the queue, whether we do anything with it or not.
  return true;
#endif
}

bool MessagePumpX::ProcessXEvent(XEvent* xev) {
  bool should_quit = false;

  bool have_cookie = false;
  if (xev->type == GenericEvent &&
      XGetEventData(xev->xgeneric.display, &xev->xcookie)) {
    have_cookie = true;
  }

  if (WillProcessXEvent(xev) == EVENT_CONTINUE) {
    MessagePumpDispatcher::DispatchStatus status =
        GetDispatcher()->Dispatch(xev);

    if (status == MessagePumpDispatcher::EVENT_QUIT) {
      should_quit = true;
      Quit();
    } else if (status == MessagePumpDispatcher::EVENT_IGNORED) {
      VLOG(1) << "Event (" << xev->type << ") not handled.";
    }
    DidProcessXEvent(xev);
  }

  if (have_cookie) {
    XFreeEventData(xev->xgeneric.display, &xev->xcookie);
  }

  return should_quit;
}

bool MessagePumpX::RunOnce(GMainContext* context, bool block) {
  Display* display = GetDefaultXDisplay();
  if (!display || !GetDispatcher())
    return g_main_context_iteration(context, block);

  // In the general case, we want to handle all pending events before running
  // the tasks. This is what happens in the message_pump_glib case.
  while (XPending(display)) {
    XEvent xev;
    XPeekEvent(display, &xev);

    if (ShouldCaptureXEvent(&xev)) {
      XNextEvent(display, &xev);
      if (ProcessXEvent(&xev))
        return true;
#if defined(TOOLKIT_USES_GTK)
    } else if (gdksource_) {
      // TODO(sad): A couple of extra events can still sneak in during this.
      // Those should be sent back to the X queue from the dispatcher
      // EventDispatcherX.
      gdksource_->source_funcs->dispatch = gdkdispatcher_;
      g_main_context_iteration(context, FALSE);
#endif
    }
#if defined(TOOLKIT_USES_GTK)
    // In the GTK case, we only want to process one event at a time.
    break;
#endif
  }

  bool retvalue;
#if defined(TOOLKIT_USES_GTK)
  if (gdksource_) {
    // Replace the dispatch callback of the GDK event source temporarily so that
    // it doesn't read events from X.
    gboolean (*cb)(GSource*, GSourceFunc, void*) =
        gdksource_->source_funcs->dispatch;
    gdksource_->source_funcs->dispatch = PlaceholderDispatch;

    dispatching_event_ = true;
    retvalue = g_main_context_iteration(context, block);
    dispatching_event_ = false;

    gdksource_->source_funcs->dispatch = cb;
  } else {
    retvalue = g_main_context_iteration(context, block);
  }
#else
  retvalue = g_main_context_iteration(context, block);
#endif

  return retvalue;
}

bool MessagePumpX::WillProcessXEvent(XEvent* xevent) {
  ObserverListBase<MessagePumpObserver>::Iterator it(observers());
  MessagePumpObserver* obs;
  while ((obs = it.GetNext()) != NULL) {
    if (obs->WillProcessEvent(xevent))
      return true;
  }
  return false;
}

void MessagePumpX::DidProcessXEvent(XEvent* xevent) {
  ObserverListBase<MessagePumpObserver>::Iterator it(observers());
  MessagePumpObserver* obs;
  while ((obs = it.GetNext()) != NULL) {
    obs->DidProcessEvent(xevent);
  }
}

#if defined(TOOLKIT_USES_GTK)
GdkFilterReturn MessagePumpX::GdkEventFilter(GdkXEvent* gxevent,
                                             GdkEvent* gevent,
                                             gpointer data) {
  MessagePumpX* pump = static_cast<MessagePumpX*>(data);
  XEvent* xev = static_cast<XEvent*>(gxevent);

  if (pump->ShouldCaptureXEvent(xev) && pump->GetDispatcher()) {
    pump->ProcessXEvent(xev);
    return GDK_FILTER_REMOVE;
  }
  return GDK_FILTER_CONTINUE;
}

void MessagePumpX::EventDispatcherX(GdkEvent* event, gpointer data) {
  MessagePumpX* pump_x = reinterpret_cast<MessagePumpX*>(data);
  if (!pump_x->gdksource_) {
    pump_x->gdksource_ = g_main_current_source();
    if (pump_x->gdksource_)
      pump_x->gdkdispatcher_ = pump_x->gdksource_->source_funcs->dispatch;
  } else if (!pump_x->IsDispatchingEvent()) {
    if (event->type != GDK_NOTHING &&
        pump_x->capture_gdk_events_[event->type]) {
      NOTREACHED() << "GDK received an event it shouldn't have:" << event->type;
    }
  }

  gtk_main_do_event(event);
}

void MessagePumpX::InitializeEventsToCapture(void) {
  // TODO(sad): Decide which events we want to capture and update the tables
  // accordingly.
  capture_x_events_[KeyPress] = true;
  capture_gdk_events_[GDK_KEY_PRESS] = true;

  capture_x_events_[KeyRelease] = true;
  capture_gdk_events_[GDK_KEY_RELEASE] = true;

  capture_x_events_[ButtonPress] = true;
  capture_gdk_events_[GDK_BUTTON_PRESS] = true;

  capture_x_events_[ButtonRelease] = true;
  capture_gdk_events_[GDK_BUTTON_RELEASE] = true;

  capture_x_events_[MotionNotify] = true;
  capture_gdk_events_[GDK_MOTION_NOTIFY] = true;

  capture_x_events_[GenericEvent] = true;
}

COMPILE_ASSERT(XLASTEvent >= LASTEvent, XLASTEvent_too_small);

#endif  // defined(TOOLKIT_USES_GTK)

}  // namespace base
