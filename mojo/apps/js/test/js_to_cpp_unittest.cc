// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "gin/public/isolate_holder.h"
#include "mojo/apps/js/mojo_runner_delegate.h"
#include "mojo/apps/js/test/js_to_cpp.mojom.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/common/test/test_utils.h"
#include "mojo/public/cpp/bindings/allocation_scope.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/cpp/system/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace js {

// Global value updated by some checks to prevent compilers from optimizing
// reads out of existence.
uint32 g_waste_accumulator = 0;

namespace {

// Negative numbers with different values in each byte, the last of
// which can survive promotion to double and back.
const int8  kExpectedInt8Value = -65;
const int16 kExpectedInt16Value = -16961;
const int32 kExpectedInt32Value = -1145258561;
const int64 kExpectedInt64Value = -77263311946305LL;

// Positive numbers with different values in each byte, the last of
// which can survive promotion to double and back.
const uint8  kExpectedUInt8Value = 65;
const uint16 kExpectedUInt16Value = 16961;
const uint32 kExpectedUInt32Value = 1145258561;
const uint64 kExpectedUInt64Value = 77263311946305LL;

// Double/float values, including special case constants.
const double kExpectedDoubleVal = 3.14159265358979323846;
const double kExpectedDoubleInf = std::numeric_limits<double>::infinity();
const double kExpectedDoubleNan = std::numeric_limits<double>::quiet_NaN();
const float kExpectedFloatVal = static_cast<float>(kExpectedDoubleVal);
const float kExpectedFloatInf = std::numeric_limits<float>::infinity();
const float kExpectedFloatNan = std::numeric_limits<float>::quiet_NaN();

// NaN has the property that it is not equal to itself.
#define EXPECT_NAN(x) EXPECT_NE(x, x)

bool IsRunningOnIsolatedBot() {
  // TODO(yzshen): Remove this check once isolated tests are supported on the
  // Chromium waterfall. (http://crbug.com/351214)
  const base::FilePath test_file_path(
      test::GetFilePathForJSResource(
          "mojo/public/interfaces/bindings/tests/sample_interfaces.mojom"));
  if (!base::PathExists(test_file_path)) {
    LOG(WARNING) << "Mojom binding files don't exist. Skipping the test.";
    return true;
  }
  return false;
}

void CheckDataPipe(MojoHandle data_pipe_handle) {
  unsigned char buffer[100];
  uint32_t buffer_size = static_cast<uint32_t>(sizeof(buffer));
  MojoResult result = MojoReadData(
      data_pipe_handle, buffer, &buffer_size, MOJO_READ_DATA_FLAG_NONE);
  EXPECT_EQ(MOJO_RESULT_OK, result);
  EXPECT_EQ(64u, buffer_size);
  for (int i = 0; i < 64; ++i) {
    EXPECT_EQ(i, buffer[i]);
  }
}

void CheckMessagePipe(MojoHandle message_pipe_handle) {
  unsigned char buffer[100];
  uint32_t buffer_size = static_cast<uint32_t>(sizeof(buffer));
  MojoResult result = MojoReadMessage(
      message_pipe_handle, buffer, &buffer_size, 0, 0, 0);
  EXPECT_EQ(MOJO_RESULT_OK, result);
  EXPECT_EQ(64u, buffer_size);
  for (int i = 0; i < 64; ++i) {
    EXPECT_EQ(255 - i, buffer[i]);
  }
}

// NOTE: Callers will need to have established an AllocationScope, or you're
// gonna have a bad time.
js_to_cpp::EchoArgs BuildSampleEchoArgs() {
  js_to_cpp::EchoArgs::Builder builder;
  builder.set_si64(kExpectedInt64Value);
  builder.set_si32(kExpectedInt32Value);
  builder.set_si16(kExpectedInt16Value);
  builder.set_si8(kExpectedInt8Value);
  builder.set_ui64(kExpectedUInt64Value);
  builder.set_ui32(kExpectedUInt32Value);
  builder.set_ui16(kExpectedUInt16Value);
  builder.set_ui8(kExpectedUInt8Value);
  builder.set_float_val(kExpectedFloatVal);
  builder.set_float_inf(kExpectedFloatInf);
  builder.set_float_nan(kExpectedFloatNan);
  builder.set_double_val(kExpectedDoubleVal);
  builder.set_double_inf(kExpectedDoubleInf);
  builder.set_double_nan(kExpectedDoubleNan);
  builder.set_name("coming");
  mojo::Array<mojo::String>::Builder string_array(3);
  string_array[0] = "one";
  string_array[1] = "two";
  string_array[2] = "three";
  builder.set_string_array(string_array.Finish());
  return builder.Finish();
}

void CheckSampleEchoArgs(const js_to_cpp::EchoArgs& arg) {
  EXPECT_EQ(kExpectedInt64Value, arg.si64());
  EXPECT_EQ(kExpectedInt32Value, arg.si32());
  EXPECT_EQ(kExpectedInt16Value, arg.si16());
  EXPECT_EQ(kExpectedInt8Value, arg.si8());
  EXPECT_EQ(kExpectedUInt64Value, arg.ui64());
  EXPECT_EQ(kExpectedUInt32Value, arg.ui32());
  EXPECT_EQ(kExpectedUInt16Value, arg.ui16());
  EXPECT_EQ(kExpectedUInt8Value, arg.ui8());
  EXPECT_EQ(kExpectedFloatVal, arg.float_val());
  EXPECT_EQ(kExpectedFloatInf, arg.float_inf());
  EXPECT_NAN(arg.float_nan());
  EXPECT_EQ(kExpectedDoubleVal, arg.double_val());
  EXPECT_EQ(kExpectedDoubleInf, arg.double_inf());
  EXPECT_NAN(arg.double_nan());
  EXPECT_EQ(std::string("coming"), arg.name().To<std::string>());
  EXPECT_EQ(std::string("one"), arg.string_array()[0].To<std::string>());
  EXPECT_EQ(std::string("two"), arg.string_array()[1].To<std::string>());
  EXPECT_EQ(std::string("three"), arg.string_array()[2].To<std::string>());
  CheckDataPipe(arg.data_handle().get().value());
  CheckMessagePipe(arg.message_handle().get().value());
}

void CheckSampleEchoArgsList(const js_to_cpp::EchoArgsList& list) {
  if (list.is_null())
    return;
  CheckSampleEchoArgs(list.item());
  CheckSampleEchoArgsList(list.next());
}

// More forgiving checks are needed in the face of potentially corrupt
// messages. The values don't matter so long as all accesses are within
// bounds.
void CheckCorruptedString(const mojo::String& arg) {
  if (arg.is_null())
    return;
  for (size_t i = 0; i < arg.size(); ++i)
    g_waste_accumulator += arg[i];
}

void CheckCorruptedStringArray(const mojo::Array<mojo::String>& string_array) {
  if (string_array.is_null())
    return;
  for (size_t i = 0; i < string_array.size(); ++i)
    CheckCorruptedString(string_array[i]);
}

void CheckCorruptedDataPipe(MojoHandle data_pipe_handle) {
  unsigned char buffer[100];
  uint32_t buffer_size = static_cast<uint32_t>(sizeof(buffer));
  MojoResult result = MojoReadData(
      data_pipe_handle, buffer, &buffer_size, MOJO_READ_DATA_FLAG_NONE);
  if (result != MOJO_RESULT_OK)
    return;
  for (uint32_t i = 0; i < buffer_size; ++i)
    g_waste_accumulator += buffer[i];
}

void CheckCorruptedMessagePipe(MojoHandle message_pipe_handle) {
  unsigned char buffer[100];
  uint32_t buffer_size = static_cast<uint32_t>(sizeof(buffer));
  MojoResult result = MojoReadMessage(
      message_pipe_handle, buffer, &buffer_size, 0, 0, 0);
  if (result != MOJO_RESULT_OK)
    return;
  for (uint32_t i = 0; i < buffer_size; ++i)
    g_waste_accumulator += buffer[i];
}

void CheckCorruptedEchoArgs(const js_to_cpp::EchoArgs& arg) {
  if (arg.is_null())
    return;
  CheckCorruptedString(arg.name());
  CheckCorruptedStringArray(arg.string_array());
  if (arg.data_handle().is_valid())
    CheckCorruptedDataPipe(arg.data_handle().get().value());
  if (arg.message_handle().is_valid())
    CheckCorruptedMessagePipe(arg.message_handle().get().value());
}

void CheckCorruptedEchoArgsList(const js_to_cpp::EchoArgsList& list) {
  if (list.is_null())
    return;
  CheckCorruptedEchoArgs(list.item());
  CheckCorruptedEchoArgsList(list.next());
}

// Base Provider implementation class. It's expected that tests subclass and
// override the appropriate Provider functions. When test is done quit the
// run_loop().
class CppSideConnection : public js_to_cpp::CppSide {
 public:
  CppSideConnection() :
      run_loop_(NULL),
      js_side_(NULL),
      mishandled_messages_(0) {
  }
  virtual ~CppSideConnection() {}

  void set_run_loop(base::RunLoop* run_loop) { run_loop_ = run_loop; }
  base::RunLoop* run_loop() { return run_loop_; }

  void set_js_side(js_to_cpp::JsSide* js_side) { js_side_ = js_side; }
  js_to_cpp::JsSide* js_side() { return js_side_; }

  // js_to_cpp::CppSide:
  virtual void StartTest() OVERRIDE {
    NOTREACHED();
  }

  virtual void TestFinished() OVERRIDE {
    NOTREACHED();
  }

  virtual void PingResponse() OVERRIDE {
    mishandled_messages_ += 1;
  }

  virtual void EchoResponse(const js_to_cpp::EchoArgsList& list) OVERRIDE {
    mishandled_messages_ += 1;
  }

  virtual void BitFlipResponse(const js_to_cpp::EchoArgsList& list) OVERRIDE {
    mishandled_messages_ += 1;
  }

  virtual void BackPointerResponse(
      const js_to_cpp::EchoArgsList& list) OVERRIDE {
    mishandled_messages_ += 1;
  }

 protected:
  base::RunLoop* run_loop_;
  js_to_cpp::JsSide* js_side_;
  int mishandled_messages_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CppSideConnection);
};

// Trivial test to verify a message sent from JS is received.
class PingCppSideConnection : public CppSideConnection {
 public:
  PingCppSideConnection() : got_message_(false) {}
  virtual ~PingCppSideConnection() {}

  // js_to_cpp::CppSide:
  virtual void StartTest() OVERRIDE {
    js_side_->Ping();
  }

  virtual void PingResponse() OVERRIDE {
    got_message_ = true;
    run_loop()->Quit();
  }

  bool DidSucceed() {
    return got_message_ && !mishandled_messages_;
  }

 private:
  bool got_message_;
  DISALLOW_COPY_AND_ASSIGN(PingCppSideConnection);
};

// Test that parameters are passed with correct values.
class EchoCppSideConnection : public CppSideConnection {
 public:
  EchoCppSideConnection() :
      message_count_(0),
      termination_seen_(false) {
  }
  virtual ~EchoCppSideConnection() {}

  // js_to_cpp::CppSide:
  virtual void StartTest() OVERRIDE {
    AllocationScope scope;
    js_side_->Echo(kExpectedMessageCount, BuildSampleEchoArgs());
  }

  virtual void EchoResponse(const js_to_cpp::EchoArgsList& list) OVERRIDE {
    const js_to_cpp::EchoArgs& special_arg = list.item();
    message_count_ += 1;
    EXPECT_EQ(-1, special_arg.si64());
    EXPECT_EQ(-1, special_arg.si32());
    EXPECT_EQ(-1, special_arg.si16());
    EXPECT_EQ(-1, special_arg.si8());
    EXPECT_EQ(std::string("going"), special_arg.name().To<std::string>());
    CheckSampleEchoArgsList(list.next());
  }

  virtual void TestFinished() OVERRIDE {
    termination_seen_ = true;
    run_loop()->Quit();
  }

  bool DidSucceed() {
    return termination_seen_ &&
        !mishandled_messages_ &&
        message_count_ == kExpectedMessageCount;
  }

 private:
  static const int kExpectedMessageCount = 10;
  int message_count_;
  bool termination_seen_;
  DISALLOW_COPY_AND_ASSIGN(EchoCppSideConnection);
};

// Test that corrupted messages don't wreak havoc.
class BitFlipCppSideConnection : public CppSideConnection {
 public:
  BitFlipCppSideConnection() : termination_seen_(false) {}
  virtual ~BitFlipCppSideConnection() {}

  // js_to_cpp::CppSide:
  virtual void StartTest() OVERRIDE {
    AllocationScope scope;
    js_side_->BitFlip(BuildSampleEchoArgs());
  }

  virtual void BitFlipResponse(const js_to_cpp::EchoArgsList& list) OVERRIDE {
    CheckCorruptedEchoArgsList(list);
  }

  virtual void TestFinished() OVERRIDE {
    termination_seen_ = true;
    run_loop()->Quit();
  }

  bool DidSucceed() {
    return termination_seen_;
  }

 private:
  bool termination_seen_;
  DISALLOW_COPY_AND_ASSIGN(BitFlipCppSideConnection);
};

// Test that severely random messages don't wreak havoc.
class BackPointerCppSideConnection : public CppSideConnection {
 public:
  BackPointerCppSideConnection() : termination_seen_(false) {}
  virtual ~BackPointerCppSideConnection() {}

  // js_to_cpp::CppSide:
  virtual void StartTest() OVERRIDE {
    AllocationScope scope;
    js_side_->BackPointer(BuildSampleEchoArgs());
  }

  virtual void BackPointerResponse(
      const js_to_cpp::EchoArgsList& list) OVERRIDE {
    CheckCorruptedEchoArgsList(list);
  }

  virtual void TestFinished() OVERRIDE {
    termination_seen_ = true;
    run_loop()->Quit();
  }

  bool DidSucceed() {
    return termination_seen_;
  }

 private:
  bool termination_seen_;
  DISALLOW_COPY_AND_ASSIGN(BackPointerCppSideConnection);
};

}  // namespace

class JsToCppTest : public testing::Test {
 public:
  JsToCppTest() {}

  void RunTest(const std::string& test, CppSideConnection* cpp_side) {
    cpp_side->set_run_loop(&run_loop_);

    MessagePipe pipe;
    js_to_cpp::JsSidePtr js_side =
        MakeProxy<js_to_cpp::JsSide>(pipe.handle0.Pass());
    js_side.set_client(cpp_side);

    js_side.internal_state()->router()->
        set_enforce_errors_from_incoming_receiver(false);

    cpp_side->set_js_side(js_side.get());

    gin::IsolateHolder instance(gin::IsolateHolder::kStrictMode);
    apps::MojoRunnerDelegate delegate;
    gin::ShellRunner runner(&delegate, instance.isolate());
    delegate.Start(&runner, pipe.handle1.release().value(), test);

    run_loop_.Run();
  }

 private:
  Environment environment;
  base::MessageLoop loop;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(JsToCppTest);
};

TEST_F(JsToCppTest, Ping) {
  if (IsRunningOnIsolatedBot())
    return;

  PingCppSideConnection cpp_side_connection;
  RunTest("mojo/apps/js/test/js_to_cpp_unittest", &cpp_side_connection);
  EXPECT_TRUE(cpp_side_connection.DidSucceed());
}

TEST_F(JsToCppTest, Echo) {
  if (IsRunningOnIsolatedBot())
    return;

  EchoCppSideConnection cpp_side_connection;
  RunTest("mojo/apps/js/test/js_to_cpp_unittest", &cpp_side_connection);
  EXPECT_TRUE(cpp_side_connection.DidSucceed());
}

// TODO(tsepez): Disabled due to http://crbug.com/366797.
TEST_F(JsToCppTest, DISABLED_BitFlip) {
  if (IsRunningOnIsolatedBot())
    return;

  BitFlipCppSideConnection cpp_side_connection;
  RunTest("mojo/apps/js/test/js_to_cpp_unittest", &cpp_side_connection);
  EXPECT_TRUE(cpp_side_connection.DidSucceed());
}

// TODO(tsepez): Disabled due to http://crbug.com/366797.
TEST_F(JsToCppTest, DISABLED_BackPointer) {
  if (IsRunningOnIsolatedBot())
    return;

  BackPointerCppSideConnection cpp_side_connection;
  RunTest("mojo/apps/js/test/js_to_cpp_unittest", &cpp_side_connection);
  EXPECT_TRUE(cpp_side_connection.DidSucceed());
}

}  // namespace js
}  // namespace mojo
