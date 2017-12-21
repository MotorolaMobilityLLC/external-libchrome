// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/trace_config_category_filter.h"

#include "base/memory/ptr_util.h"
#include "base/strings/pattern.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"

namespace base {
namespace trace_event {

namespace {
const char kIncludedCategoriesParam[] = "included_categories";
const char kExcludedCategoriesParam[] = "excluded_categories";
const char kSyntheticDelaysParam[] = "synthetic_delays";

const char kSyntheticDelayCategoryFilterPrefix[] = "DELAY(";
}

TraceConfigCategoryFilter::TraceConfigCategoryFilter() {}

TraceConfigCategoryFilter::TraceConfigCategoryFilter(
    const TraceConfigCategoryFilter& other)
    : included_categories_(other.included_categories_),
      disabled_categories_(other.disabled_categories_),
      excluded_categories_(other.excluded_categories_),
      synthetic_delays_(other.synthetic_delays_) {}

TraceConfigCategoryFilter::~TraceConfigCategoryFilter() {}

TraceConfigCategoryFilter& TraceConfigCategoryFilter::operator=(
    const TraceConfigCategoryFilter& rhs) {
  included_categories_ = rhs.included_categories_;
  disabled_categories_ = rhs.disabled_categories_;
  excluded_categories_ = rhs.excluded_categories_;
  synthetic_delays_ = rhs.synthetic_delays_;
  return *this;
}

void TraceConfigCategoryFilter::InitializeFromString(
    const StringPiece& category_filter_string) {
  std::vector<std::string> split =
      SplitString(category_filter_string, ",", TRIM_WHITESPACE, SPLIT_WANT_ALL);
  for (const std::string& category : split) {
    // Ignore empty categories.
    if (category.empty())
      continue;
    // Synthetic delays are of the form 'DELAY(delay;option;option;...)'.
    if (StartsWith(category, kSyntheticDelayCategoryFilterPrefix,
                   CompareCase::SENSITIVE) &&
        category.back() == ')') {
      std::string synthetic_category = category.substr(
          strlen(kSyntheticDelayCategoryFilterPrefix),
          category.size() - strlen(kSyntheticDelayCategoryFilterPrefix) - 1);
      size_t name_length = synthetic_category.find(';');
      if (name_length != std::string::npos && name_length > 0 &&
          name_length != synthetic_category.size() - 1) {
        synthetic_delays_.push_back(synthetic_category);
      }
    } else if (category.front() == '-') {
      // Excluded categories start with '-'.
      // Remove '-' from category string.
      excluded_categories_.push_back(category.substr(1));
    } else if (category.compare(0, strlen(TRACE_DISABLED_BY_DEFAULT("")),
                                TRACE_DISABLED_BY_DEFAULT("")) == 0) {
      disabled_categories_.push_back(category);
    } else {
      included_categories_.push_back(category);
    }
  }
}

void TraceConfigCategoryFilter::InitializeFromConfigDict(
    const DictionaryValue& dict) {
  const ListValue* category_list = nullptr;
  if (dict.GetList(kIncludedCategoriesParam, &category_list))
    SetCategoriesFromIncludedList(*category_list);
  if (dict.GetList(kExcludedCategoriesParam, &category_list))
    SetCategoriesFromExcludedList(*category_list);
  if (dict.GetList(kSyntheticDelaysParam, &category_list))
    SetSyntheticDelaysFromList(*category_list);
}

bool TraceConfigCategoryFilter::IsCategoryGroupEnabled(
    const char* category_group_name) const {
  bool had_enabled_by_default = false;
  DCHECK(category_group_name);
  std::string category_group_name_str = category_group_name;
  StringTokenizer category_group_tokens(category_group_name_str, ",");
  while (category_group_tokens.GetNext()) {
    std::string category_group_token = category_group_tokens.token();
    // Don't allow empty tokens, nor tokens with leading or trailing space.
    DCHECK(IsCategoryNameAllowed(category_group_token))
        << "Disallowed category string";
    if (IsCategoryEnabled(category_group_token.c_str()))
      return true;

    if (!MatchPattern(category_group_token, TRACE_DISABLED_BY_DEFAULT("*")))
      had_enabled_by_default = true;
  }
  // Do a second pass to check for explicitly disabled categories
  // (those explicitly enabled have priority due to first pass).
  category_group_tokens.Reset();
  bool category_group_disabled = false;
  while (category_group_tokens.GetNext()) {
    std::string category_group_token = category_group_tokens.token();
    for (const std::string& category : excluded_categories_) {
      if (MatchPattern(category_group_token, category)) {
        // Current token of category_group_name is present in excluded_list.
        // Flag the exclusion and proceed further to check if any of the
        // remaining categories of category_group_name is not present in the
        // excluded_ list.
        category_group_disabled = true;
        break;
      }
      // One of the category of category_group_name is not present in
      // excluded_ list. So, if it's not a disabled-by-default category,
      // it has to be included_ list. Enable the category_group_name
      // for recording.
      if (!MatchPattern(category_group_token, TRACE_DISABLED_BY_DEFAULT("*")))
        category_group_disabled = false;
    }
    // One of the categories present in category_group_name is not present in
    // excluded_ list. Implies this category_group_name group can be enabled
    // for recording, since one of its groups is enabled for recording.
    if (!category_group_disabled)
      break;
  }
  // If the category group is not excluded, and there are no included patterns
  // we consider this category group enabled, as long as it had categories
  // other than disabled-by-default.
  return !category_group_disabled && had_enabled_by_default &&
         included_categories_.empty();
}

bool TraceConfigCategoryFilter::IsCategoryEnabled(
    const char* category_name) const {
  // Check the disabled- filters and the disabled-* wildcard first so that a
  // "*" filter does not include the disabled.
  for (const std::string& category : disabled_categories_) {
    if (MatchPattern(category_name, category))
      return true;
  }

  if (MatchPattern(category_name, TRACE_DISABLED_BY_DEFAULT("*")))
    return false;

  for (const std::string& category : included_categories_) {
    if (MatchPattern(category_name, category))
      return true;
  }

  return false;
}

void TraceConfigCategoryFilter::Merge(const TraceConfigCategoryFilter& config) {
  // Keep included patterns only if both filters have an included entry.
  // Otherwise, one of the filter was specifying "*" and we want to honor the
  // broadest filter.
  if (!included_categories_.empty() && !config.included_categories_.empty()) {
    included_categories_.insert(included_categories_.end(),
                                config.included_categories_.begin(),
                                config.included_categories_.end());
  } else {
    included_categories_.clear();
  }

  disabled_categories_.insert(disabled_categories_.end(),
                              config.disabled_categories_.begin(),
                              config.disabled_categories_.end());
  excluded_categories_.insert(excluded_categories_.end(),
                              config.excluded_categories_.begin(),
                              config.excluded_categories_.end());
  synthetic_delays_.insert(synthetic_delays_.end(),
                           config.synthetic_delays_.begin(),
                           config.synthetic_delays_.end());
}

void TraceConfigCategoryFilter::Clear() {
  included_categories_.clear();
  disabled_categories_.clear();
  excluded_categories_.clear();
  synthetic_delays_.clear();
}

void TraceConfigCategoryFilter::ToDict(DictionaryValue* dict) const {
  StringList categories(included_categories_);
  categories.insert(categories.end(), disabled_categories_.begin(),
                    disabled_categories_.end());
  AddCategoriesToDict(categories, kIncludedCategoriesParam, dict);
  AddCategoriesToDict(excluded_categories_, kExcludedCategoriesParam, dict);
  AddCategoriesToDict(synthetic_delays_, kSyntheticDelaysParam, dict);
}

std::string TraceConfigCategoryFilter::ToFilterString() const {
  std::string filter_string;
  WriteCategoryFilterString(included_categories_, &filter_string, true);
  WriteCategoryFilterString(disabled_categories_, &filter_string, true);
  WriteCategoryFilterString(excluded_categories_, &filter_string, false);
  WriteCategoryFilterString(synthetic_delays_, &filter_string);
  return filter_string;
}

void TraceConfigCategoryFilter::SetCategoriesFromIncludedList(
    const ListValue& included_list) {
  included_categories_.clear();
  for (size_t i = 0; i < included_list.GetSize(); ++i) {
    std::string category;
    if (!included_list.GetString(i, &category))
      continue;
    if (category.compare(0, strlen(TRACE_DISABLED_BY_DEFAULT("")),
                         TRACE_DISABLED_BY_DEFAULT("")) == 0) {
      disabled_categories_.push_back(category);
    } else {
      included_categories_.push_back(category);
    }
  }
}

void TraceConfigCategoryFilter::SetCategoriesFromExcludedList(
    const ListValue& excluded_list) {
  excluded_categories_.clear();
  for (size_t i = 0; i < excluded_list.GetSize(); ++i) {
    std::string category;
    if (excluded_list.GetString(i, &category))
      excluded_categories_.push_back(category);
  }
}

void TraceConfigCategoryFilter::SetSyntheticDelaysFromList(
    const ListValue& list) {
  for (size_t i = 0; i < list.GetSize(); ++i) {
    std::string delay;
    if (!list.GetString(i, &delay))
      continue;
    // Synthetic delays are of the form "delay;option;option;...".
    size_t name_length = delay.find(';');
    if (name_length != std::string::npos && name_length > 0 &&
        name_length != delay.size() - 1) {
      synthetic_delays_.push_back(delay);
    }
  }
}

void TraceConfigCategoryFilter::AddCategoriesToDict(
    const StringList& categories,
    const char* param,
    DictionaryValue* dict) const {
  if (categories.empty())
    return;

  auto list = MakeUnique<ListValue>();
  for (const std::string& category : categories)
    list->AppendString(category);
  dict->Set(param, std::move(list));
}

void TraceConfigCategoryFilter::WriteCategoryFilterString(
    const StringList& values,
    std::string* out,
    bool included) const {
  bool prepend_comma = !out->empty();
  int token_cnt = 0;
  for (const std::string& category : values) {
    if (token_cnt > 0 || prepend_comma)
      StringAppendF(out, ",");
    StringAppendF(out, "%s%s", (included ? "" : "-"), category.c_str());
    ++token_cnt;
  }
}

void TraceConfigCategoryFilter::WriteCategoryFilterString(
    const StringList& delays,
    std::string* out) const {
  bool prepend_comma = !out->empty();
  int token_cnt = 0;
  for (const std::string& category : delays) {
    if (token_cnt > 0 || prepend_comma)
      StringAppendF(out, ",");
    StringAppendF(out, "%s%s)", kSyntheticDelayCategoryFilterPrefix,
                  category.c_str());
    ++token_cnt;
  }
}

// static
bool TraceConfigCategoryFilter::IsCategoryNameAllowed(StringPiece str) {
  return !str.empty() && str.front() != ' ' && str.back() != ' ';
}

}  // namespace trace_event
}  // namespace base
