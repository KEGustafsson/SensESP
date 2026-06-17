/**
 * @file registries_test.cpp
 * @brief Tests for static object registry cleanup (#894).
 *
 * Verifies that objects unregister from the raw-pointer registries when they
 * are destroyed (so the registries can never hold a dangling pointer), and
 * that SensESPBaseApp::clear_registries() empties every registry for clean
 * restart / test isolation.
 *
 * SKOutput<T> derives from both SKEmitter and SymmetricTransform<T>, so a
 * single instance exercises both the SKEmitter source registry and the
 * Transform registry.
 */

#include <Arduino.h>

#include <memory>

#include "sensesp/signalk/signalk_emitter.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/transforms/transform.h"
#include "sensesp/ui/config_item.h"
#include "sensesp/ui/status_page_item.h"
#include "sensesp/ui/ui_button.h"
#include "sensesp_base_app.h"
#include "unity.h"

using namespace sensesp;

static bool sources_contains(SKEmitter* emitter) {
  for (auto* source : SKEmitter::get_sources()) {
    if (source == emitter) {
      return true;
    }
  }
  return false;
}

// ---------------------------------------------------------------------------
// Unregister on destruction: raw-pointer registries
// ---------------------------------------------------------------------------

void test_skemitter_unregisters_on_destruction() {
  size_t before = SKEmitter::get_sources().size();
  {
    SKOutput<float> out("test.skemitter.unregister", "/test/skemitter/unreg");
    TEST_ASSERT_TRUE(sources_contains(&out));
    TEST_ASSERT_EQUAL(before + 1, SKEmitter::get_sources().size());
  }
  // The emitter unregistered itself in its destructor.
  TEST_ASSERT_EQUAL(before, SKEmitter::get_sources().size());
}

void test_transformbase_unregisters_on_destruction() {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  size_t before = TransformBase::get_transforms().size();
  {
    SKOutput<float> out("test.transform.unregister", "/test/transform/unreg");
    TEST_ASSERT_EQUAL(before + 1, TransformBase::get_transforms().size());
  }
  // The transform unregistered itself in its destructor.
  TEST_ASSERT_EQUAL(before, TransformBase::get_transforms().size());
#pragma GCC diagnostic pop
}

void test_statuspageitem_unregisters_on_destruction() {
  const char* name = "RegistryTestStatusItem";
  TEST_ASSERT_EQUAL(0, StatusPageItemBase::get_status_page_items()->count(name));
  {
    StatusPageItem<int> item(name, 0, "Test", 1000);
    TEST_ASSERT_EQUAL(
        1, StatusPageItemBase::get_status_page_items()->count(name));
  }
  // The item unregistered itself in its destructor.
  TEST_ASSERT_EQUAL(0, StatusPageItemBase::get_status_page_items()->count(name));
}

// ---------------------------------------------------------------------------
// clear_registries() empties every registry
// ---------------------------------------------------------------------------

void test_clear_registries_empties_all() {
  // Keep the objects alive through local owners so the registries stay
  // populated until clear_registries() runs.
  auto sk = std::make_shared<SKOutput<float>>("test.clear.sk", "/test/clear/sk");
  ConfigItem(sk);
  auto status_item =
      std::make_shared<StatusPageItem<int>>("ClearTestItem", 0, "Test", 1100);
  UIButton::add("clear_test_button", "Clear Test Button");

  TEST_ASSERT_GREATER_OR_EQUAL(1, SKEmitter::get_sources().size());
  TEST_ASSERT_GREATER_OR_EQUAL(1, StatusPageItemBase::get_status_page_items()->size());
  TEST_ASSERT_GREATER_OR_EQUAL(1, UIButton::get_ui_buttons().size());
  TEST_ASSERT_GREATER_OR_EQUAL(1, ConfigItemBase::get_config_items()->size());

  SensESPBaseApp::clear_registries();

  TEST_ASSERT_EQUAL(0, SKEmitter::get_sources().size());
  TEST_ASSERT_EQUAL(0, StatusPageItemBase::get_status_page_items()->size());
  TEST_ASSERT_EQUAL(0, UIButton::get_ui_buttons().size());
  TEST_ASSERT_EQUAL(0, ConfigItemBase::get_config_items()->size());
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  TEST_ASSERT_EQUAL(0, TransformBase::get_transforms().size());
#pragma GCC diagnostic pop

  // The locally-owned objects are destroyed at scope exit; their destructors
  // must safely no-op against the already-cleared registries.
}

// ---------------------------------------------------------------------------
// Test runner
// ---------------------------------------------------------------------------

void setup() {
  delay(2000);

  UNITY_BEGIN();

  RUN_TEST(test_skemitter_unregisters_on_destruction);
  RUN_TEST(test_transformbase_unregisters_on_destruction);
  RUN_TEST(test_statuspageitem_unregisters_on_destruction);
  RUN_TEST(test_clear_registries_empties_all);

  UNITY_END();
}

void loop() {}
