#ifndef SENSESP_SRC_SENSESP_SIGNALK_SIGNALK_WS_DELTA_SIZE_H_
#define SENSESP_SRC_SENSESP_SIGNALK_SIGNALK_WS_DELTA_SIZE_H_

#include <cstddef>

namespace sensesp {

/**
 * @brief True if a delta is too large to hand to esp_websocket_client as a
 * single tx chunk, so send_delta drops it to keep the connection alive.
 *
 * Pure and dependency-free (no Arduino, no esp_websocket_client) so it can be
 * unit-tested on the host.
 *
 * The bound is strictly greater-than, and that is deliberate. esp_websocket_client
 * copies up to buffer_size *payload* bytes per transport write; the WebSocket
 * frame header is framed separately, not in tx_buffer. A delta of exactly
 * buffer_size is sent as one chunk with FIN set and never enters the multi-write
 * path. Only a delta longer than buffer_size is split into two or more
 * non-blocking writes, the second of which can short-write and make
 * esp_websocket_client abort the connection. Do not subtract a frame-header
 * margin here: the header does not consume buffer_size.
 */
inline bool sk_delta_exceeds_ws_buffer(size_t delta_length, size_t buffer_size) {
  return delta_length > buffer_size;
}

}  // namespace sensesp

#endif  // SENSESP_SRC_SENSESP_SIGNALK_SIGNALK_WS_DELTA_SIZE_H_
