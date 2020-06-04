// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SYSTEM_MONITOR_SYSTEM_MONITOR_H_
#define BASE_SYSTEM_MONITOR_SYSTEM_MONITOR_H_
#pragma once

#include <map>
#include <string>
#include <utility>

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"

// Windows HiRes timers drain the battery faster so we need to know the battery
// status.  This isn't true for other platforms.
#if defined(OS_WIN)
#define ENABLE_BATTERY_MONITORING 1
#else
#undef ENABLE_BATTERY_MONITORING
#endif  // !OS_WIN

#include "base/observer_list_threadsafe.h"
#if defined(ENABLE_BATTERY_MONITORING)
#include "base/timer.h"
#endif  // defined(ENABLE_BATTERY_MONITORING)

#if defined(OS_MACOSX)
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>
#endif  // OS_MACOSX

class FilePath;

namespace base {

// Class for monitoring various system-related subsystems
// such as power management, network status, etc.
// TODO(mbelshe):  Add support beyond just power management.
class BASE_EXPORT SystemMonitor {
 public:
  // Normalized list of power events.
  enum PowerEvent {
    POWER_STATE_EVENT,  // The Power status of the system has changed.
    SUSPEND_EVENT,      // The system is being suspended.
    RESUME_EVENT        // The system is being resumed.
  };

  enum PowerRequirement {
    DISPLAY_REQUIRED,   // The display should not be shut down.
    SYSTEM_REQUIRED,    // The system should not be suspended.
    CPU_REQUIRED,       // The process should not be suspended.
    TEST_REQUIRED       // This is used by unit tests.
  };

  typedef unsigned int DeviceIdType;

  // Create SystemMonitor. Only one SystemMonitor instance per application
  // is allowed.
  SystemMonitor();
  ~SystemMonitor();

  // Get the application-wide SystemMonitor (if not present, returns NULL).
  static SystemMonitor* Get();

#if defined(OS_MACOSX)
  // Allocate system resources needed by the SystemMonitor class.
  //
  // This function must be called before instantiating an instance of the class
  // and before the Sandbox is initialized.
  static void AllocateSystemIOPorts();
#endif

  //
  // Power-related APIs
  //

  // Is the computer currently on battery power.
  // Can be called on any thread.
  bool BatteryPower() const {
    // Using a lock here is not necessary for just a bool.
    return battery_in_use_;
  }

  // Callbacks will be called on the thread which creates the SystemMonitor.
  // During the callback, Add/RemoveObserver will block until the callbacks
  // are finished. Observers should implement quick callback functions; if
  // lengthy operations are needed, the observer should take care to invoke
  // the operation on an appropriate thread.
  class BASE_EXPORT PowerObserver {
   public:
    // Notification of a change in power status of the computer, such
    // as from switching between battery and A/C power.
    virtual void OnPowerStateChange(bool on_battery_power) {}

    // Notification that the system is suspending.
    virtual void OnSuspend() {}

    // Notification that the system is resuming.
    virtual void OnResume() {}

   protected:
    virtual ~PowerObserver() {}
  };

  class BASE_EXPORT DevicesChangedObserver {
   public:
    // Notification that the devices connected to the system have changed.
    // This is only implemented on Windows currently.
    virtual void OnDevicesChanged() {}

    // When a media device is attached or detached, one of these two events
    // is triggered.
    // TODO(vandebo) Pass an appropriate device identifier or way to interact
    // with the devices instead of FilePath.
    virtual void OnMediaDeviceAttached(const DeviceIdType& id,
                                       const std::string& name,
                                       const FilePath& path) {}

    virtual void OnMediaDeviceDetached(const DeviceIdType& id) {}

   protected:
    virtual ~DevicesChangedObserver() {}
  };

  // Add a new observer.
  // Can be called from any thread.
  // Must not be called from within a notification callback.
  void AddPowerObserver(PowerObserver* obs);
  void AddDevicesChangedObserver(DevicesChangedObserver* obs);

  // Remove an existing observer.
  // Can be called from any thread.
  // Must not be called from within a notification callback.
  void RemovePowerObserver(PowerObserver* obs);
  void RemoveDevicesChangedObserver(DevicesChangedObserver* obs);

#if defined(OS_WIN)
  // Windows-specific handling of a WM_POWERBROADCAST message.
  // Embedders of this API should hook their top-level window
  // message loop and forward WM_POWERBROADCAST through this call.
  void ProcessWmPowerBroadcastMessage(int event_id);
#endif

  // Cross-platform handling of a power event.
  void ProcessPowerMessage(PowerEvent event_id);

  // Cross-platform handling of a device change event.
  void ProcessDevicesChanged();
  void ProcessMediaDeviceAttached(const DeviceIdType& id,
                                  const std::string& name,
                                  const FilePath& path);
  void ProcessMediaDeviceDetached(const DeviceIdType& id);

  // Enters or leaves a period of time with a given |requirement|. If the
  // operation has multiple requirements, make sure to use a unique |reason| for
  // each one. Reusing the same |reason| is OK as far as the |requirement| is
  // the same in every call, and each BeginPowerRequirement call is paired with
  // a call to EndPowerRequirement. |reason| is expected to be an ASCII string.
  // Must be called from the thread that created the SystemMonitor.
  // Warning: Please remember that sleep deprivation is not a good thing; use
  // with caution.
  void BeginPowerRequirement(PowerRequirement requirement,
                             const std::string& reason);
  void EndPowerRequirement(PowerRequirement requirement,
                           const std::string& reason);

  // Returns the number of outsanding power requirement requests.
  size_t GetPowerRequirementsCountForTest() const;

 private:
#if defined(OS_MACOSX)
  void PlatformInit();
  void PlatformDestroy();
#endif

  // Platform-specific method to check whether the system is currently
  // running on battery power.  Returns true if running on batteries,
  // false otherwise.
  bool IsBatteryPower();

  // Checks the battery status and notifies observers if the battery
  // status has changed.
  void BatteryCheck();

  // Functions to trigger notifications.
  void NotifyDevicesChanged();
  void NotifyMediaDeviceAttached(const DeviceIdType& id,
                                 const std::string& name,
                                 const FilePath& path);
  void NotifyMediaDeviceDetached(const DeviceIdType& id);
  void NotifyPowerStateChange();
  void NotifySuspend();
  void NotifyResume();

  scoped_refptr<ObserverListThreadSafe<PowerObserver> > power_observer_list_;
  scoped_refptr<ObserverListThreadSafe<DevicesChangedObserver> >
      devices_changed_observer_list_;
  bool battery_in_use_;
  bool suspended_;

#if defined(OS_WIN)
  std::map<std::string, std::pair<HANDLE, int> > handles_;
  base::ThreadChecker thread_checker_;
#endif

#if defined(ENABLE_BATTERY_MONITORING)
  base::OneShotTimer<SystemMonitor> delayed_battery_check_;
#endif

  DISALLOW_COPY_AND_ASSIGN(SystemMonitor);
};

}  // namespace base

#endif  // BASE_SYSTEM_MONITOR_SYSTEM_MONITOR_H_
