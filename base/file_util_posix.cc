// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <fstream>

#include "base/basictypes.h"
#include "base/eintr_wrapper.h"
#include "base/file_path.h"
#include "base/lock.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/time.h"
#include "unicode/coll.h"

namespace {

class LocaleAwareComparator {
 public:
  LocaleAwareComparator() {
    UErrorCode error_code = U_ZERO_ERROR;
    // Use the default collator. The default locale should have been properly
    // set by the time this constructor is called.
    collator_.reset(icu::Collator::createInstance(error_code));
    DCHECK(U_SUCCESS(error_code));
    // Make it case-sensitive.
    collator_->setStrength(icu::Collator::TERTIARY);
    // Note: We do not set UCOL_NORMALIZATION_MODE attribute. In other words, we
    // do not pay performance penalty to guarantee sort order correctness for
    // non-FCD (http://unicode.org/notes/tn5/#FCD) file names. This should be a
    // reasonable tradeoff because such file names should be rare and the sort
    // order doesn't change much anyway.
  }

  // Note: A similar function is available in l10n_util.
  // We cannot use it because base should not depend on l10n_util.
  // TODO(yuzo): Move some of l10n_util to base.
  int Compare(const string16& a, const string16& b) {
    // We are not sure if Collator::compare is thread-safe.
    // Use an AutoLock just in case.
    AutoLock auto_lock(lock_);

    UErrorCode error_code = U_ZERO_ERROR;
    UCollationResult result = collator_->compare(
        static_cast<const UChar*>(a.c_str()),
        static_cast<int>(a.length()),
        static_cast<const UChar*>(b.c_str()),
        static_cast<int>(b.length()),
        error_code);
    DCHECK(U_SUCCESS(error_code));
    return result;
  }

 private:
  scoped_ptr<icu::Collator> collator_;
  Lock lock_;
  friend struct DefaultSingletonTraits<LocaleAwareComparator>;

  DISALLOW_COPY_AND_ASSIGN(LocaleAwareComparator);
};

}  // namespace

namespace file_util {

#if defined(GOOGLE_CHROME_BUILD)
static const char* kTempFileName = "com.google.chrome.XXXXXX";
#else
static const char* kTempFileName = "org.chromium.XXXXXX";
#endif

std::wstring GetDirectoryFromPath(const std::wstring& path) {
  if (EndsWithSeparator(path)) {
    std::wstring dir = path;
    TrimTrailingSeparator(&dir);
    return dir;
  } else {
    char full_path[PATH_MAX];
    base::strlcpy(full_path, WideToUTF8(path).c_str(), arraysize(full_path));
    return UTF8ToWide(dirname(full_path));
  }
}

bool AbsolutePath(FilePath* path) {
  char full_path[PATH_MAX];
  if (realpath(path->value().c_str(), full_path) == NULL)
    return false;
  *path = FilePath(full_path);
  return true;
}

int CountFilesCreatedAfter(const FilePath& path,
                           const base::Time& comparison_time) {
  int file_count = 0;

  DIR* dir = opendir(path.value().c_str());
  if (dir) {
#if !defined(OS_LINUX) && !defined(OS_MACOSX)
  #error Depending on the definition of struct dirent, additional space for \
      pathname may be needed
#endif
    struct dirent ent_buf;
    struct dirent* ent;
    while (readdir_r(dir, &ent_buf, &ent) == 0 && ent) {
      if ((strcmp(ent->d_name, ".") == 0) ||
          (strcmp(ent->d_name, "..") == 0))
        continue;

      struct stat64 st;
      int test = stat64(path.Append(ent->d_name).value().c_str(), &st);
      if (test != 0) {
        LOG(ERROR) << "stat64 failed: " << strerror(errno);
        continue;
      }
      // Here, we use Time::TimeT(), which discards microseconds. This
      // means that files which are newer than |comparison_time| may
      // be considered older. If we don't discard microseconds, it
      // introduces another issue. Suppose the following case:
      //
      // 1. Get |comparison_time| by Time::Now() and the value is 10.1 (secs).
      // 2. Create a file and the current time is 10.3 (secs).
      //
      // As POSIX doesn't have microsecond precision for |st_ctime|,
      // the creation time of the file created in the step 2 is 10 and
      // the file is considered older than |comparison_time|. After
      // all, we may have to accept either of the two issues: 1. files
      // which are older than |comparison_time| are considered newer
      // (current implementation) 2. files newer than
      // |comparison_time| are considered older.
      if (st.st_ctime >= comparison_time.ToTimeT())
        ++file_count;
    }
    closedir(dir);
  }
  return file_count;
}

// TODO(erikkay): The Windows version of this accepts paths like "foo/bar/*"
// which works both with and without the recursive flag.  I'm not sure we need
// that functionality. If not, remove from file_util_win.cc, otherwise add it
// here.
bool Delete(const FilePath& path, bool recursive) {
  const char* path_str = path.value().c_str();
  struct stat64 file_info;
  int test = stat64(path_str, &file_info);
  if (test != 0) {
    // The Windows version defines this condition as success.
    bool ret = (errno == ENOENT || errno == ENOTDIR);
    return ret;
  }
  if (!S_ISDIR(file_info.st_mode))
    return (unlink(path_str) == 0);
  if (!recursive)
    return (rmdir(path_str) == 0);

  bool success = true;
  std::stack<std::string> directories;
  directories.push(path.value());
  FileEnumerator traversal(path, true, static_cast<FileEnumerator::FILE_TYPE>(
        FileEnumerator::FILES | FileEnumerator::DIRECTORIES |
        FileEnumerator::SHOW_SYM_LINKS));
  for (FilePath current = traversal.Next(); success && !current.empty();
       current = traversal.Next()) {
    FileEnumerator::FindInfo info;
    traversal.GetFindInfo(&info);

    if (S_ISDIR(info.stat.st_mode))
      directories.push(current.value());
    else
      success = (unlink(current.value().c_str()) == 0);
  }

  while (success && !directories.empty()) {
    FilePath dir = FilePath(directories.top());
    directories.pop();
    success = (rmdir(dir.value().c_str()) == 0);
  }

  return success;
}

bool Move(const FilePath& from_path, const FilePath& to_path) {
  if (rename(from_path.value().c_str(), to_path.value().c_str()) == 0)
    return true;

  if (!CopyDirectory(from_path, to_path, true))
    return false;

  Delete(from_path, true);
  return true;
}

bool ReplaceFile(const FilePath& from_path, const FilePath& to_path) {
  return (rename(from_path.value().c_str(), to_path.value().c_str()) == 0);
}

bool CopyDirectory(const FilePath& from_path,
                   const FilePath& to_path,
                   bool recursive) {
  // Some old callers of CopyDirectory want it to support wildcards.
  // After some discussion, we decided to fix those callers.
  // Break loudly here if anyone tries to do this.
  // TODO(evanm): remove this once we're sure it's ok.
  DCHECK(to_path.value().find('*') == std::string::npos);
  DCHECK(from_path.value().find('*') == std::string::npos);

  char top_dir[PATH_MAX];
  if (base::strlcpy(top_dir, from_path.value().c_str(),
                    arraysize(top_dir)) >= arraysize(top_dir)) {
    return false;
  }

  // This function does not properly handle destinations within the source
  FilePath real_to_path = to_path;
  if (PathExists(real_to_path)) {
    if (!AbsolutePath(&real_to_path))
      return false;
  } else {
    real_to_path = real_to_path.DirName();
    if (!AbsolutePath(&real_to_path))
      return false;
  }
  FilePath real_from_path = from_path;
  if (!AbsolutePath(&real_from_path))
    return false;
  if (real_to_path.value().size() >= real_from_path.value().size() &&
      real_to_path.value().compare(0, real_from_path.value().size(),
      real_from_path.value()) == 0)
    return false;

  bool success = true;
  FileEnumerator::FILE_TYPE traverse_type =
      static_cast<FileEnumerator::FILE_TYPE>(FileEnumerator::FILES |
      FileEnumerator::SHOW_SYM_LINKS);
  if (recursive)
    traverse_type = static_cast<FileEnumerator::FILE_TYPE>(
        traverse_type | FileEnumerator::DIRECTORIES);
  FileEnumerator traversal(from_path, recursive, traverse_type);

  // to_path may not exist yet, start the loop with to_path
  FileEnumerator::FindInfo info;
  FilePath current = from_path;
  if (stat(from_path.value().c_str(), &info.stat) < 0) {
    LOG(ERROR) << "CopyDirectory() couldn't stat source directory: " <<
        from_path.value() << " errno = " << errno;
    success = false;
  }

  while (success && !current.empty()) {
    // current is the source path, including from_path, so paste
    // the suffix after from_path onto to_path to create the target_path.
    std::string suffix(&current.value().c_str()[from_path.value().size()]);
    // Strip the leading '/' (if any).
    if (!suffix.empty()) {
      DCHECK_EQ('/', suffix[0]);
      suffix.erase(0, 1);
    }
    const FilePath target_path = to_path.Append(suffix);

    if (S_ISDIR(info.stat.st_mode)) {
      if (mkdir(target_path.value().c_str(), info.stat.st_mode & 01777) != 0 &&
          errno != EEXIST) {
        LOG(ERROR) << "CopyDirectory() couldn't create directory: " <<
            target_path.value() << " errno = " << errno;
        success = false;
      }
    } else if (S_ISREG(info.stat.st_mode)) {
      if (!CopyFile(current, target_path)) {
        LOG(ERROR) << "CopyDirectory() couldn't create file: " <<
            target_path.value();
        success = false;
      }
    } else {
      LOG(WARNING) << "CopyDirectory() skipping non-regular file: " <<
          current.value();
    }

    current = traversal.Next();
    traversal.GetFindInfo(&info);
  }

  return success;
}

bool PathExists(const FilePath& path) {
  struct stat64 file_info;
  return (stat64(path.value().c_str(), &file_info) == 0);
}

bool PathIsWritable(const FilePath& path) {
  FilePath test_path(path);
  struct stat64 file_info;
  if (stat64(test_path.value().c_str(), &file_info) != 0) {
    // If the path doesn't exist, test the parent dir.
    test_path = test_path.DirName();
    // If the parent dir doesn't exist, then return false (the path is not
    // directly writable).
    if (stat64(test_path.value().c_str(), &file_info) != 0)
      return false;
  }
  if (S_IWOTH & file_info.st_mode)
    return true;
  if (getegid() == file_info.st_gid && (S_IWGRP & file_info.st_mode))
    return true;
  if (geteuid() == file_info.st_uid && (S_IWUSR & file_info.st_mode))
    return true;
  return false;
}

bool DirectoryExists(const FilePath& path) {
  struct stat64 file_info;
  if (stat64(path.value().c_str(), &file_info) == 0)
    return S_ISDIR(file_info.st_mode);
  return false;
}

// TODO(erikkay): implement
#if 0
bool GetFileCreationLocalTimeFromHandle(int fd,
                                        LPSYSTEMTIME creation_time) {
  if (!file_handle)
    return false;

  FILETIME utc_filetime;
  if (!GetFileTime(file_handle, &utc_filetime, NULL, NULL))
    return false;

  FILETIME local_filetime;
  if (!FileTimeToLocalFileTime(&utc_filetime, &local_filetime))
    return false;

  return !!FileTimeToSystemTime(&local_filetime, creation_time);
}

bool GetFileCreationLocalTime(const std::string& filename,
                              LPSYSTEMTIME creation_time) {
  ScopedHandle file_handle(
      CreateFile(filename.c_str(), GENERIC_READ,
                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
  return GetFileCreationLocalTimeFromHandle(file_handle.Get(), creation_time);
}
#endif

bool ReadFromFD(int fd, char* buffer, size_t bytes) {
  size_t total_read = 0;
  while (total_read < bytes) {
    ssize_t bytes_read =
        HANDLE_EINTR(read(fd, buffer + total_read, bytes - total_read));
    if (bytes_read <= 0)
      break;
    total_read += bytes_read;
  }
  return total_read == bytes;
}

// Creates and opens a temporary file in |directory|, returning the
// file descriptor.  |path| is set to the temporary file path.
// Note TODO(erikkay) comment in header for BlahFileName() calls; the
// intent is to rename these files BlahFile() (since they create
// files, not filenames).  This function does NOT unlink() the file.
int CreateAndOpenFdForTemporaryFile(FilePath directory, FilePath* path) {
  *path = directory.Append(kTempFileName);
  const std::string& tmpdir_string = path->value();
  // this should be OK since mkstemp just replaces characters in place
  char* buffer = const_cast<char*>(tmpdir_string.c_str());

  return mkstemp(buffer);
}

bool CreateTemporaryFileName(FilePath* path) {
  FilePath directory;
  if (!GetTempDir(&directory))
    return false;
  int fd = CreateAndOpenFdForTemporaryFile(directory, path);
  if (fd < 0)
    return false;
  close(fd);
  return true;
}

FILE* CreateAndOpenTemporaryShmemFile(FilePath* path) {
  FilePath directory;
  if (!GetShmemTempDir(&directory))
    return false;

  return CreateAndOpenTemporaryFileInDir(directory, path);
}

FILE* CreateAndOpenTemporaryFileInDir(const FilePath& dir, FilePath* path) {
  int fd = CreateAndOpenFdForTemporaryFile(dir, path);
  if (fd < 0)
    return NULL;

  return fdopen(fd, "a+");
}

bool CreateTemporaryFileNameInDir(const std::wstring& dir,
                                  std::wstring* temp_file) {
  // Not implemented yet.
  NOTREACHED();
  return false;
}

bool CreateNewTempDirectory(const FilePath::StringType& prefix,
                            FilePath* new_temp_path) {
  FilePath tmpdir;
  if (!GetTempDir(&tmpdir))
    return false;
  tmpdir = tmpdir.Append(kTempFileName);
  std::string tmpdir_string = tmpdir.value();
  // this should be OK since mkdtemp just replaces characters in place
  char* buffer = const_cast<char*>(tmpdir_string.c_str());
  char* dtemp = mkdtemp(buffer);
  if (!dtemp)
    return false;
  *new_temp_path = FilePath(dtemp);
  return true;
}

bool CreateDirectory(const FilePath& full_path) {
  std::vector<FilePath> subpaths;

  // Collect a list of all parent directories.
  FilePath last_path = full_path;
  subpaths.push_back(full_path);
  for (FilePath path = full_path.DirName();
       path.value() != last_path.value(); path = path.DirName()) {
    subpaths.push_back(path);
    last_path = path;
  }

  // Iterate through the parents and create the missing ones.
  for (std::vector<FilePath>::reverse_iterator i = subpaths.rbegin();
       i != subpaths.rend(); ++i) {
    if (!DirectoryExists(*i)) {
      if (mkdir(i->value().c_str(), 0700) != 0)
        return false;
    }
  }
  return true;
}

bool GetFileInfo(const FilePath& file_path, FileInfo* results) {
  struct stat64 file_info;
  if (stat64(file_path.value().c_str(), &file_info) != 0)
    return false;
  results->is_directory = S_ISDIR(file_info.st_mode);
  results->size = file_info.st_size;
  results->last_modified = base::Time::FromTimeT(file_info.st_mtime);
  return true;
}

bool GetInode(const FilePath& path, ino_t* inode) {
  struct stat buffer;
  int result = stat(path.value().c_str(), &buffer);
  if (result < 0)
    return false;

  *inode = buffer.st_ino;
  return true;
}

FILE* OpenFile(const std::string& filename, const char* mode) {
  return OpenFile(FilePath(filename), mode);
}

FILE* OpenFile(const FilePath& filename, const char* mode) {
  return fopen(filename.value().c_str(), mode);
}

int ReadFile(const FilePath& filename, char* data, int size) {
  int fd = open(filename.value().c_str(), O_RDONLY);
  if (fd < 0)
    return -1;

  int ret_value = HANDLE_EINTR(read(fd, data, size));
  HANDLE_EINTR(close(fd));
  return ret_value;
}

int WriteFile(const FilePath& filename, const char* data, int size) {
  int fd = creat(filename.value().c_str(), 0666);
  if (fd < 0)
    return -1;

  // Allow for partial writes
  ssize_t bytes_written_total = 0;
  do {
    ssize_t bytes_written_partial =
      HANDLE_EINTR(write(fd, data + bytes_written_total,
                         size - bytes_written_total));
    if (bytes_written_partial < 0) {
      HANDLE_EINTR(close(fd));
      return -1;
    }
    bytes_written_total += bytes_written_partial;
  } while (bytes_written_total < size);

  HANDLE_EINTR(close(fd));
  return bytes_written_total;
}

// Gets the current working directory for the process.
bool GetCurrentDirectory(FilePath* dir) {
  char system_buffer[PATH_MAX] = "";
  if (!getcwd(system_buffer, sizeof(system_buffer))) {
    NOTREACHED();
    return false;
  }
  *dir = FilePath(system_buffer);
  return true;
}

// Sets the current working directory for the process.
bool SetCurrentDirectory(const FilePath& path) {
  int ret = chdir(path.value().c_str());
  return !ret;
}

///////////////////////////////////////////////
// FileEnumerator

FileEnumerator::FileEnumerator(const FilePath& root_path,
                               bool recursive,
                               FileEnumerator::FILE_TYPE file_type)
    : root_path_(root_path),
      recursive_(recursive),
      file_type_(file_type),
      is_in_find_op_(false),
      current_directory_entry_(0) {
  // INCLUDE_DOT_DOT must not be specified if recursive.
  DCHECK(!(recursive && (INCLUDE_DOT_DOT & file_type_)));
  pending_paths_.push(root_path);
}

FileEnumerator::FileEnumerator(const FilePath& root_path,
                               bool recursive,
                               FileEnumerator::FILE_TYPE file_type,
                               const FilePath::StringType& pattern)
    : root_path_(root_path),
      recursive_(recursive),
      file_type_(file_type),
      pattern_(root_path.Append(pattern)),
      is_in_find_op_(false),
      current_directory_entry_(0) {
  // INCLUDE_DOT_DOT must not be specified if recursive.
  DCHECK(!(recursive && (INCLUDE_DOT_DOT & file_type_)));
  // The Windows version of this code appends the pattern to the root_path,
  // potentially only matching against items in the top-most directory.
  // Do the same here.
  if (pattern.size() == 0)
    pattern_ = FilePath();
  pending_paths_.push(root_path);
}

FileEnumerator::~FileEnumerator() {
}

void FileEnumerator::GetFindInfo(FindInfo* info) {
  DCHECK(info);

  if (current_directory_entry_ >= directory_entries_.size())
    return;

  DirectoryEntryInfo* cur_entry = &directory_entries_[current_directory_entry_];
  memcpy(&(info->stat), &(cur_entry->stat), sizeof(info->stat));
  info->filename.assign(cur_entry->filename.value());
}

FilePath FileEnumerator::Next() {
  ++current_directory_entry_;

  // While we've exhausted the entries in the current directory, do the next
  while (current_directory_entry_ >= directory_entries_.size()) {
    if (pending_paths_.empty())
      return FilePath();

    root_path_ = pending_paths_.top();
    root_path_ = root_path_.StripTrailingSeparators();
    pending_paths_.pop();

    std::vector<DirectoryEntryInfo> entries;
    if (!ReadDirectory(&entries, root_path_, file_type_ & SHOW_SYM_LINKS))
      continue;

    // The API says that order is not guaranteed, but order affects UX
    std::sort(entries.begin(), entries.end(), CompareFiles);

    directory_entries_.clear();
    current_directory_entry_ = 0;
    for (std::vector<DirectoryEntryInfo>::const_iterator
        i = entries.begin(); i != entries.end(); ++i) {
      FilePath full_path = root_path_.Append(i->filename);
      if (ShouldSkip(full_path))
        continue;

      if (pattern_.value().size() &&
          fnmatch(pattern_.value().c_str(), full_path.value().c_str(),
              FNM_NOESCAPE))
        continue;

      if (recursive_ && S_ISDIR(i->stat.st_mode))
        pending_paths_.push(full_path);

      if ((S_ISDIR(i->stat.st_mode) && (file_type_ & DIRECTORIES)) ||
          (!S_ISDIR(i->stat.st_mode) && (file_type_ & FILES)))
        directory_entries_.push_back(*i);
    }
  }

  return root_path_.Append(directory_entries_[current_directory_entry_
      ].filename);
}

bool FileEnumerator::ReadDirectory(std::vector<DirectoryEntryInfo>* entries,
                                   const FilePath& source, bool show_links) {
  DIR* dir = opendir(source.value().c_str());
  if (!dir)
    return false;

#if !defined(OS_LINUX) && !defined(OS_MACOSX)
  #error Depending on the definition of struct dirent, additional space for \
      pathname may be needed
#endif
  struct dirent dent_buf;
  struct dirent* dent;
  while (readdir_r(dir, &dent_buf, &dent) == 0 && dent) {
    DirectoryEntryInfo info;
    FilePath full_name;
    int stat_value;

    info.filename = FilePath(dent->d_name);
    full_name = source.Append(dent->d_name);
    if (show_links)
      stat_value = lstat(full_name.value().c_str(), &info.stat);
    else
      stat_value = stat(full_name.value().c_str(), &info.stat);
    if (stat_value < 0) {
      LOG(ERROR) << "Couldn't stat file: " <<
          source.Append(dent->d_name).value().c_str() << " errno = " << errno;
      memset(&info.stat, 0, sizeof(info.stat));
    }
    entries->push_back(info);
  }

  closedir(dir);
  return true;
}

bool FileEnumerator::CompareFiles(const DirectoryEntryInfo& a,
                                  const DirectoryEntryInfo& b) {
  // Order lexicographically with directories before other files.
  if (S_ISDIR(a.stat.st_mode) != S_ISDIR(b.stat.st_mode))
    return S_ISDIR(a.stat.st_mode);

  // On linux, the file system encoding is not defined. We assume
  // SysNativeMBToWide takes care of it.
  //
  // ICU's collator can take strings in OS native encoding. But we convert the
  // strings to UTF-16 ourselves to ensure conversion consistency.
  // TODO(yuzo): Perhaps we should define SysNativeMBToUTF16?
  return Singleton<LocaleAwareComparator>()->Compare(
      WideToUTF16(base::SysNativeMBToWide(a.filename.value().c_str())),
      WideToUTF16(base::SysNativeMBToWide(b.filename.value().c_str()))) < 0;
}

///////////////////////////////////////////////
// MemoryMappedFile

MemoryMappedFile::MemoryMappedFile()
    : file_(-1),
      data_(NULL),
      length_(0) {
}

bool MemoryMappedFile::MapFileToMemory(const FilePath& file_name) {
  file_ = open(file_name.value().c_str(), O_RDONLY);

  if (file_ == -1) {
    LOG(ERROR) << "Couldn't open " << file_name.value();
    return false;
  }

  struct stat file_stat;
  if (fstat(file_, &file_stat) == -1) {
    LOG(ERROR) << "Couldn't fstat " << file_name.value() << ", errno " << errno;
    return false;
  }
  length_ = file_stat.st_size;

  data_ = static_cast<uint8*>(
      mmap(NULL, length_, PROT_READ, MAP_SHARED, file_, 0));
  if (data_ == MAP_FAILED)
    LOG(ERROR) << "Couldn't mmap " << file_name.value() << ", errno " << errno;

  return data_ != MAP_FAILED;
}

void MemoryMappedFile::CloseHandles() {
  if (data_ != NULL)
    munmap(data_, length_);
  if (file_ != -1)
    close(file_);

  data_ = NULL;
  length_ = 0;
  file_ = -1;
}

} // namespace file_util
