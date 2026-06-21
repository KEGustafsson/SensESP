#include "sensesp/system/log_buffer.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_random.h>
#include <esp_timer.h>

#include <cstdio>
#include <cstring>

#ifndef USE_ESP_IDF_LOG
#warning \
    "USE_ESP_IDF_LOG is not defined: Arduino-ESP32 reroutes ESP_LOGx to " \
    "log_printf, which bypasses esp_log_set_vprintf. The web log viewer will " \
    "capture nothing. Build with -D USE_ESP_IDF_LOG."
#endif

namespace sensesp {

LogBuffer* LogBuffer::instance_ = nullptr;

LogBuffer::LogBuffer(size_t max_lines, uint32_t max_age_ms,
                     size_t max_line_length, size_t min_headroom_bytes)
    : max_lines_(max_lines),
      max_age_ms_(max_age_ms),
      max_line_length_(max_line_length),
      min_headroom_bytes_(min_headroom_bytes),
      session_id_(0) {
  mutex_ = xSemaphoreCreateMutexStatic(&mutex_buffer_);
}

void LogBuffer::install() {
  // Idempotent: a second call would set previous_vprintf_ to the trampoline
  // itself (esp_log_set_vprintf returns the currently installed handler),
  // turning the fallback paths into unbounded recursion. After the first
  // install previous_vprintf_ is the (always non-null) prior handler.
  if (previous_vprintf_ != nullptr) {
    return;
  }

  // Draw the per-boot session id here rather than in the constructor: the hook
  // is installed during setup(), past early static init, and mixing in the boot
  // timer makes a repeated value across reboots unlikely even before the RF
  // subsystem seeds the RNG. The client also falls back to a cursor reset.
  session_id_ = esp_random() ^ static_cast<uint32_t>(esp_timer_get_time());

  // Stand up the handoff queue and capture task before the hook goes live, so
  // the first captured line already has somewhere to go. If the task can't be
  // created, drop the queue too: a non-null queue with no drainer would fill
  // after kLogCaptureQueueDepth lines and silently lose all capture for the
  // rest of the uptime. Leaving capture_queue_ null makes the trampoline fall
  // back to the chained handler (console preserved, capture disabled).
  capture_queue_ = xQueueCreate(kLogCaptureQueueDepth, sizeof(LogQueueItem));
  if (capture_queue_ != nullptr &&
      xTaskCreate(&LogBuffer::capture_task, "LogCapture",
                  kLogCaptureTaskStackSize, this, kLogCaptureTaskPriority,
                  nullptr) != pdPASS) {
    vQueueDelete(capture_queue_);
    capture_queue_ = nullptr;
  }

  instance_ = this;
  previous_vprintf_ = esp_log_set_vprintf(&LogBuffer::vprintf_trampoline);
}

void LogBuffer::enqueue_capture(LogQueueItem& item, int n) {
  // Drop empty/error results; push_line would discard them anyway.
  if (n <= 0) {
    return;
  }
  item.length = (static_cast<size_t>(n) < sizeof(item.text))
                    ? static_cast<uint16_t>(n)
                    : static_cast<uint16_t>(sizeof(item.text) - 1);
  item.timestamp_ms = esp_log_timestamp();
  // Non-blocking: drop the line rather than stall the logger if the capture
  // task is behind.
  xQueueSend(capture_queue_, &item, 0);
}

void LogBuffer::capture_task(void* arg) {
  auto* self = static_cast<LogBuffer*>(arg);
  LogQueueItem item;
  while (true) {
    if (xQueueReceive(self->capture_queue_, &item, portMAX_DELAY) == pdTRUE) {
      // The heap-allocating work (ANSI strip, std::string, deque) happens here,
      // on this task's stack, not the logging task's.
      self->push_line(item.text, item.length, item.timestamp_ms);
    }
  }
}

int LogBuffer::vprintf_trampoline(const char* format, va_list args) {
  LogBuffer* inst = instance_;

#if !defined(ESP_LOG_VERSION) || ESP_LOG_VERSION == 1
  // ESP-IDF log V1: the hook is called once per line with the complete format
  // string and va_list, so a single vsnprintf reproduces the exact console
  // bytes. We format ONCE on the caller stack, emit the bytes ourselves, and
  // hand the same buffer to the capture task -- avoiding the chained handler's
  // second full format pass (newlib vfprintf), which is the dominant stack cost
  // and what overflows small logging tasks.
  //
  // ISR / pre-install context: stdio and the capture queue are unsafe, so fall
  // back to the chained handler (a full vfprintf). Rare path.
  if (inst == nullptr || inst->capture_queue_ == nullptr ||
      xPortInIsrContext()) {
    if (inst != nullptr && inst->previous_vprintf_ != nullptr) {
      return inst->previous_vprintf_(format, args);
    }
    return 0;
  }

  LogQueueItem item;
  va_list args_copy;
  va_copy(args_copy, args);
  int n = vsnprintf(item.text, sizeof(item.text), format, args);
  if (n < 0) {
    va_end(args_copy);
    return n;
  }

  int ret = n;
  if (static_cast<size_t>(n) >= sizeof(item.text)) {
    // The line is longer than our buffer. Self-emitting the truncated copy
    // would drop the trailing newline and run console lines together, so emit
    // the full line via the chained handler -- a second format pass. This
    // re-incurs the caller-stack cost the lean path avoids, so it relies on
    // over-length lines coming from roomy-stack loggers; the small-stack tasks
    // this protects log short lines and never reach here.
    if (inst->previous_vprintf_ != nullptr) {
      ret = inst->previous_vprintf_(format, args_copy);
    }
  } else {
    // Fits: format once, emit the bytes ourselves through stdout (not a fixed
    // peripheral) so output follows the active console -- UART and
    // USB-CDC/JTAG alike. newlib FILE locking serializes concurrent writers,
    // as it did for the chained vfprintf.
    fwrite(item.text, 1, static_cast<size_t>(n), stdout);
  }
  va_end(args_copy);

  // Hand the (possibly truncated) line to the capture task; the heap work
  // runs on its stack, not the caller's.
  inst->enqueue_capture(item, n);
  return ret;
#else
  // ESP-IDF log V2 calls the hook multiple times per line with fragments, so a
  // single format-and-emit would mangle output. Fall back to chaining the
  // previous handler for console output and a second format for capture.
  int ret = 0;
  if (inst != nullptr && inst->previous_vprintf_ != nullptr) {
    va_list copy;
    va_copy(copy, args);
    ret = inst->previous_vprintf_(format, copy);
    va_end(copy);
  }
  if (inst != nullptr && inst->capture_queue_ != nullptr &&
      !xPortInIsrContext()) {
    LogQueueItem item;
    va_list copy;
    va_copy(copy, args);
    int n = vsnprintf(item.text, sizeof(item.text), format, copy);
    va_end(copy);
    inst->enqueue_capture(item, n);
  }
  return ret;
#endif
}

namespace {
// Copy text with ANSI/VT100 escape sequences removed. ESP_LOG wraps each line
// in color codes when CONFIG_LOG_COLORS is enabled; the raw ESC byte (0x1b) is
// a control character that ArduinoJson does not escape, so it would otherwise
// reach the /api/log response unescaped and make the JSON unparseable in the
// browser. A CSI sequence is ESC '[' then parameter/intermediate bytes up to a
// final byte in 0x40-0x7e; a lone ESC is simply dropped.
std::string strip_ansi(const char* text, size_t length) {
  std::string out;
  out.reserve(length);
  size_t i = 0;
  while (i < length) {
    if (text[i] == '\x1b') {
      i++;  // drop ESC
      if (i < length && text[i] == '[') {
        i++;  // drop '['
        while (i < length) {
          unsigned char c = static_cast<unsigned char>(text[i]);
          i++;
          if (c >= 0x40 && c <= 0x7e) {
            break;  // final byte, end of sequence
          }
        }
      }
      continue;
    }
    out.push_back(text[i]);
    i++;
  }
  return out;
}
}  // namespace

void LogBuffer::push_line(const char* text, size_t length, uint32_t now_ms) {
  // Strip trailing CR/LF so each record is one display line.
  while (length > 0 && (text[length - 1] == '\n' || text[length - 1] == '\r')) {
    length--;
  }
  if (length == 0) {
    return;
  }

  // Never let log capture be the allocation that exhausts the heap. strip_ansi()
  // and the records_ deque below both allocate; with C++ exceptions disabled a
  // failed allocation throws std::bad_alloc, which routes to abort() and reboots
  // the device -- so under memory pressure the logger would crash the device
  // while capturing the very errors that report the pressure. Drop the line
  // instead, mirroring the queue-full drop policy in enqueue_capture().
  if (heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < min_headroom_bytes_) {
    return;
  }

  // Remove ANSI color escapes so the buffered copy served over /api/log is
  // plain, JSON-safe text. The console output above keeps its colors.
  std::string line = strip_ansi(text, length);
  if (line.empty()) {
    return;
  }
  if (line.size() > max_line_length_) {
    line.resize(max_line_length_);
  }

  xSemaphoreTake(mutex_, portMAX_DELAY);
  prune_locked(now_ms);
  records_.push_back({next_seq_, now_ms, std::move(line)});
  next_seq_++;
  // Enforce the count cap after inserting the new record.
  while (records_.size() > max_lines_) {
    records_.pop_front();
  }
  xSemaphoreGive(mutex_);
}

void LogBuffer::prune_locked(uint32_t now_ms) {
  while (!records_.empty()) {
    const LogRecord& front = records_.front();
    if (now_ms >= front.timestamp_ms &&
        now_ms - front.timestamp_ms > max_age_ms_) {
      records_.pop_front();
    } else {
      break;
    }
  }
}

LogSnapshot LogBuffer::snapshot_since(uint32_t since, bool has_since,
                                      uint32_t now_ms) {
  LogSnapshot snapshot;
  snapshot.session_id = session_id_;
  snapshot.gap = false;

  xSemaphoreTake(mutex_, portMAX_DELAY);
  prune_locked(now_ms);
  snapshot.next = next_seq_;

  if (!has_since) {
    // Initial load: return the whole retained buffer, never a gap.
    for (const auto& record : records_) {
      snapshot.lines.push_back(record.text);
    }
  } else {
    if (records_.empty()) {
      // Lines existed between the client's cursor and now but were all
      // evicted. (since >= next_seq_ is the caught-up or post-reboot case.)
      snapshot.gap = since < next_seq_;
    } else if (records_.front().seq > since) {
      // The line the client expected next was evicted before it was read.
      snapshot.gap = true;
    }
    for (const auto& record : records_) {
      if (record.seq >= since) {
        snapshot.lines.push_back(record.text);
      }
    }
  }
  xSemaphoreGive(mutex_);

  return snapshot;
}

LogSnapshot LogBuffer::snapshot_since(uint32_t since, bool has_since) {
  return snapshot_since(since, has_since, esp_log_timestamp());
}

size_t LogBuffer::size() {
  xSemaphoreTake(mutex_, portMAX_DELAY);
  size_t count = records_.size();
  xSemaphoreGive(mutex_);
  return count;
}

}  // namespace sensesp
