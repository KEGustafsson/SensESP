#ifndef SENSESP_SRC_SENSESP_SIGNALK_SIGNALK_TOFU_H_
#define SENSESP_SRC_SENSESP_SIGNALK_SIGNALK_TOFU_H_

namespace sensesp {

/**
 * @brief Capture-mode decision for TOFU certificate pinning.
 *
 * Pure and dependency-free (no mbedTLS, no Arduino) so it can be unit-tested on
 * the host. Captures the security-relevant branching of the verify callback in
 * one place; the callback supplies the booleans from the presented chain and
 * acts on the result.
 */
enum class TofuCaptureDecision {
  kAccept,       ///< leaf matches the stored fingerprint; nothing to capture
  kReject,       ///< leaf does not match the stored fingerprint
  kCaptureLeaf,  ///< first use, no usable CA in the chain: pin the leaf
  kCaptureCa     ///< first use, usable CA present: pin the CA
};

/**
 * @param has_leaf_anchor    A leaf fingerprint is stored (leaf-fingerprint mode).
 * @param leaf_matches_anchor The presented leaf equals the stored fingerprint.
 * @param ca_present         A usable CA (basicConstraints CA:TRUE) was seen in
 *                           the presented chain.
 * @param leaf_has_identity  The leaf carries a bindable identity (>=1 DNS SAN).
 *
 * Mode is chosen once, at first use, and never changes on its own. A CA is
 * adopted only when one is present AND the leaf has an identity to bind
 * (ca_present && leaf_has_identity). Without a bindable identity, CA-anchor mode
 * would trust any leaf the CA signs (the public-CA hole), so capture falls back
 * to leaf-fingerprint mode instead. A device already pinned to a leaf stays in
 * leaf mode even if the server later starts presenting a CA; moving to CA mode
 * requires a manual reset-tofu followed by a fresh first-use capture.
 *
 * Note: CA-anchor mode (a CA is already pinned) is handled separately by
 * mbedTLS REQUIRED-mode validation plus the SAN identity check, and never
 * reaches this function. A captured CA is committed only after the connection
 * succeeds, so an unauthenticated MITM handshake cannot plant a trust anchor.
 */
inline TofuCaptureDecision tofu_decide_capture(bool has_leaf_anchor,
                                               bool leaf_matches_anchor,
                                               bool ca_present,
                                               bool leaf_has_identity) {
  if (!has_leaf_anchor) {
    const bool ca_usable = ca_present && leaf_has_identity;
    return ca_usable ? TofuCaptureDecision::kCaptureCa
                     : TofuCaptureDecision::kCaptureLeaf;
  }
  return leaf_matches_anchor ? TofuCaptureDecision::kAccept
                             : TofuCaptureDecision::kReject;
}

}  // namespace sensesp

#endif  // SENSESP_SRC_SENSESP_SIGNALK_SIGNALK_TOFU_H_
