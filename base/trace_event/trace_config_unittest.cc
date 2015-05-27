// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/trace_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace trace_event {

namespace {

const char kDefaultTraceConfigString[] =
  "{"
    "\"enable_argument_filter\":false,"
    "\"enable_sampling\":false,"
    "\"enable_systrace\":false,"
    "\"excluded_categories\":[\"*Debug\",\"*Test\"],"
    "\"record_mode\":\"record-until-full\""
  "}";

}  // namespace

TEST(TraceConfigTest, TraceConfigFromValidLegacyStrings) {
  // From trace options strings
  TraceConfig config("", "record-until-full");
  EXPECT_EQ(RECORD_UNTIL_FULL, config.record_mode_);
  EXPECT_FALSE(config.enable_sampling_);
  EXPECT_FALSE(config.enable_systrace_);
  EXPECT_FALSE(config.enable_argument_filter_);
  EXPECT_STREQ("record-until-full", config.ToTraceOptionsString().c_str());

  config = TraceConfig("", "record-continuously");
  EXPECT_EQ(RECORD_CONTINUOUSLY, config.record_mode_);
  EXPECT_FALSE(config.enable_sampling_);
  EXPECT_FALSE(config.enable_systrace_);
  EXPECT_FALSE(config.enable_argument_filter_);
  EXPECT_STREQ("record-continuously", config.ToTraceOptionsString().c_str());

  config = TraceConfig("", "trace-to-console");
  EXPECT_EQ(ECHO_TO_CONSOLE, config.record_mode_);
  EXPECT_FALSE(config.enable_sampling_);
  EXPECT_FALSE(config.enable_systrace_);
  EXPECT_FALSE(config.enable_argument_filter_);
  EXPECT_STREQ("trace-to-console", config.ToTraceOptionsString().c_str());

  config = TraceConfig("", "record-as-much-as-possible");
  EXPECT_EQ(RECORD_AS_MUCH_AS_POSSIBLE, config.record_mode_);
  EXPECT_FALSE(config.enable_sampling_);
  EXPECT_FALSE(config.enable_systrace_);
  EXPECT_FALSE(config.enable_argument_filter_);
  EXPECT_STREQ("record-as-much-as-possible",
               config.ToTraceOptionsString().c_str());

  config = TraceConfig("", "record-until-full, enable-sampling");
  EXPECT_EQ(RECORD_UNTIL_FULL, config.record_mode_);
  EXPECT_TRUE(config.enable_sampling_);
  EXPECT_FALSE(config.enable_systrace_);
  EXPECT_FALSE(config.enable_argument_filter_);
  EXPECT_STREQ("record-until-full,enable-sampling",
               config.ToTraceOptionsString().c_str());

  config = TraceConfig("", "enable-systrace, record-continuously");
  EXPECT_EQ(RECORD_CONTINUOUSLY, config.record_mode_);
  EXPECT_FALSE(config.enable_sampling_);
  EXPECT_TRUE(config.enable_systrace_);
  EXPECT_FALSE(config.enable_argument_filter_);
  EXPECT_STREQ("record-continuously,enable-systrace",
               config.ToTraceOptionsString().c_str());

  config = TraceConfig("", "enable-argument-filter,record-as-much-as-possible");
  EXPECT_EQ(RECORD_AS_MUCH_AS_POSSIBLE, config.record_mode_);
  EXPECT_FALSE(config.enable_sampling_);
  EXPECT_FALSE(config.enable_systrace_);
  EXPECT_TRUE(config.enable_argument_filter_);
  EXPECT_STREQ("record-as-much-as-possible,enable-argument-filter",
               config.ToTraceOptionsString().c_str());

  config = TraceConfig(
    "",
    "enable-systrace,trace-to-console,enable-sampling,enable-argument-filter");
  EXPECT_EQ(ECHO_TO_CONSOLE, config.record_mode_);
  EXPECT_TRUE(config.enable_sampling_);
  EXPECT_TRUE(config.enable_systrace_);
  EXPECT_TRUE(config.enable_argument_filter_);
  EXPECT_STREQ(
    "trace-to-console,enable-sampling,enable-systrace,enable-argument-filter",
    config.ToTraceOptionsString().c_str());

  config = TraceConfig(
    "", "record-continuously, record-until-full, trace-to-console");
  EXPECT_EQ(ECHO_TO_CONSOLE, config.record_mode_);
  EXPECT_FALSE(config.enable_systrace_);
  EXPECT_FALSE(config.enable_sampling_);
  EXPECT_FALSE(config.enable_argument_filter_);
  EXPECT_STREQ("trace-to-console", config.ToTraceOptionsString().c_str());

  // From category filter strings
  config = TraceConfig("-*Debug,-*Test", "");
  EXPECT_STREQ("-*Debug,-*Test", config.ToCategoryFilterString().c_str());

  config = TraceConfig("included,-excluded,inc_pattern*,-exc_pattern*", "");
  EXPECT_STREQ("included,inc_pattern*,-excluded,-exc_pattern*",
               config.ToCategoryFilterString().c_str());

  config = TraceConfig("only_inc_cat", "");
  EXPECT_STREQ("only_inc_cat", config.ToCategoryFilterString().c_str());

  config = TraceConfig("-only_exc_cat", "");
  EXPECT_STREQ("-only_exc_cat", config.ToCategoryFilterString().c_str());

  config = TraceConfig("disabled-by-default-cc,-excluded", "");
  EXPECT_STREQ("disabled-by-default-cc,-excluded",
               config.ToCategoryFilterString().c_str());

  config = TraceConfig("disabled-by-default-cc,included", "");
  EXPECT_STREQ("included,disabled-by-default-cc",
               config.ToCategoryFilterString().c_str());

  config = TraceConfig("DELAY(test.Delay1;16),included", "");
  EXPECT_STREQ("included,DELAY(test.Delay1;16)",
               config.ToCategoryFilterString().c_str());

  // From both trace options and category filter strings
  config = TraceConfig("", "");
  EXPECT_EQ(RECORD_UNTIL_FULL, config.record_mode_);
  EXPECT_FALSE(config.enable_systrace_);
  EXPECT_FALSE(config.enable_sampling_);
  EXPECT_FALSE(config.enable_argument_filter_);
  EXPECT_STREQ("", config.ToCategoryFilterString().c_str());
  EXPECT_STREQ("record-until-full", config.ToTraceOptionsString().c_str());

  config = TraceConfig("included,-excluded,inc_pattern*,-exc_pattern*",
                       "enable-systrace, trace-to-console, enable-sampling");
  EXPECT_STREQ("included,inc_pattern*,-excluded,-exc_pattern*",
               config.ToCategoryFilterString().c_str());
  EXPECT_STREQ("trace-to-console,enable-sampling,enable-systrace",
               config.ToTraceOptionsString().c_str());

  // From both trace options and category filter strings with spaces.
  config = TraceConfig(" included , -excluded, inc_pattern*, ,-exc_pattern*   ",
                       "enable-systrace, ,trace-to-console, enable-sampling  ");
  EXPECT_STREQ("included,inc_pattern*,-excluded,-exc_pattern*",
               config.ToCategoryFilterString().c_str());
  EXPECT_STREQ("trace-to-console,enable-sampling,enable-systrace",
               config.ToTraceOptionsString().c_str());
}

TEST(TraceConfigTest, TraceConfigFromInvalidLegacyStrings) {
  TraceConfig config("", "foo-bar-baz");
  EXPECT_EQ(RECORD_UNTIL_FULL, config.record_mode_);
  EXPECT_FALSE(config.enable_systrace_);
  EXPECT_FALSE(config.enable_sampling_);
  EXPECT_FALSE(config.enable_argument_filter_);
  EXPECT_STREQ("", config.ToCategoryFilterString().c_str());
  EXPECT_STREQ("record-until-full", config.ToTraceOptionsString().c_str());

  config = TraceConfig("arbitrary-category", "foo-bar-baz, enable-systrace");
  EXPECT_EQ(RECORD_UNTIL_FULL, config.record_mode_);
  EXPECT_TRUE(config.enable_systrace_);
  EXPECT_FALSE(config.enable_sampling_);
  EXPECT_FALSE(config.enable_argument_filter_);
  EXPECT_STREQ("arbitrary-category", config.ToCategoryFilterString().c_str());
  EXPECT_STREQ("record-until-full,enable-systrace",
               config.ToTraceOptionsString().c_str());

  const char* const configs[] = {
    "",
    "DELAY(",
    "DELAY(;",
    "DELAY(;)",
    "DELAY(test.Delay)",
    "DELAY(test.Delay;)"
  };
  for (size_t i = 0; i < arraysize(configs); i++) {
    TraceConfig tc(configs[i], "");
    EXPECT_EQ(0u, tc.GetSyntheticDelayValues().size());
  }
}

TEST(TraceConfigTest, ConstructDefaultTraceConfig) {
  // Make sure that upon an empty string, we fall back to the default config.
  TraceConfig tc;
  EXPECT_STREQ(kDefaultTraceConfigString, tc.ToString().c_str());
  EXPECT_TRUE(tc.record_mode_ == RECORD_UNTIL_FULL);
  EXPECT_FALSE(tc.enable_sampling_);
  EXPECT_FALSE(tc.enable_systrace_);
  EXPECT_FALSE(tc.enable_argument_filter_);
  EXPECT_STREQ("-*Debug,-*Test", tc.ToCategoryFilterString().c_str());
  EXPECT_FALSE(tc.IsCategoryGroupEnabled("CategoryDebug"));
  EXPECT_FALSE(tc.IsCategoryGroupEnabled("CategoryTest"));
  EXPECT_FALSE(tc.IsCategoryGroupEnabled("CategoryDebug,CategoryTest"));
  EXPECT_TRUE(tc.IsCategoryGroupEnabled("Category1,CategoryDebug"));
  EXPECT_TRUE(tc.IsCategoryGroupEnabled("CategoryDebug,Category1"));
  EXPECT_TRUE(tc.IsCategoryGroupEnabled("CategoryTest,Category2"));
  EXPECT_TRUE(tc.IsCategoryGroupEnabled("not-excluded-category"));
  EXPECT_FALSE(tc.IsCategoryGroupEnabled("disabled-by-default-cc"));
}

TEST(TraceConfigTest, TraceConfigFromValidString) {
  // Using some non-empty config string.
  const char config_string[] =
    "{"
      "\"enable_argument_filter\":true,"
      "\"enable_sampling\":true,"
      "\"enable_systrace\":true,"
      "\"excluded_categories\":[\"excluded\",\"exc_pattern*\"],"
      "\"included_categories\":[\"included\",\"inc_pattern*\"],"
      "\"record_mode\":\"record-continuously\","
      "\"synthetic_delays\":[\"test.Delay1;16\",\"test.Delay2;32\"]"
    "}";
  TraceConfig tc(config_string);
  EXPECT_STREQ(config_string, tc.ToString().c_str());
  EXPECT_TRUE(tc.record_mode_ == RECORD_CONTINUOUSLY);
  EXPECT_TRUE(tc.enable_sampling_);
  EXPECT_TRUE(tc.enable_systrace_);
  EXPECT_TRUE(tc.enable_argument_filter_);
  EXPECT_STREQ("included,inc_pattern*,-excluded,-exc_pattern*,"
               "DELAY(test.Delay1;16),DELAY(test.Delay2;32)",
               tc.ToCategoryFilterString().c_str());
  EXPECT_TRUE(tc.IsCategoryGroupEnabled("included"));
  EXPECT_TRUE(tc.IsCategoryGroupEnabled("included,excluded"));
  EXPECT_TRUE(tc.IsCategoryGroupEnabled("inc_pattern_category"));
  EXPECT_FALSE(tc.IsCategoryGroupEnabled("exc_pattern_category"));
  EXPECT_FALSE(tc.IsCategoryGroupEnabled("excluded"));
  EXPECT_FALSE(tc.IsCategoryGroupEnabled("not-excluded-nor-included"));
  EXPECT_FALSE(tc.IsCategoryGroupEnabled("Category1,CategoryDebug"));
  EXPECT_FALSE(tc.IsCategoryGroupEnabled("CategoryDebug,Category1"));
  EXPECT_FALSE(tc.IsCategoryGroupEnabled("CategoryTest,Category2"));
  EXPECT_TRUE(tc.IsCategoryGroupEnabled("included,DELAY(test.Delay1;16)"));
  EXPECT_FALSE(tc.IsCategoryGroupEnabled("DELAY(test.Delay1;16)"));
  EXPECT_EQ(2u, tc.GetSyntheticDelayValues().size());
  EXPECT_STREQ("test.Delay1;16", tc.GetSyntheticDelayValues()[0].c_str());
  EXPECT_STREQ("test.Delay2;32", tc.GetSyntheticDelayValues()[1].c_str());

  // Clear
  tc.Clear();
  EXPECT_STREQ(tc.ToString().c_str(),
               "{"
                 "\"enable_argument_filter\":false,"
                 "\"enable_sampling\":false,"
                 "\"enable_systrace\":false,"
                 "\"record_mode\":\"record-until-full\""
               "}");
}

TEST(TraceConfigTest, TraceConfigFromInvalidString) {
  // The config string needs to be a dictionary correctly formatted as a JSON
  // string. Otherwise, it will fall back to the default initialization.
  TraceConfig tc("");
  EXPECT_STREQ(kDefaultTraceConfigString, tc.ToString().c_str());
  EXPECT_TRUE(tc.record_mode_ == RECORD_UNTIL_FULL);
  EXPECT_FALSE(tc.enable_sampling_);
  EXPECT_FALSE(tc.enable_systrace_);
  EXPECT_FALSE(tc.enable_argument_filter_);
  EXPECT_STREQ("-*Debug,-*Test", tc.ToCategoryFilterString().c_str());

  tc = TraceConfig("This is an invalid config string.");
  EXPECT_STREQ(kDefaultTraceConfigString, tc.ToString().c_str());
  EXPECT_TRUE(tc.record_mode_ == RECORD_UNTIL_FULL);
  EXPECT_FALSE(tc.enable_sampling_);
  EXPECT_FALSE(tc.enable_systrace_);
  EXPECT_FALSE(tc.enable_argument_filter_);
  EXPECT_STREQ("-*Debug,-*Test", tc.ToCategoryFilterString().c_str());

  tc = TraceConfig("[\"This\", \"is\", \"not\", \"a\", \"dictionary\"]");
  EXPECT_STREQ(kDefaultTraceConfigString, tc.ToString().c_str());
  EXPECT_TRUE(tc.record_mode_ == RECORD_UNTIL_FULL);
  EXPECT_FALSE(tc.enable_sampling_);
  EXPECT_FALSE(tc.enable_systrace_);
  EXPECT_FALSE(tc.enable_argument_filter_);
  EXPECT_STREQ("-*Debug,-*Test", tc.ToCategoryFilterString().c_str());

  tc = TraceConfig("{\"record_mode\": invalid-value-needs-double-quote}");
  EXPECT_STREQ(kDefaultTraceConfigString, tc.ToString().c_str());
  EXPECT_TRUE(tc.record_mode_ == RECORD_UNTIL_FULL);
  EXPECT_FALSE(tc.enable_sampling_);
  EXPECT_FALSE(tc.enable_systrace_);
  EXPECT_FALSE(tc.enable_argument_filter_);
  EXPECT_STREQ("-*Debug,-*Test", tc.ToCategoryFilterString().c_str());

  // If the config string a dictionary formatted as a JSON string, it will
  // initialize TraceConfig with best effort.
  tc = TraceConfig("{}");
  EXPECT_TRUE(tc.record_mode_ == RECORD_UNTIL_FULL);
  EXPECT_FALSE(tc.enable_sampling_);
  EXPECT_FALSE(tc.enable_systrace_);
  EXPECT_FALSE(tc.enable_argument_filter_);
  EXPECT_STREQ("", tc.ToCategoryFilterString().c_str());

  tc = TraceConfig("{\"arbitrary-key\":\"arbitrary-value\"}");
  EXPECT_TRUE(tc.record_mode_ == RECORD_UNTIL_FULL);
  EXPECT_FALSE(tc.enable_sampling_);
  EXPECT_FALSE(tc.enable_systrace_);
  EXPECT_FALSE(tc.enable_argument_filter_);
  EXPECT_STREQ("", tc.ToCategoryFilterString().c_str());

  const char invalid_config_string[] =
    "{"
      "\"enable_sampling\":\"true\","
      "\"enable_systrace\":1,"
      "\"excluded_categories\":[\"excluded\"],"
      "\"included_categories\":\"not a list\","
      "\"record_mode\":\"arbitrary-mode\","
      "\"synthetic_delays\":[\"test.Delay1;16\","
                            "\"invalid-delay\","
                            "\"test.Delay2;32\"]"
    "}";
  tc = TraceConfig(invalid_config_string);
  EXPECT_TRUE(tc.record_mode_ == RECORD_UNTIL_FULL);
  EXPECT_FALSE(tc.enable_sampling_);
  EXPECT_FALSE(tc.enable_systrace_);
  EXPECT_FALSE(tc.enable_argument_filter_);
  EXPECT_STREQ("-excluded,DELAY(test.Delay1;16),DELAY(test.Delay2;32)",
               tc.ToCategoryFilterString().c_str());
}

TEST(TraceConfigTest, MergingTraceConfigs) {
  // Merge
  TraceConfig tc;
  TraceConfig tc2("included,-excluded,inc_pattern*,-exc_pattern*", "");
  tc.Merge(tc2);
  EXPECT_STREQ("{"
                 "\"enable_argument_filter\":false,"
                 "\"enable_sampling\":false,"
                 "\"enable_systrace\":false,"
                 "\"excluded_categories\":["
                   "\"*Debug\",\"*Test\",\"excluded\",\"exc_pattern*\""
                 "],"
                 "\"record_mode\":\"record-until-full\""
               "}",
               tc.ToString().c_str());

  tc = TraceConfig("DELAY(test.Delay1;16)", "");
  tc2 = TraceConfig("DELAY(test.Delay2;32)", "");
  tc.Merge(tc2);
  EXPECT_EQ(2u, tc.GetSyntheticDelayValues().size());
  EXPECT_STREQ("test.Delay1;16", tc.GetSyntheticDelayValues()[0].c_str());
  EXPECT_STREQ("test.Delay2;32", tc.GetSyntheticDelayValues()[1].c_str());
}

TEST(TraceConfigTest, IsCategoryGroupEnabled) {
  // Enabling a disabled- category does not require all categories to be traced
  // to be included.
  TraceConfig tc("disabled-by-default-cc,-excluded", "");
  EXPECT_STREQ("disabled-by-default-cc,-excluded",
               tc.ToCategoryFilterString().c_str());
  EXPECT_TRUE(tc.IsCategoryGroupEnabled("disabled-by-default-cc"));
  EXPECT_TRUE(tc.IsCategoryGroupEnabled("some_other_group"));
  EXPECT_FALSE(tc.IsCategoryGroupEnabled("excluded"));

  // Enabled a disabled- category and also including makes all categories to
  // be traced require including.
  tc = TraceConfig("disabled-by-default-cc,included", "");
  EXPECT_STREQ("included,disabled-by-default-cc",
               tc.ToCategoryFilterString().c_str());
  EXPECT_TRUE(tc.IsCategoryGroupEnabled("disabled-by-default-cc"));
  EXPECT_TRUE(tc.IsCategoryGroupEnabled("included"));
  EXPECT_FALSE(tc.IsCategoryGroupEnabled("other_included"));
}

TEST(TraceConfigTest, IsEmptyOrContainsLeadingOrTrailingWhitespace) {
  // Test that IsEmptyOrContainsLeadingOrTrailingWhitespace actually catches
  // categories that are explicitly forbidden.
  // This method is called in a DCHECK to assert that we don't have these types
  // of strings as categories.
  EXPECT_TRUE(TraceConfig::IsEmptyOrContainsLeadingOrTrailingWhitespace(
      " bad_category "));
  EXPECT_TRUE(TraceConfig::IsEmptyOrContainsLeadingOrTrailingWhitespace(
      " bad_category"));
  EXPECT_TRUE(TraceConfig::IsEmptyOrContainsLeadingOrTrailingWhitespace(
      "bad_category "));
  EXPECT_TRUE(TraceConfig::IsEmptyOrContainsLeadingOrTrailingWhitespace(
      "   bad_category"));
  EXPECT_TRUE(TraceConfig::IsEmptyOrContainsLeadingOrTrailingWhitespace(
      "bad_category   "));
  EXPECT_TRUE(TraceConfig::IsEmptyOrContainsLeadingOrTrailingWhitespace(
      "   bad_category   "));
  EXPECT_TRUE(TraceConfig::IsEmptyOrContainsLeadingOrTrailingWhitespace(
      ""));
  EXPECT_FALSE(TraceConfig::IsEmptyOrContainsLeadingOrTrailingWhitespace(
      "good_category"));
}

}  // namespace trace_event
}  // namespace base
