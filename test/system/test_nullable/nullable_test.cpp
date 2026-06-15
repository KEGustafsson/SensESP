/**
 * @file nullable_test.cpp
 * @brief Edge-case tests for the Nullable type specializations (#920).
 *
 * Exercises validity detection, the invalid sentinel value, implicit
 * conversions, and JSON conversion across several type specializations.
 */

#include <Arduino.h>

#include "sensesp/types/nullable.h"
#include "unity.h"

using namespace sensesp;

// ---------------------------------------------------------------------------
// Default-constructed values are invalid; assigned values are valid
// ---------------------------------------------------------------------------

void test_nullable_int_default_is_invalid() {
  NullableInt n;
  TEST_ASSERT_FALSE(n.is_valid());
  TEST_ASSERT_EQUAL(NullableInt::invalid(), n.value());
}

void test_nullable_int_assigned_is_valid() {
  NullableInt n = 42;
  TEST_ASSERT_TRUE(n.is_valid());
  TEST_ASSERT_EQUAL(42, n.value());
  TEST_ASSERT_EQUAL(42, static_cast<int>(n));  // implicit conversion
}

void test_nullable_float_invalid_sentinel() {
  NullableFloat n;
  TEST_ASSERT_FALSE(n.is_valid());
  TEST_ASSERT_FLOAT_WITHIN(1.0f, -1e9f, n.value());

  NullableFloat valid = 3.14f;
  TEST_ASSERT_TRUE(valid.is_valid());
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 3.14f, valid.value());
}

void test_nullable_bool_invalid_is_false() {
  // bool's invalid sentinel is `false`, so a default Nullable<bool> is
  // invalid and a value of `false` is indistinguishable from invalid.
  NullableBool n;
  TEST_ASSERT_FALSE(n.is_valid());

  NullableBool t = true;
  TEST_ASSERT_TRUE(t.is_valid());
  TEST_ASSERT_TRUE(static_cast<bool>(t));

  NullableBool f = false;
  TEST_ASSERT_FALSE(f.is_valid());
}

void test_nullable_uint8_invalid_sentinel() {
  Nullable<uint8_t> n;
  TEST_ASSERT_FALSE(n.is_valid());
  TEST_ASSERT_EQUAL_UINT8(0xff, n.value());

  Nullable<uint8_t> valid = 0;
  TEST_ASSERT_TRUE(valid.is_valid());  // 0 is a valid uint8_t value
  TEST_ASSERT_EQUAL_UINT8(0, valid.value());
}

// ---------------------------------------------------------------------------
// ptr() exposes the underlying storage
// ---------------------------------------------------------------------------

void test_nullable_ptr_mutates_value() {
  NullableInt n = 1;
  *n.ptr() = 99;
  TEST_ASSERT_EQUAL(99, n.value());
  TEST_ASSERT_TRUE(n.is_valid());
}

// ---------------------------------------------------------------------------
// Copy assignment from another Nullable
// ---------------------------------------------------------------------------

void test_nullable_copy_assignment() {
  NullableInt src = 7;
  NullableInt dst;
  dst = src;
  TEST_ASSERT_TRUE(dst.is_valid());
  TEST_ASSERT_EQUAL(7, dst.value());
}

// ---------------------------------------------------------------------------
// JSON conversion: invalid serializes to null, valid round-trips
// ---------------------------------------------------------------------------

void test_nullable_to_json_invalid_is_null() {
  JsonDocument doc;
  NullableFloat n;  // invalid
  doc["v"] = n;
  TEST_ASSERT_TRUE(doc["v"].isNull());
}

void test_nullable_to_json_valid_serializes_value() {
  JsonDocument doc;
  NullableFloat n = 2.5f;
  doc["v"] = n;
  TEST_ASSERT_FALSE(doc["v"].isNull());
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 2.5f, doc["v"].as<float>());
}

void test_nullable_from_json_null_is_invalid() {
  JsonDocument doc;
  doc["v"] = nullptr;
  NullableFloat n = doc["v"].as<NullableFloat>();
  TEST_ASSERT_FALSE(n.is_valid());
}

void test_nullable_from_json_value_is_valid() {
  JsonDocument doc;
  doc["v"] = 12.0f;
  NullableFloat n = doc["v"].as<NullableFloat>();
  TEST_ASSERT_TRUE(n.is_valid());
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 12.0f, n.value());
}

// ---------------------------------------------------------------------------
// Test runner
// ---------------------------------------------------------------------------

void setup() {
  delay(2000);

  UNITY_BEGIN();

  RUN_TEST(test_nullable_int_default_is_invalid);
  RUN_TEST(test_nullable_int_assigned_is_valid);
  RUN_TEST(test_nullable_float_invalid_sentinel);
  RUN_TEST(test_nullable_bool_invalid_is_false);
  RUN_TEST(test_nullable_uint8_invalid_sentinel);
  RUN_TEST(test_nullable_ptr_mutates_value);
  RUN_TEST(test_nullable_copy_assignment);
  RUN_TEST(test_nullable_to_json_invalid_is_null);
  RUN_TEST(test_nullable_to_json_valid_serializes_value);
  RUN_TEST(test_nullable_from_json_null_is_invalid);
  RUN_TEST(test_nullable_from_json_value_is_valid);

  UNITY_END();
}

void loop() {}
