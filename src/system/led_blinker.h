#ifndef _led_blinker_H_
#define _led_blinker_H_

#include <ReactESP.h>

#include "net/networking.h"
#include "net/ws_client.h"

class LedBlinker : public ValueConsumer<WifiState>,
                   public ValueConsumer<WSConnectionState>,
                   public ValueConsumer<int> {
 private:
  int current_state = 0;
  int pin = 0;
  bool enabled = true;
  int ws_connected_interval;
  int wifi_connected_interval;
  int offline_interval;
  RepeatReaction* blinker = nullptr;
  void remove_blinker();

 protected:
  void set_state(int new_state);

 public:
  void set_wifi_connected();
  void set_wifi_disconnected();
  void set_server_connected();
  inline void set_server_disconnected() { set_wifi_connected(); }
  void flip();
  LedBlinker(int pin, bool enabled, int ws_connected_interval = 100,
             int wifi_connected_interval = 1000, int offline_interval = 2000);
  // ValueConsumer interface for ValueConsumer<WifiState> (Networking object
  // state updates)
  void set_input(WifiState new_value, uint8_t input_channel = 0) override;
  // ValueConsumer interface for ValueConsumer<WSConnectionState>
  // (WSClient object state updates)
  void set_input(WSConnectionState new_value, uint8_t input_channel = 0) override;
  // ValueConsumer interface for ValueConsumer<int> (delta count producer
  // updates)
  void set_input(int new_value, uint8_t input_channel = 0) override;
};

#endif
