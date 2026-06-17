#include "sensesp/signalk/signalk_ws_client.h"
#include "unity.h"

using namespace sensesp;

// A token may only be discarded when the rejecting server is reached over an
// authenticated (TLS) channel and the upgrade status is 401.
void test_clears_on_ssl_401() {
  TEST_ASSERT_TRUE(should_clear_token_on_status(true, 401));
}

// Any non-401 status over TLS is a transport/TLS/network error: keep the token.
void test_keeps_token_on_ssl_non_401() {
  TEST_ASSERT_FALSE(should_clear_token_on_status(true, 0));
  TEST_ASSERT_FALSE(should_clear_token_on_status(true, 200));
  TEST_ASSERT_FALSE(should_clear_token_on_status(true, 426));
  TEST_ASSERT_FALSE(should_clear_token_on_status(true, 500));
}

// Over plaintext the status is unauthenticated and could be injected by an
// on-path attacker, so a 401 must never wipe the token.
void test_keeps_token_on_plaintext_401() {
  TEST_ASSERT_FALSE(should_clear_token_on_status(false, 401));
}

void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_clears_on_ssl_401);
  RUN_TEST(test_keeps_token_on_ssl_non_401);
  RUN_TEST(test_keeps_token_on_plaintext_401);
  UNITY_END();
}

void loop() {}
