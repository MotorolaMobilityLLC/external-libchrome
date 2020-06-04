// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_JSON_JSON_VALUE_CONVERTER_H_
#define BASE_JSON_JSON_VALUE_CONVERTER_H_
#pragma once

#include <string>
#include <vector>

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/values.h"

// JSONValueConverter converts a JSON value into a C++ struct in a
// lightweight way.
//
// Usage:
// For real examples, you may want to refer to _unittest.cc file.
//
// Assume that you have a struct like this:
//   struct Message {
//     int foo;
//     std::string bar;
//     static void RegisterJSONConverter(
//         JSONValueConverter<Message>* converter);
//   };
//
// And you want to parse a json data into this struct.  First, you
// need to declare RegisterJSONConverter() method in your struct.
//   // static
//   void Message::RegisterJSONConverter(
//       JSONValueConverter<Message>* converter) {
//     converter->RegisterIntField("foo", &Message::foo);
//     converter->RegisterStringField("bar", &Message::bar);
//   }
//
// Then, you just instantiate your JSONValueConverter of your type and call
// Convert() method.
//   Message message;
//   JSONValueConverter<Message> converter;
//   converter.Convert(json, &message);
//
// For nested field, the internal message also has to implement the registration
// method.  Then, just use RegisterNestedField() from the containing struct's
// RegisterJSONConverter method.
//   struct Nested {
//     Message foo;
//     void RegisterJSONConverter(...) {
//       ...
//       converter->RegisterNestedField("foo", &Nested::foo);
//     }
//   };
//
// For repeated field, we just assume std::vector for its container
// and you can put RegisterRepeatedInt or some other types.  Use
// RegisterRepeatedMessage for nested repeated fields.
//

namespace base {

template <typename StructType>
class JSONValueConverter;

namespace internal {

class FieldConverterBase {
 public:
  BASE_EXPORT explicit FieldConverterBase(const std::string& path);
  BASE_EXPORT virtual ~FieldConverterBase();
  virtual void ConvertField(const base::Value& value, void* obj) const = 0;
  const std::string& field_path() const { return field_path_; }

 private:
  std::string field_path_;
  DISALLOW_COPY_AND_ASSIGN(FieldConverterBase);
};

template <typename FieldType>
class ValueConverter {
 public:
  virtual ~ValueConverter() {}
  virtual void Convert(const base::Value& value, FieldType* field) const = 0;
};

template <typename StructType, typename FieldType>
class FieldConverter : public FieldConverterBase {
 public:
  explicit FieldConverter(const std::string& path,
                          FieldType StructType::* field,
                          ValueConverter<FieldType>* converter)
      : FieldConverterBase(path),
        field_pointer_(field),
        value_converter_(converter) {
  }

  virtual void ConvertField(
      const base::Value& value, void* obj) const OVERRIDE {
    StructType* dst = reinterpret_cast<StructType*>(obj);
    value_converter_->Convert(value, &(dst->*field_pointer_));
  }

 private:
  FieldType StructType::* field_pointer_;
  scoped_ptr<ValueConverter<FieldType> > value_converter_;
  DISALLOW_COPY_AND_ASSIGN(FieldConverter);
};

template <typename FieldType>
class BasicValueConverter;

template <>
class BasicValueConverter<int> : public ValueConverter<int> {
 public:
  BasicValueConverter() {}

  virtual void Convert(const base::Value& value, int* field) const OVERRIDE {
    value.GetAsInteger(field);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BasicValueConverter);
};

template <>
class BasicValueConverter<std::string> : public ValueConverter<std::string> {
 public:
  BasicValueConverter() {}

  virtual void Convert(
      const base::Value& value, std::string* field) const OVERRIDE {
    value.GetAsString(field);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BasicValueConverter);
};

template <>
class BasicValueConverter<double> : public ValueConverter<double> {
 public:
  BasicValueConverter() {}

  virtual void Convert(const base::Value& value, double* field) const OVERRIDE {
    value.GetAsDouble(field);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BasicValueConverter);
};

template <>
class BasicValueConverter<bool> : public ValueConverter<bool> {
 public:
  BasicValueConverter() {}

  virtual void Convert(const base::Value& value, bool* field) const OVERRIDE {
    value.GetAsBoolean(field);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BasicValueConverter);
};

template <typename NestedType>
class NestedValueConverter : public ValueConverter<NestedType> {
 public:
  NestedValueConverter() {}

  virtual void Convert(
      const base::Value& value, NestedType* field) const OVERRIDE {
    converter_.Convert(value, field);
  }

 private:
  JSONValueConverter<NestedType> converter_;
  DISALLOW_COPY_AND_ASSIGN(NestedValueConverter);
};

template <typename Element>
class RepeatedValueConverter : public ValueConverter<std::vector<Element> > {
 public:
  RepeatedValueConverter() {}

  virtual void Convert(
      const base::Value& value, std::vector<Element>* field) const OVERRIDE {
    const base::ListValue* list = NULL;
    if (!value.GetAsList(&list)) {
      // The field is not a list.
      return;
    }

    field->reserve(list->GetSize());
    for (size_t i = 0; i < list->GetSize(); ++i) {
      base::Value* element = NULL;
      if (!list->Get(i, &element))
        continue;

      Element e;
      basic_converter_.Convert(*element, &e);
      field->push_back(e);
    }
  }

 private:
  BasicValueConverter<Element> basic_converter_;
  DISALLOW_COPY_AND_ASSIGN(RepeatedValueConverter);
};

template <typename NestedType>
class RepeatedMessageConverter
    : public ValueConverter<std::vector<NestedType> > {
 public:
  RepeatedMessageConverter() {}

  virtual void Convert(
      const base::Value& value, std::vector<NestedType>* field) const OVERRIDE {
    const base::ListValue* list = NULL;
    if (!value.GetAsList(&list))
      return;

    field->reserve(list->GetSize());
    for (size_t i = 0; i < list->GetSize(); ++i) {
      base::Value* element = NULL;
      if (!list->Get(i, &element))
        continue;

      field->push_back(NestedType());
      converter_.Convert(*element, &field->back());
    }
  }

 private:
  JSONValueConverter<NestedType> converter_;
  DISALLOW_COPY_AND_ASSIGN(RepeatedMessageConverter);
};

}  // namespace internal

template <class StructType>
class JSONValueConverter {
 public:
  JSONValueConverter() {
    StructType::RegisterJSONConverter(this);
  }

  ~JSONValueConverter() {
    STLDeleteContainerPointers(fields_.begin(), fields_.end());
  }

  void RegisterIntField(const std::string& field_name,
                        int StructType::* field) {
    fields_.push_back(new internal::FieldConverter<StructType, int>(
        field_name, field, new internal::BasicValueConverter<int>));
  }

  void RegisterStringField(const std::string& field_name,
                        std::string StructType::* field) {
    fields_.push_back(new internal::FieldConverter<StructType, std::string>(
        field_name, field, new internal::BasicValueConverter<std::string>));
  }

  void RegisterBoolField(const std::string& field_name,
                         bool StructType::* field) {
    fields_.push_back(new internal::FieldConverter<StructType, bool>(
        field_name, field, new internal::BasicValueConverter<bool>));
  }

  void RegisterDoubleField(const std::string& field_name,
                           double StructType::* field) {
    fields_.push_back(new internal::FieldConverter<StructType, double>(
        field_name, field, new internal::BasicValueConverter<double>));
  }

  template <class NestedType>
  void RegisterNestedField(
      const std::string& field_name, NestedType StructType::* field) {
    fields_.push_back(new internal::FieldConverter<StructType, NestedType>(
            field_name,
            field,
            new internal::NestedValueConverter<NestedType>));
  }

  void RegisterRepeatedInt(const std::string& field_name,
                           std::vector<int> StructType::* field) {
    fields_.push_back(
        new internal::FieldConverter<StructType, std::vector<int> >(
            field_name, field, new internal::RepeatedValueConverter<int>));
  }

  void RegisterRepeatedString(const std::string& field_name,
                              std::vector<std::string> StructType::* field) {
    fields_.push_back(
        new internal::FieldConverter<StructType, std::vector<std::string> >(
            field_name,
            field,
            new internal::RepeatedValueConverter<std::string>));
  }

  void RegisterRepeatedDouble(const std::string& field_name,
                              std::vector<double> StructType::* field) {
    fields_.push_back(
        new internal::FieldConverter<StructType, std::vector<double> >(
            field_name, field, new internal::RepeatedValueConverter<double>));
  }

  void RegisterRepeatedBool(const std::string& field_name,
                            std::vector<bool> StructType::* field) {
    fields_.push_back(
        new internal::FieldConverter<StructType, std::vector<bool> >(
            field_name, field, new internal::RepeatedValueConverter<bool>));
  }

  template <class NestedType>
  void RegisterRepeatedMessage(const std::string& field_name,
                               std::vector<NestedType> StructType::* field) {
    fields_.push_back(
        new internal::FieldConverter<StructType, std::vector<NestedType> >(
            field_name,
            field,
            new internal::RepeatedMessageConverter<NestedType>));
  }

  void Convert(const base::Value& value, StructType* output) const {
    const DictionaryValue* dictionary_value = NULL;
    if (!value.GetAsDictionary(&dictionary_value))
      return;

    for(std::vector<internal::FieldConverterBase*>::const_iterator it =
            fields_.begin(); it != fields_.end(); ++it) {
      base::Value* field = NULL;
      if (dictionary_value->Get((*it)->field_path(), &field)) {
        (*it)->ConvertField(*field, output);
      }
    }
  }

 private:
  std::vector<internal::FieldConverterBase*> fields_;

  DISALLOW_COPY_AND_ASSIGN(JSONValueConverter);
};

}  // namespace base

#endif  // BASE_JSON_JSON_VALUE_CONVERTER_H_
