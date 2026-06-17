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
#include "sensesp/signalk/signalk_listener.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/signalk/signalk_put_request_listener.h"
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

// The registry keeps the FIRST registration for a duplicate name. The destructor
// must only erase the entry it actually owns, so destroying an ignored duplicate
// must not strand the first registration.
void test_statuspageitem_duplicate_name_guard() {
  const char* name = "RegistryTestDuplicateName";
  auto* items = StatusPageItemBase::get_status_page_items();
  TEST_ASSERT_EQUAL(0, items->count(name));

  StatusPageItem<int> first(name, 1, "Test", 1000);
  TEST_ASSERT_EQUAL(1, items->count(name));
  StatusPageItemBase* first_ptr = items->at(name);
  {
    // The duplicate is ignored at registration (keep-first).
    StatusPageItem<int> second(name, 2, "Test", 1000);
    TEST_ASSERT_EQUAL(1, items->count(name));
    TEST_ASSERT_EQUAL_PTR(first_ptr, items->at(name));
  }
  // Destroying the ignored duplicate must leave the first registration intact.
  TEST_ASSERT_EQUAL(1, items->count(name));
  TEST_ASSERT_EQUAL_PTR(first_ptr, items->at(name));
}

void test_listener_unregisters_on_destruction() {
  size_t before_sk = SKListener::get_listeners().size();
  size_t before_put = SKPutListener::get_listeners().size();
  {
    SKListener listener("test.sklistener.unreg", 1000,
                        "/test/sklistener/unreg");
    FloatSKPutRequestListener put("test.skput.unreg");
    TEST_ASSERT_EQUAL(before_sk + 1, SKListener::get_listeners().size());
    TEST_ASSERT_EQUAL(before_put + 1, SKPutListener::get_listeners().size());
  }
  // Both listeners unregistered themselves in their destructors.
  TEST_ASSERT_EQUAL(before_sk, SKListener::get_listeners().size());
  TEST_ASSERT_EQUAL(before_put, SKPutListener::get_listeners().size());
}

// ---------------------------------------------------------------------------
// clear_registries() empties every registry
// ---------------------------------------------------------------------------

// Clearing the owning ConfigItem registry must actually destroy a sole-owned
// object, whose destructor then unregisters it from the raw-pointer registries
// while clear_registries() is still running -- the load-bearing ordering case.
void test_clear_registries_destroys_config_owned_object() {
  std::weak_ptr<SKOutput<float>> weak;
  {
    auto sk = std::make_shared<SKOutput<float>>("test.clear.owned",
                                                "/test/clear/owned");
    weak = sk;
    ConfigItem(sk);  // config_items_ co-owns; also in sources_ and transforms_
    sk.reset();      // drop the local owner: the registry is now the sole owner
    TEST_ASSERT_FALSE(weak.expired());
    TEST_ASSERT_TRUE(sources_contains(weak.lock().get()));
  }

  SensESPBaseApp::clear_registries();

  // The object was destroyed by releasing the ConfigItem registry, and its
  // destructor removed it from the raw-pointer registries during the clear.
  TEST_ASSERT_TRUE(weak.expired());
  TEST_ASSERT_EQUAL(0, SKEmitter::get_sources().size());
}

void test_clear_registries_empties_all() {
  auto sk = std::make_shared<SKOutput<float>>("test.clear.sk", "/test/clear/sk");
  ConfigItem(sk);
  auto status_item =
      std::make_shared<StatusPageItem<int>>("ClearTestItem", 0, "Test", 1100);
  UIButton::add("clear_test_button", "Clear Test Button");
  SKListener listener("test.clear.listener", 1000, "/test/clear/listener");
  FloatSKPutRequestListener put_listener("test.clear.put");

  // Assert membership of the specific objects, not just non-empty sizes, so
  // cross-test residue cannot mask a failed registration.
  TEST_ASSERT_TRUE(sources_contains(sk.get()));
  TEST_ASSERT_NOT_NULL(ConfigItemBase::get_config_item("/test/clear/sk").get());
  TEST_ASSERT_EQUAL(
      1, StatusPageItemBase::get_status_page_items()->count("ClearTestItem"));
  TEST_ASSERT_EQUAL(1, UIButton::get_ui_buttons().count("clear_test_button"));
  TEST_ASSERT_GREATER_OR_EQUAL(1, SKListener::get_listeners().size());
  TEST_ASSERT_GREATER_OR_EQUAL(1, SKPutListener::get_listeners().size());

  SensESPBaseApp::clear_registries();

  TEST_ASSERT_EQUAL(0, SKEmitter::get_sources().size());
  TEST_ASSERT_EQUAL(0, StatusPageItemBase::get_status_page_items()->size());
  TEST_ASSERT_EQUAL(0, UIButton::get_ui_buttons().size());
  TEST_ASSERT_EQUAL(0, ConfigItemBase::get_config_items()->size());
  TEST_ASSERT_EQUAL(0, SKListener::get_listeners().size());
  TEST_ASSERT_EQUAL(0, SKPutListener::get_listeners().size());
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  TEST_ASSERT_EQUAL(0, TransformBase::get_transforms().size());
#pragma GCC diagnostic pop

  // The locally-owned objects (sk, status_item, listeners) are destroyed at
  // scope exit; their destructors must safely no-op against the already-cleared
  // registries.
}

void test_clear_registries_idempotent() {
  SensESPBaseApp::clear_registries();
  // A second clear on already-empty registries must be a safe no-op.
  SensESPBaseApp::clear_registries();
  TEST_ASSERT_EQUAL(0, SKEmitter::get_sources().size());
  TEST_ASSERT_EQUAL(0, SKListener::get_listeners().size());
  TEST_ASSERT_EQUAL(0, ConfigItemBase::get_config_items()->size());
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
  RUN_TEST(test_statuspageitem_duplicate_name_guard);
  RUN_TEST(test_listener_unregisters_on_destruction);
  RUN_TEST(test_clear_registries_destroys_config_owned_object);
  RUN_TEST(test_clear_registries_empties_all);
  RUN_TEST(test_clear_registries_idempotent);

  UNITY_END();
}

void loop() {}
