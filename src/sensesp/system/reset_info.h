#ifndef SENSESP_SRC_SENSESP_SYSTEM_RESET_INFO_H_
#define SENSESP_SRC_SENSESP_SYSTEM_RESET_INFO_H_

#include <cstdint>

namespace sensesp {

/**
 * @brief Reset-cause and pre-reset heap diagnostics for the status page.
 *
 * begin_reset_info() records esp_reset_reason() (why the last boot happened)
 * and, from a value carried across the reset in RTC memory, the lowest free
 * heap the previous boot reached before it ended. Together these distinguish a
 * crash (Panic/abort) from a watchdog, a brownout, or a deliberate software
 * restart, and show whether the heap was exhausted -- diagnosable remotely over
 * /api/info, without a serial console.
 *
 * The watermark lives in RTC_NOINIT memory, which survives software reset,
 * panic, and watchdog resets but not a power cycle. A magic number guards it so
 * a cold boot reports "unknown" rather than uninitialised garbage.
 */

/// Record the reset reason and the previous boot's heap watermark, then re-arm
/// the watermark for this boot. Call once, early in setup().
void begin_reset_info();

/// Human-readable reason for the most recent reset (e.g. "Panic/abort",
/// "Task watchdog", "Brownout", "Software restart", "Power-on").
const char* reset_reason_str();

/// Lowest free heap (bytes) seen before the previous reset, or 0 if unknown
/// (first boot after power-up, RTC contents not valid).
uint32_t min_free_heap_before_reset();

/// Stamp this boot's current minimum free heap into RTC memory. Call
/// periodically; it is cheap. After a reset, min_free_heap_before_reset()
/// returns the last value stamped here.
void update_min_free_heap_watermark();

}  // namespace sensesp

#endif  // SENSESP_SRC_SENSESP_SYSTEM_RESET_INFO_H_
