#!/usr/bin/python2.5
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Snapshot Build Bisect Tool

This script bisects a snapshot archive using binary search. It starts at
a bad revision (it will try to guess HEAD) and asks for a last known-good
revision. It will then binary search across this revision range by downloading,
unzipping, and opening Chromium for you. After testing the specific revision,
it will ask you whether it is good or bad before continuing the search.
"""

# Base URL to download snapshots from.
BUILD_BASE_URL = "http://build.chromium.org/buildbot/snapshots/"

# The type (platform) of the build archive. This is what's passed in to the
# '-a/--archive' option.
BUILD_ARCHIVE_TYPE = ''

# The selected archive to bisect.
BUILD_ARCHIVE_DIR = ''

# The location of the builds.
BUILD_ARCHIVE_URL = "/%d/"

# Name of the build archive.
BUILD_ZIP_NAME = ''

# Directory name inside the archive.
BUILD_DIR_NAME = ''

# Name of the executable.
BUILD_EXE_NAME = ''

# URL to the ViewVC commit page.
BUILD_VIEWVC_URL = "http://src.chromium.org/viewvc/chrome?view=rev&revision=%d"

###############################################################################

import math
import optparse
import os
import re
import shutil
import sys
import tempfile
import urllib

def SetArchiveVars(archive):
  """Set a bunch of global variables appropriate for the specified archive."""
  global BUILD_ARCHIVE_TYPE
  global BUILD_ARCHIVE_DIR
  global BUILD_ZIP_NAME
  global BUILD_DIR_NAME
  global BUILD_EXE_NAME
  global BUILD_BASE_URL

  BUILD_ARCHIVE_TYPE = archive
  BUILD_ARCHIVE_DIR = 'chromium-rel-' + BUILD_ARCHIVE_TYPE

  if BUILD_ARCHIVE_TYPE in ('linux', 'linux-64'):
    BUILD_ZIP_NAME = 'chrome-linux.zip'
    BUILD_DIR_NAME = 'chrome-linux'
    BUILD_EXE_NAME = "chrome"
  elif BUILD_ARCHIVE_TYPE in ('mac'):
    BUILD_ZIP_NAME = 'chrome-mac.zip'
    BUILD_DIR_NAME = 'chrome-mac'
    BUILD_EXE_NAME = "Chromium.app"
  elif BUILD_ARCHIVE_TYPE in ('xp'):
    BUILD_ZIP_NAME = 'chrome-win32.zip'
    BUILD_DIR_NAME = 'chrome-win32'
    BUILD_EXE_NAME = "chrome.exe"

  BUILD_BASE_URL += BUILD_ARCHIVE_DIR

def ParseDirectoryIndex(url):
  """Parses the HTML directory listing into a list of revision numbers."""
  handle = urllib.urlopen(url)
  dirindex = handle.read()
  handle.close()
  return re.findall(r'<a href="([0-9]*)/">\1/</a>', dirindex)

def GetRevList(good, bad):
  """Gets the list of revision numbers between |good| and |bad|."""
  # Download the main revlist.
  revlist = ParseDirectoryIndex(BUILD_BASE_URL)
  revlist = map(int, revlist)
  revlist = filter(lambda r: range(good, bad).__contains__(int(r)), revlist)
  revlist.sort()
  return revlist

def TryRevision(rev):
  """Downloads revision |rev|, unzips it, and opens it for the user to test."""
  # Do this in a temp dir so we don't collide with user files.
  cwd = os.getcwd()
  tempdir = tempfile.mkdtemp(prefix='bisect_tmp')
  os.chdir(tempdir)

  # Download the file.
  download_url = BUILD_BASE_URL + (BUILD_ARCHIVE_URL % rev) + BUILD_ZIP_NAME
  try:
    print 'Fetching ' + download_url
    urllib.urlretrieve(download_url, BUILD_ZIP_NAME)
  except Exception, e:
    print("Could not retrieve the download. Sorry.")
    sys.exit(-1)

  # Unzip the file.
  print 'Unzipping ...'
  os.system("unzip -q %s" % BUILD_ZIP_NAME)

  # Tell the system to open the app.
  print 'Running %s/%s/%s' % (os.getcwd(), BUILD_DIR_NAME, BUILD_EXE_NAME)
  if BUILD_ARCHIVE_TYPE in ('linux', 'linux-64'):
    os.system("%s/%s" % (BUILD_DIR_NAME, BUILD_EXE_NAME))
  elif BUILD_ARCHIVE_TYPE in ('mac'):
    os.system("open %s/%s" % (BUILD_DIR_NAME, BUILD_EXE_NAME))
  elif BUILD_ARCHIVE_TYPE in ('xp'):
    # TODO(mmoss) Does Windows need 'start' or something?
    os.system("%s/%s" % (BUILD_DIR_NAME, BUILD_EXE_NAME))

  os.chdir(cwd)
  print 'Cleaning temp dir ...'
  try:
    shutil.rmtree(tempdir, True)
  except Exception, e:
    pass

def AskIsGoodBuild(rev):
  """Annoyingly ask the user whether build |rev| is good or bad."""
  while True:
    check = raw_input("\nBuild %d is [(g)ood/(b)ad]: " % int(rev))[0]
    if (check == "g" or check  == "b"):
      return (check == "g")
    else:
      print("Just answer the question...")

def main():
  usage = ('%prog [options]\n'
           'Perform binary search on the snapshot builds.')
  parser = optparse.OptionParser(usage=usage)
  parser.add_option('-a', '--archive',
                    choices = ['mac', 'xp', 'linux', 'linux-64'],
                    help = 'The buildbot archive to bisect.')
  parser.add_option('-b', '--bad', type = 'int',
                    help = 'The bad revision to bisect to.')
  parser.add_option('-g', '--good', type = 'int',
                    help = 'The last known good revision to bisect from.')
  (opts, args) = parser.parse_args()

  if opts.archive is None:
    parser.print_help()
    return 1

  if opts.bad and opts.good and (opts.good > opts.bad):
    print ('The good revision (%d) must precede the bad revision (%d).\n' %
           (opts.good, opts.bad))
    parser.print_help()
    return 1

  SetArchiveVars(opts.archive)

  # Pick a starting point, try to get HEAD for this.
  if opts.bad:
    bad_rev = opts.bad
  else:
    bad_rev = 0
    try:
      # Location of the latest build revision number
      BUILD_LATEST_URL = "%s/LATEST" % (BUILD_BASE_URL)
      nh = urllib.urlopen(BUILD_LATEST_URL)
      latest = int(nh.read())
      nh.close()
      bad_rev = raw_input("Bad revision [HEAD:%d]: " % latest)
      if (bad_rev == ""):
        bad_rev = latest
      bad_rev = int(bad_rev)
    except Exception, e:
      print("Could not determine latest revision. This could be bad...")
      bad_rev = int(raw_input("Bad revision: "))

  # Find out when we were good.
  if opts.good:
    good_rev = opts.good
  else:
    good_rev = 0
    try:
      good_rev = int(raw_input("Last known good [0]: "))
    except Exception, e:
      pass

  # Get a list of revisions to bisect across.
  revlist = GetRevList(good_rev, bad_rev)

  # If we don't have a |good_rev|, set it to be the first revision possible.
  if good_rev == 0:
    good_rev = revlist[0]

  # These are indexes of |revlist|.
  good = 0
  bad = len(revlist) - 1

  # Binary search time!
  while good < bad:
    candidates = revlist[good:bad]
    num_poss = len(candidates)
    if num_poss > 10:
      print("%d candidates. %d tries left." %
          (num_poss, round(math.log(num_poss, 2))))
    else:
      print("Candidates: %s" % revlist[good:bad])

    # Cut the problem in half...
    test = int((bad - good) / 2) + good
    test_rev = revlist[test]

    # Let the user give this revision a spin.
    TryRevision(test_rev)
    if AskIsGoodBuild(test_rev):
      good = test + 1
    else:
      bad = test

  # We're done. Let the user know the results in an official manner.
  print("You are probably looking for build %d." % revlist[bad])
  print("This is the ViewVC URL for the potential bustage:")
  print(BUILD_VIEWVC_URL % revlist[bad])

if __name__ == '__main__':
  sys.exit(main())
