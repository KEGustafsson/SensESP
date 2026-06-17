#ifndef SENSESP_SIGNALK_SIGNALK_PUT_REQUEST_LISTENER_H_
#define SENSESP_SIGNALK_SIGNALK_PUT_REQUEST_LISTENER_H_

#include "sensesp.h"

#include <ArduinoJson.h>
#include <set>

#include "sensesp/system/observable.h"
#include "sensesp/system/valueproducer.h"

namespace sensesp {

/**
 * @brief An Obervable class that listens for Signal K PUT
 * requests coming over the websocket connection and notifies
 * observers of the change.
 * @see SKPutRequestListener
 */
class SKPutListener : virtual public Observable {
 public:
  /**
   * The constructor
   * @param sk_path The Signal K path that identifies the signal k path
   * this particular listener responds to
   */
  SKPutListener(const String& sk_path);

  /**
   * Unregisters this listener from the static registry so the registry never
   * holds a dangling pointer. Takes the SKListener semaphore because the WS
   * client task iterates this registry under that same lock.
   */
  virtual ~SKPutListener();

  String& get_sk_path() { return sk_path; }

  virtual void parse_value(const JsonObject& put) = 0;

  static const std::vector<SKPutListener*>& get_listeners() {
    return listeners_;
  }

  /**
   * Empties the listener registry. Intended for clean app restart and test
   * isolation; not for normal runtime use. Takes the SKListener semaphore.
   */
  static void clear_registry();

 protected:
  String sk_path{};

 private:
  static std::vector<SKPutListener*> listeners_;
  int listen_delay{};
};

/**
 * @brief  An object that listens for PUT requests to come in
 * for the specified SignalK path. The value is then emitted for
 * further processing.
 */
template <class T>
class SKPutRequestListener : public SKPutListener, public ValueProducer<T> {
 public:
  SKPutRequestListener(String sk_path) : SKPutListener(sk_path) {
    if (sk_path == "") {
      ESP_LOGE(
          __FILENAME__,
          "SKPutRequestListener: User has provided no sk_path to respond to.");
    }
  }

  void parse_value(const JsonObject& put) override {
    this->emit(put["value"].as<T>());
  }
};

typedef SKPutRequestListener<float> FloatSKPutRequestListener;
typedef SKPutRequestListener<int> IntSKPutRequestListener;
typedef SKPutRequestListener<bool> BoolSKPutRequestListener;
typedef SKPutRequestListener<String> StringSKPutRequestListener;

}  // namespace sensesp

#endif
