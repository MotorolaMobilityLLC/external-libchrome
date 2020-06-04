// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SUPPORTS_USER_DATA_H_
#define BASE_SUPPORTS_USER_DATA_H_

#include <map>

#include "base/base_export.h"
#include "base/memory/linked_ptr.h"

namespace base {

// This is a helper for classes that want to allow users to stash random data by
// key. At destruction all the objects will be destructed.
class BASE_EXPORT SupportsUserData {
 public:
  SupportsUserData();
  virtual ~SupportsUserData();

  // Derive from this class and add your own data members to associate extra
  // information with this object. Use GetUserData(key) and SetUserData()
  class BASE_EXPORT Data {
   public:
    virtual ~Data() {}
  };

  // The user data allows the clients to associate data with this object.
  // Multiple user data values can be stored under different keys.
  // This object will TAKE OWNERSHIP of the given data pointer, and will
  // delete the object if it is changed or the object is destroyed.
  Data* GetUserData(const void* key) const;
  void SetUserData(const void* key, Data* data);

 private:
  typedef std::map<const void*, linked_ptr<Data> > DataMap;

  // Externally-defined data accessible by key
  DataMap user_data_;

  DISALLOW_COPY_AND_ASSIGN(SupportsUserData);
};

}  // namespace base

#endif  // BASE_SUPPORTS_USER_DATA_H_
