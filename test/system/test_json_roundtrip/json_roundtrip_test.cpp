/**
 * @file json_roundtrip_test.cpp
 * @brief to_json/from_json round-trip tests for configurable objects (#920).
 *
 * Confirms that serializing a transform's configuration and restoring it into
 * a fresh instance preserves observable behavior.
 */

#include <Arduino.h>

#include "sensesp/system/lambda_consumer.h"
#include "sensesp/transforms/linear.h"
#include "sensesp/transforms/moving_average.h"
#include "unity.h"

using namespace sensesp;

// ---------------------------------------------------------------------------
// Linear (LambdaTransform-based, two float params)
// ---------------------------------------------------------------------------

void test_linear_round_trip_preserves_behavior() {
  Linear original(2.0f, 5.0f);

  JsonDocument doc;
  JsonObject obj = doc.to<JsonObject>();
  TEST_ASSERT_TRUE(original.to_json(obj));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 2.0f, obj["multiplier"].as<float>());
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 5.0f, obj["offset"].as<float>());

  Linear restored(1.0f, 0.0f);
  JsonObject in = doc.as<JsonObject>();
  TEST_ASSERT_TRUE(restored.from_json(in));

  float received = -1.0f;
  LambdaConsumer<float> consumer([&received](float v) { received = v; });
  restored.connect_to(&consumer);

  restored.set(3.0f);  // 3*2 + 5 = 11
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 11.0f, received);
}

// ---------------------------------------------------------------------------
// MovingAverage
// ---------------------------------------------------------------------------

void test_moving_average_round_trip() {
  MovingAverage original(8, 1.5f);

  JsonDocument doc;
  JsonObject obj = doc.to<JsonObject>();
  TEST_ASSERT_TRUE(original.to_json(obj));
  TEST_ASSERT_EQUAL(8, obj["sample_size"].as<int>());
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.5f, obj["multiplier"].as<float>());

  MovingAverage restored(2, 1.0f);
  JsonObject in = doc.as<JsonObject>();
  TEST_ASSERT_TRUE(restored.from_json(in));

  JsonDocument doc2;
  JsonObject obj2 = doc2.to<JsonObject>();
  TEST_ASSERT_TRUE(restored.to_json(obj2));
  TEST_ASSERT_EQUAL(8, obj2["sample_size"].as<int>());
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.5f, obj2["multiplier"].as<float>());
}

// ---------------------------------------------------------------------------
// Test runner
// ---------------------------------------------------------------------------
// ThresholdTransform round-trip lives in test_threshold_roundtrip, alongside
// the from_json fix it exercises.

void setup() {
  delay(2000);

  UNITY_BEGIN();

  RUN_TEST(test_linear_round_trip_preserves_behavior);
  RUN_TEST(test_moving_average_round_trip);

  UNITY_END();
}

void loop() {}
