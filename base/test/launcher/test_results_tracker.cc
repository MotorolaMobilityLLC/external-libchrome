// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/launcher/test_results_tracker.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/test/launcher/test_launcher.h"
#include "base/values.h"

namespace base {

// See https://groups.google.com/a/chromium.org/d/msg/chromium-dev/nkdTP7sstSc/uT3FaE_sgkAJ .
using ::operator<<;

namespace {

// The default output file for XML output.
const FilePath::CharType kDefaultOutputFile[] = FILE_PATH_LITERAL(
    "test_detail.xml");

// Utility function to print a list of test names. Uses iterator to be
// compatible with different containers, like vector and set.
template<typename InputIterator>
void PrintTests(InputIterator first,
                InputIterator last,
                const std::string& description) {
  size_t count = std::distance(first, last);
  if (count == 0)
    return;

  fprintf(stdout,
          "%" PRIuS " test%s %s:\n",
          count,
          count != 1 ? "s" : "",
          description.c_str());
  for (InputIterator i = first; i != last; ++i)
    fprintf(stdout, "    %s\n", (*i).c_str());
  fflush(stdout);
}

}  // namespace

TestResultsTracker::TestResultsTracker() : iteration_(-1), out_(NULL) {
}

TestResultsTracker::~TestResultsTracker() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!out_)
    return;
  fprintf(out_, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  fprintf(out_, "<testsuites name=\"AllTests\" tests=\"\" failures=\"\""
          " disabled=\"\" errors=\"\" time=\"\">\n");
  for (PerIterationData::ResultsMap::iterator i =
           per_iteration_data_[iteration_].results.begin();
       i != per_iteration_data_[iteration_].results.end();
       ++i) {
    fprintf(out_, "  <testsuite name=\"%s\" tests=\"%" PRIuS "\" failures=\"\""
            " disabled=\"\" errors=\"\" time=\"\">\n",
            i->first.c_str(), i->second.size());
    for (size_t j = 0; j < i->second.size(); ++j) {
      const TestResult& result = i->second[j];
      fprintf(out_, "    <testcase name=\"%s\" status=\"run\" time=\"%.3f\""
              " classname=\"%s\">\n",
              result.GetTestName().c_str(),
              result.elapsed_time.InSecondsF(),
              result.GetTestCaseName().c_str());
      if (result.status != TestResult::TEST_SUCCESS)
        fprintf(out_, "      <failure message=\"\" type=\"\"></failure>\n");
      fprintf(out_, "    </testcase>\n");
    }
    fprintf(out_, "  </testsuite>\n");
  }
  fprintf(out_, "</testsuites>\n");
  fclose(out_);
}

bool TestResultsTracker::Init(const CommandLine& command_line) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Prevent initializing twice.
  if (out_) {
    NOTREACHED();
    return false;
  }

  if (!command_line.HasSwitch(kGTestOutputFlag))
    return true;

  std::string flag = command_line.GetSwitchValueASCII(kGTestOutputFlag);
  size_t colon_pos = flag.find(':');
  FilePath path;
  if (colon_pos != std::string::npos) {
    FilePath flag_path =
        command_line.GetSwitchValuePath(kGTestOutputFlag);
    FilePath::StringType path_string = flag_path.value();
    path = FilePath(path_string.substr(colon_pos + 1));
    // If the given path ends with '/', consider it is a directory.
    // Note: This does NOT check that a directory (or file) actually exists
    // (the behavior is same as what gtest does).
    if (path.EndsWithSeparator()) {
      FilePath executable = command_line.GetProgram().BaseName();
      path = path.Append(executable.ReplaceExtension(
                             FilePath::StringType(FILE_PATH_LITERAL("xml"))));
    }
  }
  if (path.value().empty())
    path = FilePath(kDefaultOutputFile);
  FilePath dir_name = path.DirName();
  if (!DirectoryExists(dir_name)) {
    LOG(WARNING) << "The output directory does not exist. "
                 << "Creating the directory: " << dir_name.value();
    // Create the directory if necessary (because the gtest does the same).
    if (!file_util::CreateDirectory(dir_name)) {
      LOG(ERROR) << "Failed to created directory " << dir_name.value();
      return false;
    }
  }
  out_ = file_util::OpenFile(path, "w");
  if (!out_) {
    LOG(ERROR) << "Cannot open output file: "
               << path.value() << ".";
    return false;
  }

  return true;
}

void TestResultsTracker::OnTestIterationStarting() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Start with a fresh state for new iteration.
  iteration_++;
  per_iteration_data_.push_back(PerIterationData());
}

void TestResultsTracker::AddTestResult(const TestResult& result) {
  DCHECK(thread_checker_.CalledOnValidThread());

  per_iteration_data_[iteration_].results[result.GetTestCaseName()].push_back(
      result);
  per_iteration_data_[iteration_].tests_by_status[result.status].push_back(
      result.full_name);
}

void TestResultsTracker::PrintSummaryOfCurrentIteration() const {
  PrintTestsByStatus(TestResult::TEST_FAILURE, "failed");
  PrintTestsByStatus(TestResult::TEST_TIMEOUT, "timed out");
  PrintTestsByStatus(TestResult::TEST_CRASH, "crashed");
  PrintTestsByStatus(TestResult::TEST_SKIPPED, "skipped");
  PrintTestsByStatus(TestResult::TEST_UNKNOWN, "had unknown result");
}

void TestResultsTracker::PrintSummaryOfAllIterations() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::map<TestResult::Status, std::set<std::string> > all_tests_by_status;

  for (int i = 0; i <= iteration_; i++) {
    for (PerIterationData::StatusMap::const_iterator j =
             per_iteration_data_[i].tests_by_status.begin();
         j != per_iteration_data_[i].tests_by_status.end();
         ++j) {
      for (size_t k = 0; k < j->second.size(); k++)
        all_tests_by_status[j->first].insert(j->second[k]);
    }
  }

  fprintf(stdout, "Summary of all itest iterations:\n");
  fflush(stdout);

  PrintTests(all_tests_by_status[TestResult::TEST_FAILURE].begin(),
             all_tests_by_status[TestResult::TEST_FAILURE].end(),
             "failed");
  PrintTests(all_tests_by_status[TestResult::TEST_TIMEOUT].begin(),
             all_tests_by_status[TestResult::TEST_TIMEOUT].end(),
             "timed out");
  PrintTests(all_tests_by_status[TestResult::TEST_CRASH].begin(),
             all_tests_by_status[TestResult::TEST_CRASH].end(),
             "crashed");
  PrintTests(all_tests_by_status[TestResult::TEST_SKIPPED].begin(),
             all_tests_by_status[TestResult::TEST_SKIPPED].end(),
             "skipped");
  PrintTests(all_tests_by_status[TestResult::TEST_UNKNOWN].begin(),
             all_tests_by_status[TestResult::TEST_UNKNOWN].end(),
             "had unknown result");

  fprintf(stdout, "End of the summary.\n");
  fflush(stdout);
}

bool TestResultsTracker::SaveSummaryAsJSON(const FilePath& path) const {
  scoped_ptr<DictionaryValue> summary_root(new DictionaryValue);

  ListValue* per_iteration_data = new ListValue;
  summary_root->Set("per_iteration_data", per_iteration_data);

  for (int i = 0; i <= iteration_; i++) {
    ListValue* current_iteration_data = new ListValue;
    per_iteration_data->Append(current_iteration_data);

    for (PerIterationData::ResultsMap::const_iterator j =
             per_iteration_data_[i].results.begin();
         j != per_iteration_data_[i].results.end();
         ++j) {
      for (size_t k = 0; k < j->second.size(); k++) {
        const TestResult& test_result = j->second[k];

        DictionaryValue* test_result_value = new DictionaryValue;
        current_iteration_data->Append(test_result_value);

        test_result_value->SetString("full_name", test_result.full_name);
        test_result_value->SetString("status", test_result.StatusAsString());
        test_result_value->SetInteger(
            "elapsed_time_ms", test_result.elapsed_time.InMilliseconds());
        test_result_value->SetString("output_snippet",
                                     test_result.output_snippet);
      }
    }
  }

  JSONFileValueSerializer serializer(path);
  return serializer.Serialize(*summary_root);
}

TestResultsTracker::PerIterationData::PerIterationData() {
}

TestResultsTracker::PerIterationData::~PerIterationData() {
}

void TestResultsTracker::PrintTestsByStatus(
    TestResult::Status status,
    const std::string& description) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  PerIterationData::StatusMap::const_iterator i(
      per_iteration_data_[iteration_].tests_by_status.find(status));
  if (i == per_iteration_data_[iteration_].tests_by_status.end())
    return;
  PrintTests(i->second.begin(), i->second.end(), description);
}

}  // namespace base
