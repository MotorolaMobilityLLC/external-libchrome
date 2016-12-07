// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_SCHEMA_H_
#define COMPONENTS_POLICY_CORE_COMMON_SCHEMA_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "components/policy/policy_export.h"

namespace policy {
namespace internal {

struct POLICY_EXPORT SchemaData;
struct POLICY_EXPORT SchemaNode;
struct POLICY_EXPORT PropertyNode;
struct POLICY_EXPORT PropertiesNode;

}  // namespace internal

// Option flags passed to Schema::Validate() and Schema::Normalize(), describing
// the strategy to handle unknown properties or invalid values for dict type.
// Note that in Schema::Normalize() allowed errors will be dropped and thus
// ignored.
// Unknown error indicates that some value in a dictionary (may or may not be
// the one in root) have unknown property name according to schema.
// Invalid error indicates a validation failure against the schema. As
// validation is done recursively, a validation failure of dict properties or
// list items might be ignored (or dropped in Normalize()) or trigger whole
// dictionary/list validation failure.
enum SchemaOnErrorStrategy {
  // No errors will be allowed.
  SCHEMA_STRICT = 0,
  // Unknown properties in the top-level dictionary will be ignored.
  SCHEMA_ALLOW_UNKNOWN_TOPLEVEL,
  // Unknown properties in any dictionary will be ignored.
  SCHEMA_ALLOW_UNKNOWN,
  // Mismatched values will be ignored at the toplevel.
  SCHEMA_ALLOW_INVALID_TOPLEVEL,
  // Mismatched values will be ignored at the top-level value.
  // Unknown properties in any dictionary will be ignored.
  SCHEMA_ALLOW_INVALID_TOPLEVEL_AND_ALLOW_UNKNOWN,
  // Mismatched values will be ignored.
  SCHEMA_ALLOW_INVALID,
};

class Schema;

typedef std::vector<Schema> SchemaList;

// Describes the expected type of one policy. Also recursively describes the
// types of inner elements, for structured types.
// Objects of this class refer to external, immutable data and are cheap to
// copy.
class POLICY_EXPORT Schema {
 public:
  // Used internally to store shared data.
  class InternalStorage;

  // Builds an empty, invalid schema.
  Schema();

  // Makes a copy of |schema| that shares the same internal storage.
  Schema(const Schema& schema);

  ~Schema();

  Schema& operator=(const Schema& schema);

  // Returns a Schema that references static data. This can be used by
  // the embedder to pass structures generated at compile time, which can then
  // be quickly loaded at runtime.
  static Schema Wrap(const internal::SchemaData* data);

  // Parses the JSON schema in |schema| and returns a Schema that owns
  // the internal representation. If |schema| is invalid then an invalid Schema
  // is returned and |error| contains a reason for the failure.
  static Schema Parse(const std::string& schema, std::string* error);

  // Returns true if this Schema is valid. Schemas returned by the methods below
  // may be invalid, and in those cases the other methods must not be used.
  bool valid() const { return node_ != NULL; }

  base::Value::Type type() const;

  // Validate |value| against current schema, |strategy| is the strategy to
  // handle unknown properties or invalid values. Allowed errors will be
  // ignored. |error_path| and |error| will contain the last error location and
  // detailed message if |value| doesn't strictly conform to the schema. If
  // |value| doesn't conform to the schema even within the allowance of
  // |strategy|, false will be returned and |error_path| and |error| will
  // contain the corresponding error that caused the failure. |error_path| can
  // be NULL and in that case no error path will be returned.
  bool Validate(const base::Value& value,
                SchemaOnErrorStrategy strategy,
                std::string* error_path,
                std::string* error) const;

  // Similar to Validate() but drop values with errors instead of ignoring them.
  // |changed| is a pointer to a boolean value, and indicate whether |value|
  // is changed or not (probably dropped properties or items). Be sure to set
  // the bool that |changed| pointed to to false before calling Normalize().
  // |changed| can be NULL and in that case no boolean will be set.
  // This function will also take the ownership of dropped base::Value and
  // destroy them.
  bool Normalize(base::Value* value,
                 SchemaOnErrorStrategy strategy,
                 std::string* error_path,
                 std::string* error,
                 bool* changed) const;

  // Used to iterate over the known properties of Type::DICTIONARY schemas.
  class POLICY_EXPORT Iterator {
   public:
    Iterator(const scoped_refptr<const InternalStorage>& storage,
             const internal::PropertiesNode* node);
    Iterator(const Iterator& iterator);
    ~Iterator();

    Iterator& operator=(const Iterator& iterator);

    // The other methods must not be called if the iterator is at the end.
    bool IsAtEnd() const;

    // Advances the iterator to the next property.
    void Advance();

    // Returns the name of the current property.
    const char* key() const;

    // Returns the Schema for the current property. This Schema is always valid.
    Schema schema() const;

   private:
    scoped_refptr<const InternalStorage> storage_;
    const internal::PropertyNode* it_;
    const internal::PropertyNode* end_;
  };

  // These methods should be called only if type() == Type::DICTIONARY,
  // otherwise invalid memory will be read. A CHECK is currently enforcing this.

  // Returns an iterator that goes over the named properties of this schema.
  // The returned iterator is at the beginning.
  Iterator GetPropertiesIterator() const;

  // Returns the Schema for the property named |key|. If |key| is not a known
  // property name then the returned Schema is not valid.
  Schema GetKnownProperty(const std::string& key) const;

  // Returns all Schemas from pattern properties that match |key|. May be empty.
  SchemaList GetPatternProperties(const std::string& key) const;

  // Returns the Schema for additional properties. If additional properties are
  // not allowed for this Schema then the Schema returned is not valid.
  Schema GetAdditionalProperties() const;

  // Returns the Schema for |key| if it is a known property, otherwise returns
  // the Schema for additional properties.
  // DEPRECATED: This function didn't consider patternProperties, use
  // GetMatchingProperties() instead.
  // TODO(binjin): Replace calls to this function with GetKnownProperty() or
  // GetMatchingProperties() and remove this later.
  Schema GetProperty(const std::string& key) const;

  // Returns all Schemas that are supposed to be validated against for |key|.
  // May be empty.
  SchemaList GetMatchingProperties(const std::string& key) const;

  // Returns the Schema for items of an array.
  // This method should be called only if type() == Type::LIST,
  // otherwise invalid memory will be read. A CHECK is currently enforcing this.
  Schema GetItems() const;

 private:
  // Builds a schema pointing to the inner structure of |storage|,
  // rooted at |node|.
  Schema(const scoped_refptr<const InternalStorage>& storage,
         const internal::SchemaNode* node);

  bool ValidateIntegerRestriction(int index, int value) const;
  bool ValidateStringRestriction(int index, const char* str) const;

  scoped_refptr<const InternalStorage> storage_;
  const internal::SchemaNode* node_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_SCHEMA_H_
