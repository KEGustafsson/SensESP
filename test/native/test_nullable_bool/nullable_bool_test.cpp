/**
 * @file nullable_bool_test.cpp
 * @brief Host tests for the Nullable<bool> tri-state specialization (#883).
 *
 * Unlike the numeric specializations, Nullable<bool> cannot use a sentinel
 * value (bool has no spare bit pattern), so it carries an explicit validity
 * flag: true / false / invalid are all distinct, and a valid `false` is NOT
 * the invalid state.
 *
 *   pio test -e native -f native/test_nullable_bool
 */

#include <unity.h>

#include "sensesp/types/nullable.h"

using namespace sensesp;

// ---------------------------------------------------------------------------
// true / false / invalid / default are all distinct
// ---------------------------------------------------------------------------

void test_true_is_valid(void) {
  Nullable<bool> b = true;
  TEST_ASSERT_TRUE(b.is_valid());
  TEST_ASSERT_TRUE(static_cast<bool>(b));
  TEST_ASSERT_TRUE(b.value());
}

void test_false_is_valid(void) {
  // The bug #883 fixes: a valid `false` must be distinguishable from invalid.
  Nullable<bool> b = false;
  TEST_ASSERT_TRUE(b.is_valid());
  TEST_ASSERT_FALSE(static_cast<bool>(b));
  TEST_ASSERT_FALSE(b.value());
}

void test_invalid_is_not_valid(void) {
  Nullable<bool> b = Nullable<bool>::invalid();
  TEST_ASSERT_FALSE(b.is_valid());
}

void test_default_is_invalid(void) {
  // No value yet -> unknown, not a valid false.
  Nullable<bool> b;
  TEST_ASSERT_FALSE(b.is_valid());
}

// ---------------------------------------------------------------------------
// Copy assignment carries the validity flag, not just the value
// ---------------------------------------------------------------------------

void test_copy_assignment_preserves_validity(void) {
  Nullable<bool> src = false;
  Nullable<bool> dst;
  dst = src;
  TEST_ASSERT_TRUE(dst.is_valid());
  TEST_ASSERT_FALSE(dst.value());

  dst = Nullable<bool>::invalid();
  TEST_ASSERT_FALSE(dst.is_valid());
}

// ---------------------------------------------------------------------------
// JSON: valid false -> false (NOT null); invalid -> null; round-trips
// ---------------------------------------------------------------------------

void test_to_json_valid_false_is_false(void) {
  JsonDocument doc;
  doc["v"] = Nullable<bool>(false);
  TEST_ASSERT_FALSE(doc["v"].isNull());
  TEST_ASSERT_FALSE(doc["v"].as<bool>());
}

void test_to_json_invalid_is_null(void) {
  JsonDocument doc;
  doc["v"] = Nullable<bool>::invalid();
  TEST_ASSERT_TRUE(doc["v"].isNull());
}

void test_from_json_false_is_valid(void) {
  JsonDocument doc;
  doc["v"] = false;
  Nullable<bool> b = doc["v"].as<Nullable<bool>>();
  TEST_ASSERT_TRUE(b.is_valid());
  TEST_ASSERT_FALSE(b.value());
}

void test_from_json_null_is_invalid(void) {
  JsonDocument doc;
  doc["v"] = nullptr;
  Nullable<bool> b = doc["v"].as<Nullable<bool>>();
  TEST_ASSERT_FALSE(b.is_valid());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_true_is_valid);
  RUN_TEST(test_false_is_valid);
  RUN_TEST(test_invalid_is_not_valid);
  RUN_TEST(test_default_is_invalid);
  RUN_TEST(test_copy_assignment_preserves_validity);
  RUN_TEST(test_to_json_valid_false_is_false);
  RUN_TEST(test_to_json_invalid_is_null);
  RUN_TEST(test_from_json_false_is_valid);
  RUN_TEST(test_from_json_null_is_invalid);
  return UNITY_END();
}
