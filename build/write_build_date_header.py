#!/usr/bin/env python
# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Writes a file that contains a define that approximates the build date.

For unofficial builds, the build date is set to the most recent first Sunday
of a month, in UTC time.

For official builds, the build date is set to the current date (in UTC).

It is also possible to explicitly set a build date to be used.

The reason for using the first Sunday of a month for unofficial builds is that
it is a time where invalidating the build cache shouldn't have major
reprecussions (due to lower load).
"""

import argparse
import calendar
import datetime
import os
import sys


def GetFirstSundayOfMonth(year, month):
  """Returns the first sunday of the given month of the given year."""
  weeks = calendar.Calendar().monthdays2calendar(year, month)
  # Return the first day in the first week that is a Sunday.
  return [date_day[0] for date_day in weeks[0] if date_day[1] == 6][0]


# Validate that GetFirstSundayOfMonth works.
assert GetFirstSundayOfMonth(2016, 2) == 7
assert GetFirstSundayOfMonth(2016, 3) == 6
assert GetFirstSundayOfMonth(2000, 1) == 2


def GetBuildDate(build_type, utc_now):
  """Gets the approximate build date given the specific build type."""
  day = utc_now.day
  month = utc_now.month
  year = utc_now.year
  if build_type != 'official':
    first_sunday = GetFirstSundayOfMonth(year, month)
    # If our build is after the first Sunday, we've already refreshed our build
    # cache on a quiet day, so just use that day.
    # Otherwise, take the first Sunday of the previous month.
    if day >= first_sunday:
      day = first_sunday
    else:
      month -= 1
      if month == 0:
        month = 12
        year -= 1
      day = GetFirstSundayOfMonth(year, month)
  return '{:%b %d %Y}'.format(datetime.date(year, month, day))


# Validate that GetBuildDate works.
assert GetBuildDate('default', datetime.date(2016, 2, 6)) == 'Jan 03 2016'
assert GetBuildDate('default', datetime.date(2016, 2, 7)) == 'Feb 07 2016'
assert GetBuildDate('default', datetime.date(2016, 2, 8)) == 'Feb 07 2016'


def main():
  argument_parser = argparse.ArgumentParser()
  argument_parser.add_argument('output_file', help='The file to write to')
  argument_parser.add_argument('build_type', help='The type of build',
                               choices=('official', 'default'))
  argument_parser.add_argument('build_date_override', nargs='?',
                               help='Optional override for the build date')
  args = argument_parser.parse_args()

  if args.build_date_override:
    build_date = args.build_date_override
  else:
    build_date = GetBuildDate(args.build_type, datetime.datetime.utcnow())

  output = ('// Generated by //build/write_build_date_header.py\n'
           '#ifndef BUILD_DATE\n'
           '#define BUILD_DATE "{}"\n'
           '#endif // BUILD_DATE\n'.format(build_date))

  current_contents = ''
  if os.path.isfile(args.output_file):
    with open(args.output_file, 'r') as current_file:
      current_contents = current_file.read()

  if current_contents != output:
    with open(args.output_file, 'w') as output_file:
      output_file.write(output)
  return 0


if __name__ == '__main__':
  sys.exit(main())
