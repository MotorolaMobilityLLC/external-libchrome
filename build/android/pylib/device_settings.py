# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from pylib import content_settings


def ConfigureContentSettingsDict(device, desired_settings):
  """Configures device content setings from a dictionary.

  Many settings are documented at:
    http://developer.android.com/reference/android/provider/Settings.Global.html
    http://developer.android.com/reference/android/provider/Settings.Secure.html
    http://developer.android.com/reference/android/provider/Settings.System.html

  Many others are undocumented.

  Args:
    device: A DeviceUtils instance for the device to configure.
    desired_settings: A dict of {table: {key: value}} for all
        settings to configure.
  """
  try:
    sdk_version = int(device.old_interface.system_properties[
        'ro.build.version.sdk'])
  except ValueError:
    logging.error('Skipping content settings configuration, unknown sdk %s',
                  device.old_interface.system_properties[
                      'ro.build.version.sdk'])
    return

  if sdk_version < 16:
    logging.error('Skipping content settings configuration due to outdated sdk')
    return

  for table, key_value in sorted(desired_settings.iteritems()):
    settings = content_settings.ContentSettings(table, device)
    for key, value in key_value.iteritems():
      settings[key] = value
    logging.info('\n%s %s', table, (80 - len(table)) * '-')
    for key, value in sorted(settings.iteritems()):
      logging.info('\t%s: %s', key, value)


DETERMINISTIC_DEVICE_SETTINGS = {
  'com.google.settings/partner': {
    'network_location_opt_in': 0,
    'use_location_for_services': 1,
  },
  'settings/global': {
    'assisted_gps_enabled': 0,

    # Disable "auto time" and "auto time zone" to avoid network-provided time
    # to overwrite the device's datetime and timezone synchronized from host
    # when running tests later. See b/6569849.
    'auto_time': 0,
    'auto_time_zone': 0,

    'development_settings_enabled': 1,

    # Flag for allowing ActivityManagerService to send ACTION_APP_ERROR intents
    # on application crashes and ANRs. If this is disabled, the crash/ANR dialog
    # will never display the "Report" button.
    # Type: int ( 0 = disallow, 1 = allow )
    'send_action_app_error': 0,

    'stay_on_while_plugged_in': 3,

    'verifier_verify_adb_installs' : 0,
  },
  'settings/secure': {
    'allowed_geolocation_origins':
        'http://www.google.co.uk http://www.google.com',

    # Ensure that we never get random dialogs like "Unfortunately the process
    # android.process.acore has stopped", which steal the focus, and make our
    # automation fail (because the dialog steals the focus then mistakenly
    # receives the injected user input events).
    'anr_show_background': 0,

    # Ensure Geolocation is enabled and allowed for tests.
    'location_providers_allowed': 'gps,network',

    'lockscreen.disabled': 1,

    'screensaver_enabled': 0,
  },
  'settings/system': {
    # Don't want devices to accidentally rotate the screen as that could
    # affect performance measurements.
    'accelerometer_rotation': 0,

    'lockscreen.disabled': 1,

    # Turn down brightness and disable auto-adjust so that devices run cooler.
    'screen_brightness': 5,
    'screen_brightness_mode': 0,

    'user_rotation': 0,
  },
}


NETWORK_DISABLED_SETTINGS = {
  'settings/global': {
    'airplane_mode_on': 1,
    'wifi_on': 0,
  },
}
