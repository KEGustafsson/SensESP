/**
 * @file transforms_test.cpp
 * @brief Behavioral unit tests for core transforms (#920).
 *
 * Covers representative behavior and edge cases for CurveInterpolator,
 * MovingAverage, LambdaTransform, Hysteresis, and Frequency.
 */

#include <Arduino.h>

#include <set>

#include "sensesp/system/lambda_consumer.h"
#include "sensesp/transforms/curveinterpolator.h"
#include "sensesp/transforms/frequency.h"
#include "sensesp/transforms/hysteresis.h"
#include "sensesp/transforms/lambda_transform.h"
#include "sensesp/transforms/moving_average.h"
#include "unity.h"

using namespace sensesp;

// ---------------------------------------------------------------------------
// CurveInterpolator
// ---------------------------------------------------------------------------

static CurveInterpolator make_ramp_interpolator() {
  // Samples: (0,0) (1,0) (2,1) (3,1) as documented in curveinterpolator.h
  std::set<CurveInterpolator::Sample> samples = {
      {0.0f, 0.0f}, {1.0f, 0.0f}, {2.0f, 1.0f}, {3.0f, 1.0f}};
  return CurveInterpolator(&samples);
}

void test_curve_interpolator_exact_sample_points() {
  CurveInterpolator curve = make_ramp_interpolator();

  float received = -1.0f;
  LambdaConsumer<float> consumer([&received](float v) { received = v; });
  curve.connect_to(&consumer);

  curve.set(0.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, received);

  curve.set(2.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, received);
}

void test_curve_interpolator_linear_between_points() {
  CurveInterpolator curve = make_ramp_interpolator();

  float received = -1.0f;
  LambdaConsumer<float> consumer([&received](float v) { received = v; });
  curve.connect_to(&consumer);

  // Between (1,0) and (2,1): at x=1.4 output should be 0.4.
  curve.set(1.4f);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.4f, received);

  // Flat segment between (2,1) and (3,1): output stays 1.0.
  curve.set(2.5f);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, received);
}

void test_curve_interpolator_extrapolates_below_first_point() {
  CurveInterpolator curve = make_ramp_interpolator();

  float received = -1.0f;
  LambdaConsumer<float> consumer([&received](float v) { received = v; });
  curve.connect_to(&consumer);

  // Below the first point, extrapolate using the first segment gradient.
  // First segment (0,0)->(1,0) has gradient 0, so output stays 0.
  curve.set(-5.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, received);
}

void test_curve_interpolator_extrapolates_above_last_point() {
  CurveInterpolator curve = make_ramp_interpolator();

  float received = -1.0f;
  LambdaConsumer<float> consumer([&received](float v) { received = v; });
  curve.connect_to(&consumer);

  // Above the last point, extrapolate using the last segment gradient.
  // Last segment (2,1)->(3,1) has gradient 0, so output stays 1.0.
  curve.set(10.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, received);
}

void test_curve_interpolator_empty_outputs_zero() {
  CurveInterpolator curve;  // no samples

  float received = -1.0f;
  LambdaConsumer<float> consumer([&received](float v) { received = v; });
  curve.connect_to(&consumer);

  curve.set(42.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, received);
}

void test_curve_interpolator_single_sample() {
  std::set<CurveInterpolator::Sample> samples = {{5.0f, 7.0f}};
  CurveInterpolator curve(&samples);

  float received = -1.0f;
  LambdaConsumer<float> consumer([&received](float v) { received = v; });
  curve.connect_to(&consumer);

  // Below the only sample, with a single point the output is its value.
  curve.set(0.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 7.0f, received);
}

void test_curve_interpolator_json_round_trip() {
  CurveInterpolator curve = make_ramp_interpolator();

  JsonDocument out_doc;
  JsonObject out = out_doc.to<JsonObject>();
  TEST_ASSERT_TRUE(curve.to_json(out));
  TEST_ASSERT_EQUAL(4, out["samples"].as<JsonArray>().size());

  CurveInterpolator restored;
  JsonObject in = out_doc.as<JsonObject>();
  TEST_ASSERT_TRUE(restored.from_json(in));

  float received = -1.0f;
  LambdaConsumer<float> consumer([&received](float v) { received = v; });
  restored.connect_to(&consumer);

  restored.set(1.4f);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.4f, received);
}

// ---------------------------------------------------------------------------
// MovingAverage
// ---------------------------------------------------------------------------

void test_moving_average_fills_buffer_on_first_sample() {
  MovingAverage ma(4, 1.0);

  float received = -1.0f;
  LambdaConsumer<float> consumer([&received](float v) { received = v; });
  ma.connect_to(&consumer);

  // First value fills the whole buffer, so the average equals that value.
  ma.set(8.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 8.0f, received);
}

void test_moving_average_windowed_mean() {
  MovingAverage ma(4, 1.0);

  float received = -1.0f;
  LambdaConsumer<float> consumer([&received](float v) { received = v; });
  ma.connect_to(&consumer);

  // Buffer initializes to all 4.0 (avg 4.0). Then push 8 three times,
  // replacing three of the four slots: (8+8+8+4)/4 = 7.0.
  ma.set(4.0f);
  ma.set(8.0f);
  ma.set(8.0f);
  ma.set(8.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 7.0f, received);

  // One more 8 fully fills the window: average 8.0.
  ma.set(8.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 8.0f, received);
}

void test_moving_average_multiplier_scales_output() {
  MovingAverage ma(2, 2.0);

  float received = -1.0f;
  LambdaConsumer<float> consumer([&received](float v) { received = v; });
  ma.connect_to(&consumer);

  // Buffer fills with 10.0 (avg 10.0), scaled by 2.0 -> 20.0.
  ma.set(10.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 20.0f, received);
}

// ---------------------------------------------------------------------------
// LambdaTransform
// ---------------------------------------------------------------------------

void test_lambda_transform_zero_params() {
  LambdaTransform<float, float> doubler([](float x) { return x * 2.0f; });

  float received = -1.0f;
  LambdaConsumer<float> consumer([&received](float v) { received = v; });
  doubler.connect_to(&consumer);

  doubler.set(3.5f);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 7.0f, received);
}

void test_lambda_transform_two_params_and_json() {
  const ParamInfo param_info[2] = {{"gain", "Gain"}, {"bias", "Bias"}};
  LambdaTransform<float, float, float, float> affine(
      [](float x, float gain, float bias) { return x * gain + bias; }, 3.0f,
      1.0f, param_info);

  float received = -1.0f;
  LambdaConsumer<float> consumer([&received](float v) { received = v; });
  affine.connect_to(&consumer);

  affine.set(2.0f);  // 2*3 + 1 = 7
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 7.0f, received);

  // Serialize params, mutate, then restore and confirm behavior reverts.
  JsonDocument doc;
  JsonObject obj = doc.to<JsonObject>();
  TEST_ASSERT_TRUE(affine.to_json(obj));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 3.0f, obj["gain"].as<float>());
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, obj["bias"].as<float>());

  affine.param1_ = 0.0f;
  affine.param2_ = 0.0f;
  JsonObject in = doc.as<JsonObject>();
  TEST_ASSERT_TRUE(affine.from_json(in));

  affine.set(2.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 7.0f, received);
}

void test_lambda_transform_from_json_rejects_missing_key() {
  const ParamInfo param_info[2] = {{"gain", "Gain"}, {"bias", "Bias"}};
  LambdaTransform<float, float, float, float> affine(
      [](float x, float gain, float bias) { return x * gain + bias; }, 3.0f,
      1.0f, param_info);

  JsonDocument doc;
  doc["gain"] = 5.0f;  // "bias" intentionally missing
  JsonObject in = doc.as<JsonObject>();
  TEST_ASSERT_FALSE(affine.from_json(in));
}

// ---------------------------------------------------------------------------
// Hysteresis
// ---------------------------------------------------------------------------

void test_hysteresis_dead_zone_holds_state() {
  Hysteresis<float, int> hyst(10.0f, 20.0f, 0, 1);

  int received = -1;
  LambdaConsumer<int> consumer([&received](int v) { received = v; });
  hyst.connect_to(&consumer);

  // Rise above upper threshold -> high.
  hyst.set(25.0f);
  TEST_ASSERT_EQUAL(1, received);

  // Into dead zone -> hold high.
  hyst.set(15.0f);
  TEST_ASSERT_EQUAL(1, received);

  // Below lower threshold -> low.
  hyst.set(5.0f);
  TEST_ASSERT_EQUAL(0, received);

  // Back into dead zone -> hold low.
  hyst.set(15.0f);
  TEST_ASSERT_EQUAL(0, received);
}

void test_hysteresis_upper_threshold_is_inclusive() {
  Hysteresis<float, int> hyst(10.0f, 20.0f, 0, 1);

  int received = -1;
  LambdaConsumer<int> consumer([&received](int v) { received = v; });
  hyst.connect_to(&consumer);

  // Exactly at the upper threshold switches high (upper <= input).
  hyst.set(20.0f);
  TEST_ASSERT_EQUAL(1, received);
}

// ---------------------------------------------------------------------------
// Frequency
// ---------------------------------------------------------------------------

void test_frequency_first_call_suppressed() {
  Frequency freq(1.0);

  bool was_called = false;
  LambdaConsumer<float> consumer([&was_called](float v) { was_called = true; });
  freq.connect_to(&consumer);

  // The first input only establishes the baseline timestamp; nothing emitted.
  freq.set(10);
  TEST_ASSERT_FALSE(was_called);
}

void test_frequency_emits_after_elapsed_time() {
  Frequency freq(1.0);

  float received = -1.0f;
  bool was_called = false;
  LambdaConsumer<float> consumer([&](float v) {
    received = v;
    was_called = true;
  });
  freq.connect_to(&consumer);

  freq.set(10);  // baseline
  delay(100);
  freq.set(10);  // ~10 counts over ~0.1 s -> ~100 Hz

  TEST_ASSERT_TRUE(was_called);
  // Timing on hardware is imprecise; allow a wide band but confirm it is a
  // sane positive frequency in the expected order of magnitude.
  TEST_ASSERT_TRUE(received > 20.0f);
  TEST_ASSERT_TRUE(received < 500.0f);
}

void test_frequency_json_round_trip() {
  Frequency freq(0.25);

  JsonDocument doc;
  JsonObject obj = doc.to<JsonObject>();
  TEST_ASSERT_TRUE(freq.to_json(obj));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.25f, obj["multiplier"].as<float>());

  Frequency restored(1.0);
  JsonObject in = doc.as<JsonObject>();
  TEST_ASSERT_TRUE(restored.from_json(in));

  JsonDocument doc2;
  JsonObject obj2 = doc2.to<JsonObject>();
  TEST_ASSERT_TRUE(restored.to_json(obj2));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.25f, obj2["multiplier"].as<float>());
}

// ---------------------------------------------------------------------------
// Test runner
// ---------------------------------------------------------------------------

void setup() {
  delay(2000);

  UNITY_BEGIN();

  RUN_TEST(test_curve_interpolator_exact_sample_points);
  RUN_TEST(test_curve_interpolator_linear_between_points);
  RUN_TEST(test_curve_interpolator_extrapolates_below_first_point);
  RUN_TEST(test_curve_interpolator_extrapolates_above_last_point);
  RUN_TEST(test_curve_interpolator_empty_outputs_zero);
  RUN_TEST(test_curve_interpolator_single_sample);
  RUN_TEST(test_curve_interpolator_json_round_trip);

  RUN_TEST(test_moving_average_fills_buffer_on_first_sample);
  RUN_TEST(test_moving_average_windowed_mean);
  RUN_TEST(test_moving_average_multiplier_scales_output);

  RUN_TEST(test_lambda_transform_zero_params);
  RUN_TEST(test_lambda_transform_two_params_and_json);
  RUN_TEST(test_lambda_transform_from_json_rejects_missing_key);

  RUN_TEST(test_hysteresis_dead_zone_holds_state);
  RUN_TEST(test_hysteresis_upper_threshold_is_inclusive);

  RUN_TEST(test_frequency_first_call_suppressed);
  RUN_TEST(test_frequency_emits_after_elapsed_time);
  RUN_TEST(test_frequency_json_round_trip);

  UNITY_END();
}

void loop() {}
