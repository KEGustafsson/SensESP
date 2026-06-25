/**
 * @file ws_delta_size_test.cpp
 * @brief Host unit tests for the oversize-delta drop decision
 *        (signalk_ws_delta_size.h).
 *
 * Runs on the `native` env (no Arduino / esp_websocket_client):
 *   pio test -e native -f native/test_ws_delta_size
 *
 * The boundary is locked deliberately: esp_websocket_client copies up to
 * buffer_size payload bytes per transport write, so a delta of exactly
 * buffer_size is sent as a single chunk and must NOT be dropped; only a delta
 * longer than buffer_size would split into multiple non-blocking writes and
 * abort the connection. A frame-header margin must not be subtracted.
 */

#include <unity.h>

#include "sensesp/signalk/signalk_ws_delta_size.h"

using namespace sensesp;

// A delta shorter than the buffer fits in one chunk: send it.
void test_under_buffer_is_sent(void) {
  TEST_ASSERT_FALSE(sk_delta_exceeds_ws_buffer(1023, 1024));
}

// A delta of exactly buffer_size is one chunk (FIN set), not a multi-write: send.
void test_exactly_buffer_is_sent(void) {
  TEST_ASSERT_FALSE(sk_delta_exceeds_ws_buffer(1024, 1024));
}

// One byte over the buffer splits into two writes that can abort: drop.
void test_one_over_buffer_is_dropped(void) {
  TEST_ASSERT_TRUE(sk_delta_exceeds_ws_buffer(1025, 1024));
}

// An empty delta is always sendable.
void test_empty_is_sent(void) {
  TEST_ASSERT_FALSE(sk_delta_exceeds_ws_buffer(0, 1024));
}

// The boundary tracks an overridden buffer size, not the 1024 default.
void test_custom_buffer_boundary(void) {
  TEST_ASSERT_FALSE(sk_delta_exceeds_ws_buffer(8192, 8192));
  TEST_ASSERT_TRUE(sk_delta_exceeds_ws_buffer(8193, 8192));
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_under_buffer_is_sent);
  RUN_TEST(test_exactly_buffer_is_sent);
  RUN_TEST(test_one_over_buffer_is_dropped);
  RUN_TEST(test_empty_is_sent);
  RUN_TEST(test_custom_buffer_boundary);
  return UNITY_END();
}
