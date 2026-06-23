/**
 * @file debounce_config_load_test.cpp
 * @brief Regression test for DigitalInputDebounceCounter config persistence.
 *
 * load() calls the virtual from_json(). The bug (PR #1021) was that load() ran
 * from the DigitalInputCounter base constructor, where the dynamic type is
 * DigitalInputCounter, so DigitalInputCounter::from_json() (read_delay only) ran
 * instead of the derived DigitalInputDebounceCounter::from_json() (read_delay +
 * ignore_interval). A persisted ignore_interval was silently dropped on reboot.
 *
 * This test seeds a persisted config, constructs a FRESH counter through the
 * load()-during-construction path, and asserts ignore_interval survived. A
 * plain to_json/from_json round-trip would NOT catch the bug: it calls from_json
 * on an already-complete object where dispatch is correct either way. The
 * ignore_interval assertion is the one that distinguishes the derived override
 * from the base one (both write read_delay; only the derived writes
 * ignore_interval), so it is what fails before the fix and passes after.
 *
 * The counter's config members are private, so the loaded value is read back
 * through the public Serializable::to_json().
 */

#include <Arduino.h>

#include <ArduinoJson.h>

#include "sensesp/sensors/digital_input.h"
#include "sensesp/system/serializable.h"
#include "sensesp_base_app.h"
#include "sensesp_minimal_app_builder.h"
#include "unity.h"

using namespace sensesp;

namespace {

constexpr const char* kConfigPath = "/test/debounce_config_load";
constexpr unsigned int kSavedReadDelay = 500;
constexpr unsigned int kSavedIgnoreInterval = 250;
constexpr unsigned int kCtorReadDelay = 1000;
constexpr unsigned int kCtorIgnoreInterval = 750;

unsigned int serialized_value(DigitalInputDebounceCounter& counter,
                              const char* key) {
  JsonDocument doc;
  JsonObject obj = doc.to<JsonObject>();
  static_cast<Serializable&>(counter).to_json(obj);
  return obj[key].as<unsigned int>();
}

}  // namespace

void test_debounce_counter_restores_persisted_ignore_interval() {
  SensESPMinimalAppBuilder builder;
  auto app = builder.get_app();
  TEST_ASSERT_NOT_NULL(app);

  // Remove any config left by a previous run on this device's flash.
  {
    DigitalInputDebounceCounter cleaner(4, INPUT_PULLUP, RISING, kSavedReadDelay,
                                        kSavedIgnoreInterval, kConfigPath);
    cleaner.clear();
  }

  // Seed a known config. With no file on disk, the seeder's construction-time
  // load() finds nothing and keeps the constructor values, so save() writes
  // exactly kSavedReadDelay / kSavedIgnoreInterval.
  {
    DigitalInputDebounceCounter seeder(4, INPUT_PULLUP, RISING, kSavedReadDelay,
                                       kSavedIgnoreInterval, kConfigPath);
    TEST_ASSERT_TRUE(seeder.save());
  }

  // Construct with different defaults; the construction-time load() must restore
  // the persisted values. A different pin avoids re-registering the same ISR.
  DigitalInputDebounceCounter restored(5, INPUT_PULLUP, RISING, kCtorReadDelay,
                                       kCtorIgnoreInterval, kConfigPath);

  // read_delay is restored by both the base and derived from_json, so it only
  // confirms load() ran at all.
  TEST_ASSERT_EQUAL_UINT(kSavedReadDelay,
                         serialized_value(restored, "read_delay"));
  // ignore_interval is restored only by the derived from_json. This is the
  // assertion that fails before the fix (base from_json runs, value stays at the
  // constructor default) and passes after.
  TEST_ASSERT_EQUAL_UINT(kSavedIgnoreInterval,
                         serialized_value(restored, "ignore_interval"));
}

void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_debounce_counter_restores_persisted_ignore_interval);
  UNITY_END();
}

void loop() {}
