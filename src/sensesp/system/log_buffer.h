#ifndef SENSESP_SRC_SENSESP_SYSTEM_LOG_BUFFER_H_
#define SENSESP_SRC_SENSESP_SYSTEM_LOG_BUFFER_H_

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <cstdarg>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace sensesp {

/**
 * @brief A single captured log line.
 */
struct LogRecord {
  uint32_t seq;
  uint32_t timestamp_ms;
  std::string text;
};

/// Maximum captured line length, and the size of each queue slot's text.
inline constexpr size_t kLogCaptureLineMax = 256;
/// Depth of the lock-free handoff queue between the vprintf hook and the
/// capture task. Bursts beyond this drop lines rather than block the logger.
inline constexpr size_t kLogCaptureQueueDepth = 8;
/// Stack (bytes) and priority of the capture task. The task does the
/// heap-allocating work the hook offloads (ANSI strip + std::string build + a
/// largest-free-block probe). Measured worst case ~0.7 KB used on an ESP32-C3;
/// 3072 keeps a conservative margin because this task allocates under memory
/// pressure. Priority 1 matches the other SensESP service tasks.
inline constexpr uint32_t kLogCaptureTaskStackSize = 3072;
inline constexpr UBaseType_t kLogCaptureTaskPriority = 1;
/// Minimum largest-free-block (bytes) required before the capture path will
/// allocate. Below this, push_line() drops the line instead of building the
/// std::string/deque record. With C++ exceptions disabled (the default on
/// ESP32) a failed allocation throws std::bad_alloc, which routes to abort()
/// and reboots the device -- so the logger must never be the allocation that
/// exhausts the heap, least of all while capturing the errors that report the
/// pressure. The margin is far larger than one line (<= kLogCaptureLineMax plus
/// a deque node) to stay clear of the check-then-allocate race and to leave
/// contiguous heap for the TLS Signal K connection.
inline constexpr size_t kLogCaptureMinHeadroomBytes = 8192;

/// One formatted line in transit from the vprintf hook to the capture task.
/// Copied by value through the queue, so the hook allocates nothing.
struct LogQueueItem {
  uint32_t timestamp_ms;
  uint16_t length;
  char text[kLogCaptureLineMax];
};

/**
 * @brief Result of a snapshot_since() query, ready to serialize for the web UI.
 *
 * `session_id` lets a client detect a device reboot (the sequence counter
 * resets to 0 on boot, so a stale high cursor would otherwise stall silently).
 * `next` is the cursor to send on the following poll. `gap` is true when the
 * client's cursor predated the oldest retained line, i.e. some lines were
 * evicted before the client read them.
 */
struct LogSnapshot {
  uint32_t session_id;
  uint32_t next;
  bool gap;
  std::vector<std::string> lines;
};

/**
 * @brief Captures ESP_LOGx output into a bounded RAM buffer for the web UI.
 *
 * install() chains an esp_log_set_vprintf() hook: every formatted log line is
 * forwarded to the previously installed handler (so UART output is unchanged)
 * and also appended to a bounded in-memory buffer. The buffer is a live tail,
 * not a persistent log: it is bounded by age (~2 s) and a hard line count, and
 * is never written to flash.
 *
 * Capture only works when the firmware is built with -D USE_ESP_IDF_LOG;
 * otherwise Arduino-ESP32 reroutes ESP_LOGx to log_printf, bypassing the
 * vprintf hook (a compile-time #warning is emitted in that case).
 *
 * Thread-safety: the vprintf hook runs in the context of whichever task logged,
 * so it must stay cheap on stack. On ESP-IDF log V1 it formats the line once
 * into a small stack buffer, emits it to the console itself (fwrite to stdout),
 * and hands the same buffer to a fixed-size queue; a dedicated capture task
 * drains the queue and does the heap-allocating buffer work (ANSI stripping,
 * std::string, deque) on its own adequately sized stack, keeping the hook from
 * overflowing small-stack caller tasks. Over-length lines, ISR / pre-install
 * context, and log V2 fall back to the chained previous handler for console
 * output. A FreeRTOS mutex guards the records against the HTTP reader, which
 * copies lines out while holding it and must not log while holding it.
 */
class LogBuffer {
 public:
  // max_line_length defaults to kLogCaptureLineMax to match the capture buffer
  // in vprintf_trampoline(); a larger value cannot recover more than that
  // buffer already captured.
  explicit LogBuffer(size_t max_lines = 100, uint32_t max_age_ms = 2000,
                     size_t max_line_length = kLogCaptureLineMax,
                     size_t min_headroom_bytes = kLogCaptureMinHeadroomBytes);

  /**
   * @brief Install the chained vprintf hook. Call once, early in startup.
   */
  void install();

  /**
   * @brief Append a pre-formatted log line to the buffer.
   *
   * Trailing CR/LF is stripped and the text is truncated to max_line_length.
   * `now_ms` is the capture time (esp_log_timestamp() in production); tests
   * pass an explicit value to exercise age-based eviction.
   */
  void push_line(const char* text, size_t length, uint32_t now_ms);

  /**
   * @brief Return retained lines newer than `since`.
   *
   * @param since   Cursor from the client's previous poll (the prior `next`).
   * @param has_since  False on the initial load (no cursor) — returns the whole
   *                   retained buffer and never reports a gap.
   * @param now_ms  Reference time for age eviction (testing override).
   */
  LogSnapshot snapshot_since(uint32_t since, bool has_since, uint32_t now_ms);
  LogSnapshot snapshot_since(uint32_t since, bool has_since);

  uint32_t session_id() const { return session_id_; }

  /// Test/inspection helper: current number of retained records.
  size_t size();

  /// Accessor used by the static vprintf trampoline.
  static LogBuffer* instance() { return instance_; }

 private:
  static int vprintf_trampoline(const char* format, va_list args);

  /// Drains the handoff queue and appends lines to the buffer. Runs on its own
  /// task so the heap work stays off the logging task's stack.
  static void capture_task(void* arg);

  /// Clamp the formatted length, stamp the time, and hand the item to the
  /// capture task (non-blocking). Drops empty/error results (n <= 0). Runs in
  /// the logging task's context, so it allocates nothing.
  void enqueue_capture(LogQueueItem& item, int n);

  /// Evict records older than max_age_ms_ and beyond max_lines_. Caller holds
  /// the mutex.
  void prune_locked(uint32_t now_ms);

  size_t max_lines_;
  uint32_t max_age_ms_;
  size_t max_line_length_;
  size_t min_headroom_bytes_;

  std::deque<LogRecord> records_;
  uint32_t next_seq_ = 0;
  uint32_t session_id_;

  int (*previous_vprintf_)(const char*, va_list) = nullptr;

  // The capture task is created once in install() and runs for the process
  // lifetime (never torn down), so its handle is not retained.
  QueueHandle_t capture_queue_ = nullptr;

  SemaphoreHandle_t mutex_;
  StaticSemaphore_t mutex_buffer_;

  static LogBuffer* instance_;
};

}  // namespace sensesp

#endif  // SENSESP_SRC_SENSESP_SYSTEM_LOG_BUFFER_H_
