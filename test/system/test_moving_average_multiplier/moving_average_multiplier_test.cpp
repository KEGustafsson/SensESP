/**
 * @file moving_average_multiplier_test.cpp
 * @brief Regression tests for MovingAverage's multiplier (#1003).
 *
 * The documented contract is y = multiplier * (1/n) * sum(last n inputs):
 * the multiplier scales the averaged output, including the seeded first
 * sample and a steady input stream.
 */

#include <Arduino.h>

#include "sensesp/system/lambda_consumer.h"
#include "sensesp/transforms/moving_average.h"
#include "unity.h"

using namespace sensesp;

void test_multiplier_scales_seeded_first_sample() {
  MovingAverage ma(2, 2.0);  // output = 2.0 * mean

  float received = -1.0f;
  LambdaConsumer<float> consumer([&received](float v) { received = v; });
  ma.connect_to(&consumer);

  // First sample seeds the whole buffer (mean == input), scaled by 2.0.
  ma.set(10.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 20.0f, received);
}

void test_multiplier_applies_to_steady_input() {
  MovingAverage ma(4, 3.0);  // output = 3.0 * mean

  float received = -1.0f;
  LambdaConsumer<float> consumer([&received](float v) { received = v; });
  ma.connect_to(&consumer);

  // A constant input of 5.0 has mean 5.0; the multiplier must still apply.
  for (int i = 0; i < 6; i++) {
    ma.set(5.0f);
  }
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 15.0f, received);
}

void test_multiplier_tracks_windowed_mean() {
  MovingAverage ma(2, 2.0);

  float received = -1.0f;
  LambdaConsumer<float> consumer([&received](float v) { received = v; });
  ma.connect_to(&consumer);

  ma.set(10.0f);  // buffer [10,10], mean 10 -> 20
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 20.0f, received);

  ma.set(20.0f);  // buffer [20,10], mean 15 -> 30
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 30.0f, received);

  ma.set(30.0f);  // buffer [20,30], mean 25 -> 50
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 50.0f, received);
}

void test_default_multiplier_is_plain_average() {
  MovingAverage ma(4, 1.0);

  float received = -1.0f;
  LambdaConsumer<float> consumer([&received](float v) { received = v; });
  ma.connect_to(&consumer);

  // Buffer initializes to all 4.0; push three 8.0s: (8+8+8+4)/4 = 7.0.
  ma.set(4.0f);
  ma.set(8.0f);
  ma.set(8.0f);
  ma.set(8.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 7.0f, received);
}

void setup() {
  delay(2000);

  UNITY_BEGIN();

  RUN_TEST(test_multiplier_scales_seeded_first_sample);
  RUN_TEST(test_multiplier_applies_to_steady_input);
  RUN_TEST(test_multiplier_tracks_windowed_mean);
  RUN_TEST(test_default_multiplier_is_plain_average);

  UNITY_END();
}

void loop() {}
