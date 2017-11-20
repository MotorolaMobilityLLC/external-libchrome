// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_OBSERVER_LIST_H_
#define BASE_OBSERVER_LIST_H_

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"

///////////////////////////////////////////////////////////////////////////////
//
// OVERVIEW:
//
//   A container for a list of observers.  Unlike a normal STL vector or list,
//   this container can be modified during iteration without invalidating the
//   iterator.  So, it safely handles the case of an observer removing itself
//   or other observers from the list while observers are being notified.
//
// TYPICAL USAGE:
//
//   class MyWidget {
//    public:
//     ...
//
//     class Observer {
//      public:
//       virtual void OnFoo(MyWidget* w) = 0;
//       virtual void OnBar(MyWidget* w, int x, int y) = 0;
//     };
//
//     void AddObserver(Observer* obs) {
//       observer_list_.AddObserver(obs);
//     }
//
//     void RemoveObserver(const Observer* obs) {
//       observer_list_.RemoveObserver(obs);
//     }
//
//     void NotifyFoo() {
//       for (auto& observer : observer_list_)
//         observer.OnFoo(this);
//     }
//
//     void NotifyBar(int x, int y) {
//       for (FooList::iterator i = observer_list.begin(),
//           e = observer_list.end(); i != e; ++i)
//        i->OnBar(this, x, y);
//     }
//
//    private:
//     base::ObserverList<Observer> observer_list_;
//   };
//
//
///////////////////////////////////////////////////////////////////////////////

namespace base {

template <class ObserverType>
class ObserverListBase
    : public SupportsWeakPtr<ObserverListBase<ObserverType>> {
 public:
  // Enumeration of which observers are notified.
  enum NotificationType {
    // Specifies that any observers added during notification are notified.
    // This is the default type if no type is provided to the constructor.
    NOTIFY_ALL,

    // Specifies that observers added while sending out notification are not
    // notified.
    NOTIFY_EXISTING_ONLY
  };

  // An iterator class that can be used to access the list of observers.
  class Iter {
   public:
    Iter() : index_(0), max_index_(0) {}

    explicit Iter(const ObserverListBase* list)
        : list_(const_cast<ObserverListBase*>(list)->AsWeakPtr()),
          index_(0),
          max_index_(list->type_ == NOTIFY_ALL
                         ? std::numeric_limits<size_t>::max()
                         : list->observers_.size()) {
      DCHECK(list_);
      EnsureValidIndex();
      ++list_->live_iterator_count_;
    }

    ~Iter() {
      if (!list_)
        return;

      DCHECK_GT(list_->live_iterator_count_, 0);
      if (--list_->live_iterator_count_ == 0)
        list_->Compact();
    }

    Iter(const Iter& other)
        : list_(other.list_),
          index_(other.index_),
          max_index_(other.max_index_) {
      if (list_)
        ++list_->live_iterator_count_;
    }

    Iter& operator=(Iter other) {
      using std::swap;
      swap(list_, other.list_);
      swap(index_, other.index_);
      swap(max_index_, other.max_index_);
      return *this;
    }

    bool operator==(const Iter& other) const {
      return (is_end() && other.is_end()) ||
             (list_.get() == other.list_.get() && index_ == other.index_);
    }

    bool operator!=(const Iter& other) const { return !(*this == other); }

    Iter& operator++() {
      if (list_) {
        ++index_;
        EnsureValidIndex();
      }
      return *this;
    }

    ObserverType* operator->() const {
      ObserverType* const current = GetCurrent();
      DCHECK(current);
      return current;
    }

    ObserverType& operator*() const {
      ObserverType* const current = GetCurrent();
      DCHECK(current);
      return *current;
    }

   private:
    FRIEND_TEST_ALL_PREFIXES(ObserverListTest, BasicStdIterator);
    FRIEND_TEST_ALL_PREFIXES(ObserverListTest, StdIteratorRemoveFront);

    ObserverType* GetCurrent() const {
      DCHECK(list_);
      DCHECK_LT(index_, clamped_max_index());
      return list_->observers_[index_];
    }

    void EnsureValidIndex() {
      DCHECK(list_);
      const size_t max_index = clamped_max_index();
      while (index_ < max_index && !list_->observers_[index_])
        ++index_;
    }

    size_t clamped_max_index() const {
      return std::min(max_index_, list_->observers_.size());
    }

    bool is_end() const { return !list_ || index_ == clamped_max_index(); }

    WeakPtr<ObserverListBase> list_;

    // When initially constructed and each time the iterator is incremented,
    // |index_| is guaranteed to point to a non-null index if the iterator
    // has not reached the end of the ObserverList.
    size_t index_;
    size_t max_index_;
  };

  using iterator = Iter;
  using const_iterator = Iter;

  const_iterator begin() const {
    // An optimization: do not involve weak pointers for empty list.
    return observers_.empty() ? const_iterator() : const_iterator(this);
  }

  const_iterator end() const { return const_iterator(); }

  ObserverListBase() : live_iterator_count_(0), type_(NOTIFY_ALL) {}
  explicit ObserverListBase(NotificationType type)
      : live_iterator_count_(0), type_(type) {}

  // Add an observer to this list. An observer should not be added to the same
  // list more than once.
  //
  // Precondition: obs != nullptr
  // Precondition: !HasObserver(obs)
  void AddObserver(ObserverType* obs) {
    DCHECK(obs);
    if (HasObserver(obs)) {
      NOTREACHED() << "Observers can only be added once!";
      return;
    }
    observers_.push_back(obs);
  }

  // Removes the given observer from this list. Does nothing if this observer is
  // not in this list.
  void RemoveObserver(const ObserverType* obs) {
    DCHECK(obs);
    const auto it = std::find(observers_.begin(), observers_.end(), obs);
    if (it == observers_.end())
      return;

    DCHECK_GE(live_iterator_count_, 0);
    if (live_iterator_count_) {
      *it = nullptr;
    } else {
      observers_.erase(it);
    }
  }

  // Determine whether a particular observer is in the list.
  bool HasObserver(const ObserverType* obs) const {
    return ContainsValue(observers_, obs);
  }

  // Removes all the observers from this list.
  void Clear() {
    DCHECK_GE(live_iterator_count_, 0);
    if (live_iterator_count_) {
      std::fill(observers_.begin(), observers_.end(), nullptr);
    } else {
      observers_.clear();
    }
  }

 protected:
  size_t size() const { return observers_.size(); }

  void Compact() {
    observers_.erase(std::remove(observers_.begin(), observers_.end(), nullptr),
                     observers_.end());
  }

 private:
  std::vector<ObserverType*> observers_;

  // Number of active iterators referencing this ObserverListBase.
  //
  // This counter is not synchronized although it is modified by const
  // iterators.
  int live_iterator_count_;

  const NotificationType type_;

  DISALLOW_COPY_AND_ASSIGN(ObserverListBase);
};

template <class ObserverType, bool check_empty = false>
class ObserverList : public ObserverListBase<ObserverType> {
 public:
  using ObserverListBase<ObserverType>::ObserverListBase;

  ~ObserverList() {
    // When check_empty is true, assert that the list is empty on destruction.
    if (check_empty) {
      ObserverListBase<ObserverType>::Compact();
      DCHECK_EQ(ObserverListBase<ObserverType>::size(), 0U);
    }
  }

  bool might_have_observers() const {
    return ObserverListBase<ObserverType>::size() != 0;
  }
};

}  // namespace base

#endif  // BASE_OBSERVER_LIST_H_
