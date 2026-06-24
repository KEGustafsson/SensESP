/**
 * @file nullable_test.cpp
 * @brief Edge-case tests for the Nullable type specializations (#920).
 *
 * Exercises validity detection, the invalid sentinel value, implicit
 * conversions, and JSON conversion across several type specializations.
 *
 * Note on semantics: a Nullable holds a plain value plus a per-type invalid
 * sentinel (e.g. float -> -1e9, uint8_t -> 0xff). A value is "invalid" only
 * when it equals that sentinel. A default-constructed Nullable holds T{} (0),
 * which is therefore VALID for types whose sentinel differs from 0 (int,
 * float, uint8_t). Nullable<bool> is a flag-based specialization: true, false,
 * and invalid are all distinct (see test/native/test_nullable_bool and #883).
 */

#include <Arduino.h>

#include "sensesp/transforms/repeat.h"
#include "sensesp/types/nullable.h"
#include "unity.h"

using namespace sensesp;

// ---------------------------------------------------------------------------
// The invalid sentinel reads as invalid; ordinary values read as valid
// ---------------------------------------------------------------------------

void test_nullable_invalid_sentinel_is_not_valid() {
  NullableInt i = NullableInt::invalid();
  TEST_ASSERT_FALSE(i.is_valid());

  NullableFloat f = NullableFloat::invalid();
  TEST_ASSERT_FALSE(f.is_valid());

  Nullable<uint8_t> u = Nullable<uint8_t>::invalid();
  TEST_ASSERT_FALSE(u.is_valid());
}

void test_nullable_value_is_valid() {
  NullableInt i = 42;
  TEST_ASSERT_TRUE(i.is_valid());
  TEST_ASSERT_EQUAL(42, i.value());
  TEST_ASSERT_EQUAL(42, static_cast<int>(i));  // implicit conversion

  NullableFloat f = 3.14f;
  TEST_ASSERT_TRUE(f.is_valid());
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 3.14f, f.value());

  Nullable<uint8_t> u = 5;
  TEST_ASSERT_TRUE(u.is_valid());
  TEST_ASSERT_EQUAL_UINT8(5, u.value());
}

void test_nullable_default_float_is_valid_zero() {
  // A default-constructed value is T{} (0). For float the sentinel is -1e9,
  // so the default is a valid zero -- it is NOT the invalid value.
  NullableFloat n;
  TEST_ASSERT_TRUE(n.is_valid());
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, n.value());
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
  NullableFloat n = NullableFloat::invalid();
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
// Nullable<bool>: flag-based, so false is valid and distinct from invalid
// ---------------------------------------------------------------------------

void test_nullable_bool_tristate() {
  NullableBool t = true;
  NullableBool f = false;
  NullableBool inv = NullableBool::invalid();
  TEST_ASSERT_TRUE(t.is_valid());
  TEST_ASSERT_TRUE(f.is_valid());
  TEST_ASSERT_FALSE(inv.is_valid());
  TEST_ASSERT_FALSE(f.value());
}

// RepeatExpiring<bool> is the #883 use case: its output type is Nullable<bool>
// and repeat_function() emits this->get().invalid() on expiry. Instantiating it
// compiles that emit path for bool; here we also check the valid path.
void test_repeat_expiring_bool() {
  RepeatExpiring<bool> repeat(1000, 5000);
  repeat.set(false);
  TEST_ASSERT_TRUE(repeat.get().is_valid());
  TEST_ASSERT_FALSE(repeat.get().value());
}

// ---------------------------------------------------------------------------
// Test runner
// ---------------------------------------------------------------------------

void setup() {
  delay(2000);

  UNITY_BEGIN();

  RUN_TEST(test_nullable_invalid_sentinel_is_not_valid);
  RUN_TEST(test_nullable_value_is_valid);
  RUN_TEST(test_nullable_default_float_is_valid_zero);
  RUN_TEST(test_nullable_ptr_mutates_value);
  RUN_TEST(test_nullable_copy_assignment);
  RUN_TEST(test_nullable_to_json_invalid_is_null);
  RUN_TEST(test_nullable_to_json_valid_serializes_value);
  RUN_TEST(test_nullable_from_json_null_is_invalid);
  RUN_TEST(test_nullable_from_json_value_is_valid);
  RUN_TEST(test_nullable_bool_tristate);
  RUN_TEST(test_repeat_expiring_bool);

  UNITY_END();
}

void loop() {}
