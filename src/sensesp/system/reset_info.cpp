#include "sensesp/system/reset_info.h"

#include <esp_attr.h>
#include <esp_system.h>

namespace sensesp {

namespace {
// Bumped if the RTC layout below changes, so a stale image reads as a cold boot.
constexpr uint32_t kRtcMagic = 0x5E5E0001;

// RTC_NOINIT memory is not zeroed at startup and survives panic, watchdog, and
// software resets (but not power loss), so it carries the watermark across a
// crash. Guarded by kRtcMagic to reject uninitialised contents after power-up.
RTC_NOINIT_ATTR uint32_t rtc_magic_;
RTC_NOINIT_ATTR uint32_t rtc_min_free_heap_;

esp_reset_reason_t reason_ = ESP_RST_UNKNOWN;
uint32_t min_free_before_reset_ = 0;
}  // namespace

void begin_reset_info() {
  reason_ = esp_reset_reason();
  min_free_before_reset_ = (rtc_magic_ == kRtcMagic) ? rtc_min_free_heap_ : 0;
  rtc_magic_ = kRtcMagic;
  rtc_min_free_heap_ = esp_get_minimum_free_heap_size();
}

const char* reset_reason_str() {
  switch (reason_) {
    case ESP_RST_POWERON:
      return "Power-on";
    case ESP_RST_EXT:
      return "External pin";
    case ESP_RST_SW:
      return "Software restart";
    case ESP_RST_PANIC:
      return "Panic/abort";
    case ESP_RST_INT_WDT:
      return "Interrupt watchdog";
    case ESP_RST_TASK_WDT:
      return "Task watchdog";
    case ESP_RST_WDT:
      return "Other watchdog";
    case ESP_RST_DEEPSLEEP:
      return "Deep-sleep wake";
    case ESP_RST_BROWNOUT:
      return "Brownout";
    case ESP_RST_SDIO:
      return "SDIO";
    default:
      return "Unknown";
  }
}

uint32_t min_free_heap_before_reset() { return min_free_before_reset_; }

void update_min_free_heap_watermark() {
  rtc_min_free_heap_ = esp_get_minimum_free_heap_size();
}

}  // namespace sensesp
