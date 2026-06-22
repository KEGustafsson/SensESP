/**
 * @file tofu_decision_test.cpp
 * @brief Host unit tests for the pure TOFU capture decision (signalk_tofu.h).
 *
 * Runs on the `native` env (no mbedTLS / Arduino):
 *   pio test -e native -f native/test_tofu_decision
 */

#include <unity.h>

#include "sensesp/signalk/signalk_tofu.h"

using namespace sensesp;

static int decide(bool has_leaf_anchor, bool leaf_matches, bool ca_present,
                  bool leaf_has_identity = true) {
  return static_cast<int>(tofu_decide_capture(
      has_leaf_anchor, leaf_matches, ca_present, leaf_has_identity));
}

// First use (no anchor stored): capture the CA when one is present, else the leaf.
void test_first_use_no_ca_captures_leaf(void) {
  TEST_ASSERT_EQUAL_INT(static_cast<int>(TofuCaptureDecision::kCaptureLeaf),
                        decide(false, false, false));
}
void test_first_use_with_ca_captures_ca(void) {
  TEST_ASSERT_EQUAL_INT(static_cast<int>(TofuCaptureDecision::kCaptureCa),
                        decide(false, false, true));
}

// Leaf-fingerprint mode, leaf matches: accept. Mode is fixed at first use, so a
// CA presented later to a leaf-pinned device is NOT adopted automatically --
// it is ignored and the leaf pin kept (reset-tofu is required to re-pin as CA).
void test_leaf_match_no_ca_accepts(void) {
  TEST_ASSERT_EQUAL_INT(static_cast<int>(TofuCaptureDecision::kAccept),
                        decide(true, true, false));
}
void test_leaf_match_with_ca_still_accepts(void) {
  TEST_ASSERT_EQUAL_INT(static_cast<int>(TofuCaptureDecision::kAccept),
                        decide(true, true, true));
}

// Leaf-fingerprint mode, leaf does NOT match: reject. Crucially, a CA presented
// alongside a mismatched leaf must NOT rescue it -- an attacker cannot override
// a failed leaf check by offering a CA.
void test_leaf_mismatch_rejects(void) {
  TEST_ASSERT_EQUAL_INT(static_cast<int>(TofuCaptureDecision::kReject),
                        decide(true, false, false));
}
void test_leaf_mismatch_with_ca_still_rejects(void) {
  TEST_ASSERT_EQUAL_INT(static_cast<int>(TofuCaptureDecision::kReject),
                        decide(true, false, true));
}

// First use with a CA in the chain but no bindable leaf identity (no SAN): the
// CA must NOT be adopted; fall back to leaf-fingerprint mode so a public CA
// cannot be trusted to vouch for any leaf.
void test_first_use_ca_without_identity_captures_leaf(void) {
  TEST_ASSERT_EQUAL_INT(static_cast<int>(TofuCaptureDecision::kCaptureLeaf),
                        decide(false, false, true, false));
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_first_use_no_ca_captures_leaf);
  RUN_TEST(test_first_use_with_ca_captures_ca);
  RUN_TEST(test_leaf_match_no_ca_accepts);
  RUN_TEST(test_leaf_match_with_ca_still_accepts);
  RUN_TEST(test_leaf_mismatch_rejects);
  RUN_TEST(test_leaf_mismatch_with_ca_still_rejects);
  RUN_TEST(test_first_use_ca_without_identity_captures_leaf);
  return UNITY_END();
}
