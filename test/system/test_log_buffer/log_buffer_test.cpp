/**
 * @file log_buffer_test.cpp
 * @brief Unit tests for LogBuffer: append, snapshot-since cursor semantics,
 *        age/count eviction, truncation, gap detection, and session id.
 *
 * Tests drive push_line()/snapshot_since() directly with explicit timestamps,
 * so they do not depend on the esp_log vprintf hook or real time.
 */

#include <Arduino.h>

#include <string>

#include "sensesp/system/log_buffer.h"
#include "unity.h"

using namespace sensesp;

static void push(LogBuffer& buf, const std::string& text, uint32_t now_ms) {
  buf.push_line(text.c_str(), text.size(), now_ms);
}

// ---------------------------------------------------------------------------
// Append + cursor semantics
// ---------------------------------------------------------------------------

void test_initial_load_returns_all_in_order() {
  LogBuffer buf(200, 2000, 128);
  push(buf, "first", 10);
  push(buf, "second", 10);
  push(buf, "third", 10);

  LogSnapshot snap = buf.snapshot_since(0, /*has_since=*/false, 10);
  TEST_ASSERT_EQUAL(3, snap.lines.size());
  TEST_ASSERT_EQUAL_STRING("first", snap.lines[0].c_str());
  TEST_ASSERT_EQUAL_STRING("second", snap.lines[1].c_str());
  TEST_ASSERT_EQUAL_STRING("third", snap.lines[2].c_str());
  TEST_ASSERT_EQUAL(3, snap.next);
  TEST_ASSERT_FALSE(snap.gap);
}

void test_caught_up_returns_empty_same_cursor() {
  LogBuffer buf(200, 2000, 128);
  push(buf, "a", 10);
  push(buf, "b", 10);

  LogSnapshot snap = buf.snapshot_since(2, /*has_since=*/true, 10);
  TEST_ASSERT_EQUAL(0, snap.lines.size());
  TEST_ASSERT_EQUAL(2, snap.next);
  TEST_ASSERT_FALSE(snap.gap);
}

void test_delta_returns_only_newer() {
  LogBuffer buf(200, 2000, 128);
  push(buf, "a", 10);   // seq 0
  push(buf, "b", 10);   // seq 1
  LogSnapshot first = buf.snapshot_since(0, /*has_since=*/false, 10);
  push(buf, "c", 10);   // seq 2

  LogSnapshot snap = buf.snapshot_since(first.next, /*has_since=*/true, 10);
  TEST_ASSERT_EQUAL(1, snap.lines.size());
  TEST_ASSERT_EQUAL_STRING("c", snap.lines[0].c_str());
  TEST_ASSERT_EQUAL(3, snap.next);
  TEST_ASSERT_FALSE(snap.gap);
}

// ---------------------------------------------------------------------------
// Eviction + truncation
// ---------------------------------------------------------------------------

void test_overlong_line_truncated() {
  LogBuffer buf(200, 2000, /*max_line_length=*/8);
  push(buf, "0123456789ABCDEF", 10);

  LogSnapshot snap = buf.snapshot_since(0, false, 10);
  TEST_ASSERT_EQUAL(1, snap.lines.size());
  TEST_ASSERT_EQUAL(8, snap.lines[0].size());
  TEST_ASSERT_EQUAL_STRING("01234567", snap.lines[0].c_str());
  TEST_ASSERT_EQUAL(1, snap.next);  // sequence still advanced
}

void test_trailing_newline_stripped() {
  LogBuffer buf(200, 2000, 128);
  push(buf, "line\r\n", 10);
  LogSnapshot snap = buf.snapshot_since(0, false, 10);
  TEST_ASSERT_EQUAL(1, snap.lines.size());
  TEST_ASSERT_EQUAL_STRING("line", snap.lines[0].c_str());
}

void test_count_cap_evicts_oldest() {
  LogBuffer buf(/*max_lines=*/3, 2000, 128);
  push(buf, "0", 10);
  push(buf, "1", 10);
  push(buf, "2", 10);
  push(buf, "3", 10);
  push(buf, "4", 10);

  TEST_ASSERT_EQUAL(3, buf.size());
  LogSnapshot snap = buf.snapshot_since(0, false, 10);
  TEST_ASSERT_EQUAL(3, snap.lines.size());
  TEST_ASSERT_EQUAL_STRING("2", snap.lines[0].c_str());
  TEST_ASSERT_EQUAL_STRING("4", snap.lines[2].c_str());
}

void test_age_cap_evicts_old() {
  LogBuffer buf(200, /*max_age_ms=*/1000, 128);
  push(buf, "old", 0);
  push(buf, "new", 2000);  // prune on insert drops "old" (age 2000 > 1000)

  TEST_ASSERT_EQUAL(1, buf.size());
  LogSnapshot snap = buf.snapshot_since(0, false, 2000);
  TEST_ASSERT_EQUAL(1, snap.lines.size());
  TEST_ASSERT_EQUAL_STRING("new", snap.lines[0].c_str());
}

// ---------------------------------------------------------------------------
// Gap detection
// ---------------------------------------------------------------------------

void test_gap_flag_when_cursor_evicted() {
  LogBuffer buf(/*max_lines=*/3, 2000, 128);
  push(buf, "0", 10);
  push(buf, "1", 10);
  push(buf, "2", 10);
  push(buf, "3", 10);
  push(buf, "4", 10);  // retained seqs: 2,3,4

  // Client last saw up to seq 0, asks from seq 1; seq 1 was evicted -> gap.
  LogSnapshot snap = buf.snapshot_since(1, /*has_since=*/true, 10);
  TEST_ASSERT_TRUE(snap.gap);
  TEST_ASSERT_EQUAL(3, snap.lines.size());
  TEST_ASSERT_EQUAL_STRING("2", snap.lines[0].c_str());
}

void test_no_gap_on_empty_caught_up() {
  LogBuffer buf(200, 2000, 128);
  LogSnapshot snap = buf.snapshot_since(0, /*has_since=*/true, 10);
  TEST_ASSERT_EQUAL(0, snap.lines.size());
  TEST_ASSERT_FALSE(snap.gap);
  TEST_ASSERT_EQUAL(0, snap.next);
}

void test_gap_when_all_lines_age_evicted() {
  LogBuffer buf(200, /*max_age_ms=*/1000, 128);
  push(buf, "a", 0);  // seq 0
  push(buf, "b", 0);  // seq 1

  // A later poll past the age window prunes the whole buffer. The client's
  // cursor (0) predates next_seq_ (2), so this is a gap, not caught-up.
  LogSnapshot snap = buf.snapshot_since(0, /*has_since=*/true, 5000);
  TEST_ASSERT_EQUAL(0, buf.size());
  TEST_ASSERT_EQUAL(0, snap.lines.size());
  TEST_ASSERT_TRUE(snap.gap);
  TEST_ASSERT_EQUAL(2, snap.next);
}

void test_post_reboot_stale_cursor_no_gap() {
  // After a reboot next_seq_ restarts low while a client may still hold a high
  // cursor. since > next_seq_ must read as caught-up (no gap, no lines), letting
  // the client detect the reset rather than seeing a phantom gap.
  LogBuffer buf(200, 2000, 128);
  push(buf, "x", 10);  // seq 0
  push(buf, "y", 10);  // seq 1

  LogSnapshot snap = buf.snapshot_since(/*since=*/99, /*has_since=*/true, 10);
  TEST_ASSERT_EQUAL(0, snap.lines.size());
  TEST_ASSERT_FALSE(snap.gap);
  TEST_ASSERT_EQUAL(2, snap.next);
}

void test_prune_keeps_lines_when_now_before_timestamp() {
  // now_ms earlier than a record's timestamp (e.g. millis() wrap) must not
  // underflow the age math and evict everything.
  LogBuffer buf(200, /*max_age_ms=*/1000, 128);
  push(buf, "kept", /*now_ms=*/5000);

  LogSnapshot snap = buf.snapshot_since(0, /*has_since=*/false, /*now_ms=*/10);
  TEST_ASSERT_EQUAL(1, buf.size());
  TEST_ASSERT_EQUAL(1, snap.lines.size());
  TEST_ASSERT_EQUAL_STRING("kept", snap.lines[0].c_str());
}

void test_bare_newline_dropped() {
  LogBuffer buf(200, 2000, 128);
  push(buf, "\r\n", 10);
  push(buf, "", 10);

  LogSnapshot snap = buf.snapshot_since(0, /*has_since=*/false, 10);
  TEST_ASSERT_EQUAL(0, snap.lines.size());
  TEST_ASSERT_EQUAL(0, snap.next);  // empty lines do not advance the sequence
}

// ---------------------------------------------------------------------------
// Session id
// ---------------------------------------------------------------------------

void test_session_id_stable_across_calls() {
  LogBuffer buf(200, 2000, 128);
  uint32_t s1 = buf.snapshot_since(0, false, 10).session_id;
  push(buf, "x", 10);
  uint32_t s2 = buf.snapshot_since(0, false, 10).session_id;
  TEST_ASSERT_EQUAL(buf.session_id(), s1);
  TEST_ASSERT_EQUAL(s1, s2);
}

// ---------------------------------------------------------------------------
// Test runner
// ---------------------------------------------------------------------------

void setup() {
  delay(2000);  // Allow serial connection to stabilize

  UNITY_BEGIN();

  RUN_TEST(test_initial_load_returns_all_in_order);
  RUN_TEST(test_caught_up_returns_empty_same_cursor);
  RUN_TEST(test_delta_returns_only_newer);

  RUN_TEST(test_overlong_line_truncated);
  RUN_TEST(test_trailing_newline_stripped);
  RUN_TEST(test_count_cap_evicts_oldest);
  RUN_TEST(test_age_cap_evicts_old);

  RUN_TEST(test_gap_flag_when_cursor_evicted);
  RUN_TEST(test_no_gap_on_empty_caught_up);
  RUN_TEST(test_gap_when_all_lines_age_evicted);
  RUN_TEST(test_post_reboot_stale_cursor_no_gap);
  RUN_TEST(test_prune_keeps_lines_when_now_before_timestamp);
  RUN_TEST(test_bare_newline_dropped);

  RUN_TEST(test_session_id_stable_across_calls);

  UNITY_END();
}

void loop() {}
