// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/html_viewer/blink_input_events_type_converters.h"

#include "base/time/time.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/event_constants.h"

namespace mojo {
namespace {

// Used for scrolling. This matches Firefox behavior.
const int kPixelsPerTick = 53;

int EventFlagsToWebEventModifiers(int flags) {
  int modifiers = 0;

  if (flags & ui::EF_SHIFT_DOWN)
    modifiers |= blink::WebInputEvent::ShiftKey;
  if (flags & ui::EF_CONTROL_DOWN)
    modifiers |= blink::WebInputEvent::ControlKey;
  if (flags & ui::EF_ALT_DOWN)
    modifiers |= blink::WebInputEvent::AltKey;
  // TODO(beng): MetaKey/META_MASK
  if (flags & ui::EF_LEFT_MOUSE_BUTTON)
    modifiers |= blink::WebInputEvent::LeftButtonDown;
  if (flags & ui::EF_MIDDLE_MOUSE_BUTTON)
    modifiers |= blink::WebInputEvent::MiddleButtonDown;
  if (flags & ui::EF_RIGHT_MOUSE_BUTTON)
    modifiers |= blink::WebInputEvent::RightButtonDown;
  if (flags & ui::EF_CAPS_LOCK_DOWN)
    modifiers |= blink::WebInputEvent::CapsLockOn;
  return modifiers;
}

int EventFlagsToWebInputEventModifiers(int flags) {
  return
      (flags & ui::EF_SHIFT_DOWN ? blink::WebInputEvent::ShiftKey : 0) |
      (flags & ui::EF_CONTROL_DOWN ? blink::WebInputEvent::ControlKey : 0) |
      (flags & ui::EF_CAPS_LOCK_DOWN ? blink::WebInputEvent::CapsLockOn : 0) |
      (flags & ui::EF_ALT_DOWN ? blink::WebInputEvent::AltKey : 0);
}

// TODO(erg): This function is extremely hacky and should only be accepted
// since this is demo code which won't live very long. Actually calculating
// this accurately would require the native X11 event so we could get accurate
// key codes via XKeyEventToWindowsKeyCode() and ui::GetCharacterFromXEvent().
// That option is closed to us, and thus, hack.
int32_t MakeHackyText(int32_t key_code, int flags) {
  if (!(flags & ui::EF_SHIFT_DOWN) && key_code >= 'A' && key_code <= 'Z')
    key_code = key_code + ('a' - 'A');

  return key_code;
}

int GetClickCount(int flags) {
  if (flags & ui::EF_IS_TRIPLE_CLICK)
    return 3;
  else if (flags & ui::EF_IS_DOUBLE_CLICK)
    return 2;

  return 1;
}

}  // namespace

// static
scoped_ptr<blink::WebInputEvent>
TypeConverter<EventPtr, scoped_ptr<blink::WebInputEvent> >::ConvertTo(
    const EventPtr& event) {

  if (event->action == ui::ET_MOUSE_PRESSED ||
      event->action == ui::ET_MOUSE_RELEASED ||
      event->action == ui::ET_MOUSE_ENTERED ||
      event->action == ui::ET_MOUSE_EXITED ||
      event->action == ui::ET_MOUSE_MOVED ||
      event->action == ui::ET_MOUSE_DRAGGED) {
    scoped_ptr<blink::WebMouseEvent> web_event(new blink::WebMouseEvent);
    web_event->x = event->location->x;
    web_event->y = event->location->y;

    web_event->modifiers = EventFlagsToWebEventModifiers(event->flags);
    web_event->timeStampSeconds =
        base::TimeDelta::FromInternalValue(event->time_stamp).InSecondsF();

    web_event->button = blink::WebMouseEvent::ButtonNone;
    if (event->flags & ui::EF_LEFT_MOUSE_BUTTON)
      web_event->button = blink::WebMouseEvent::ButtonLeft;
    if (event->flags & ui::EF_MIDDLE_MOUSE_BUTTON)
      web_event->button = blink::WebMouseEvent::ButtonMiddle;
    if (event->flags & ui::EF_RIGHT_MOUSE_BUTTON)
      web_event->button = blink::WebMouseEvent::ButtonRight;

    switch (event->action) {
      case ui::ET_MOUSE_PRESSED:
        web_event->type = blink::WebInputEvent::MouseDown;
        web_event->clickCount = GetClickCount(event->flags);
        break;
      case ui::ET_MOUSE_RELEASED:
        web_event->type = blink::WebInputEvent::MouseUp;
        web_event->clickCount = GetClickCount(event->flags);
        break;
      case ui::ET_MOUSE_ENTERED:
      case ui::ET_MOUSE_EXITED:
      case ui::ET_MOUSE_MOVED:
      case ui::ET_MOUSE_DRAGGED:
        web_event->type = blink::WebInputEvent::MouseMove;
        break;
      default:
        NOTIMPLEMENTED() << "Received unexpected event: " << event->action;
        break;
    }

    return web_event.PassAs<blink::WebInputEvent>();
  } else if ((event->action == ui::ET_KEY_PRESSED ||
              event->action == ui::ET_KEY_RELEASED) &&
             event->key_data) {
    scoped_ptr<blink::WebKeyboardEvent> web_event(new blink::WebKeyboardEvent);

    // TODO(erg): This derivation of is_char is a bad hack, along with our key
    // code and text calculations. Normally, key code calculations need the
    // gfx::NativeEvent, which we don't have access to here. Likewise, the
    // calculation of the |text| is overly simplified for similar
    // reasons. (Contrast with web_input_event_aurax11.cc in content/.)
    bool is_char =
        event->key_data->is_char ||
        (event->key_data->key_code >= 32 && event->key_data->key_code < 127);

    switch (event->action) {
      case ui::ET_KEY_PRESSED:
        web_event->type = is_char ? blink::WebInputEvent::Char :
            blink::WebInputEvent::RawKeyDown;
        break;
      case ui::ET_KEY_RELEASED:
        web_event->type = blink::WebInputEvent::KeyUp;
        break;
      default:
        NOTREACHED();
    }

    web_event->modifiers = EventFlagsToWebInputEventModifiers(event->flags);
    web_event->timeStampSeconds =
        base::TimeDelta::FromInternalValue(event->time_stamp).InSecondsF();

    if (web_event->modifiers & blink::WebInputEvent::AltKey)
      web_event->isSystemKey = true;

    web_event->windowsKeyCode = event->key_data->key_code;
    web_event->nativeKeyCode = event->key_data->key_code;
    web_event->text[0] = MakeHackyText(event->key_data->key_code, event->flags);
    web_event->unmodifiedText[0] = event->key_data->key_code;

    web_event->setKeyIdentifierFromWindowsKeyCode();
    return web_event.PassAs<blink::WebInputEvent>();
  } else if (event->action == ui::ET_MOUSEWHEEL) {
    scoped_ptr<blink::WebMouseWheelEvent> web_event(
        new blink::WebMouseWheelEvent);
    web_event->type = blink::WebInputEvent::MouseWheel;
    web_event->button = blink::WebMouseEvent::ButtonNone;
    web_event->modifiers = EventFlagsToWebEventModifiers(event->flags);
    web_event->timeStampSeconds =
        base::TimeDelta::FromInternalValue(event->time_stamp).InSecondsF();

    web_event->x = event->location->x;
    web_event->y = event->location->y;

    if ((event->flags & ui::EF_SHIFT_DOWN) != 0 &&
        event->wheel_data->x_offset == 0) {
      web_event->deltaX = event->wheel_data->y_offset;
      web_event->deltaY = 0;
    } else {
      web_event->deltaX = event->wheel_data->x_offset;
      web_event->deltaY = event->wheel_data->y_offset;
    }

    web_event->wheelTicksX = web_event->deltaX / kPixelsPerTick;
    web_event->wheelTicksY = web_event->deltaY / kPixelsPerTick;

    return web_event.PassAs<blink::WebInputEvent>();
  }

  return scoped_ptr<blink::WebInputEvent>();
}

}  // namespace mojo
