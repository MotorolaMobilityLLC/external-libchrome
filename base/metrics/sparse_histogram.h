// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_METRICS_SPARSE_HISTOGRAM_H_
#define BASE_METRICS_SPARSE_HISTOGRAM_H_

#include <map>
#include <string>

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/metrics/histogram_base.h"
#include "base/synchronization/lock.h"

namespace base {

class BASE_EXPORT_PRIVATE SparseHistogram : public HistogramBase {
 public:
  // If there's one with same name, return the existing one. If not, create a
  // new one.
  static HistogramBase* FactoryGet(const std::string& name, int32 flags);

  virtual ~SparseHistogram();

  virtual void Add(Sample value) OVERRIDE;

  virtual void SnapshotSample(std::map<Sample, Count>* sample) const;
  virtual void WriteHTMLGraph(std::string* output) const OVERRIDE;
  virtual void WriteAscii(std::string* output) const OVERRIDE;

 protected:
  // Clients should always use FactoryGet to create SparseHistogram.
  SparseHistogram(const std::string& name);

 private:
  friend class SparseHistogramTest;  // For constuctor calling.

  std::map<Sample, Count> samples_;

  // Protects access to above map.
  mutable base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(SparseHistogram);
};

}  // namespace base

#endif  // BASE_METRICS_SPARSE_HISTOGRAM_H_
