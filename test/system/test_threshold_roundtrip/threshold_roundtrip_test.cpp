/**
 * @file threshold_roundtrip_test.cpp
 * @brief Round-trip and validation tests for ThresholdTransform JSON config.
 *
 * Regression coverage for the from_json/to_json asymmetry: to_json writes
 * {min, max, in_range}, so from_json must require exactly those keys. It
 * previously also required "out_range", which to_json never emits, so a
 * saved threshold config could never be reloaded.
 */

#include <Arduino.h>

#include "sensesp/system/lambda_consumer.h"
#include "sensesp/transforms/threshold.h"
#include "unity.h"

using namespace sensesp;

void test_float_threshold_round_trip_preserves_behavior() {
  FloatThreshold original(10.0f, 20.0f, true);

  JsonDocument doc;
  JsonObject obj = doc.to<JsonObject>();
  TEST_ASSERT_TRUE(original.to_json(obj));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 10.0f, obj["min"].as<float>());
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 20.0f, obj["max"].as<float>());
  TEST_ASSERT_TRUE(obj["in_range"].as<bool>());

  FloatThreshold restored(0.0f, 0.0f, false);
  JsonObject in = doc.as<JsonObject>();
  TEST_ASSERT_TRUE(restored.from_json(in));

  bool received = false;
  LambdaConsumer<bool> consumer([&received](bool v) { received = v; });
  restored.connect_to(&consumer);

  restored.set(15.0f);  // in range -> in_range value (true)
  TEST_ASSERT_TRUE(received);

  restored.set(25.0f);  // out of range -> !in_range (false)
  TEST_ASSERT_FALSE(received);
}

void test_threshold_from_json_rejects_missing_required_key() {
  FloatThreshold threshold(0.0f, 0.0f, true);

  JsonDocument doc;
  doc["min"] = 1.0f;
  doc["max"] = 2.0f;
  // "in_range" intentionally missing
  JsonObject in = doc.as<JsonObject>();
  TEST_ASSERT_FALSE(threshold.from_json(in));
}

void setup() {
  delay(2000);

  UNITY_BEGIN();

  RUN_TEST(test_float_threshold_round_trip_preserves_behavior);
  RUN_TEST(test_threshold_from_json_rejects_missing_required_key);

  UNITY_END();
}

void loop() {}
