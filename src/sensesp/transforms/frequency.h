#ifndef SENSESP_TRANSFORMS_FREQUENCY_H_
#define SENSESP_TRANSFORMS_FREQUENCY_H_

#include "sensesp/ui/config_item.h"
#include "transform.h"

namespace sensesp {

/**
 * @brief Transforms its input into frequency (Hz: cycles per second)
 *
 * Divides its input value by the time elapsed since the last reading.
 *
 * @param multiplier Output is multiplied by this parameter before being
 * output, as a way to adjust the scale of the ouput. For example, if
 * you're calculating the frequency of engine revolutions and you're
 * sensing 4 signals per revolution (such as with 4 magnets on the flywheel),
 * set multiplier = 0.25 to convert the total signals into "revolutions".
 *
 * @param config_path Path to configure this transform in the Config UI.
 */
class Frequency : public Transform<int, float> {
 public:
  Frequency(float multiplier = 1, const String& config_path = "");
  virtual void set(const int& input) override;
  virtual bool to_json(JsonObject& root) override;
  virtual bool from_json(const JsonObject& config) override;

 private:
  float multiplier_;
  int ticks_ = 0;
  unsigned int last_update_ = 0;
};

const String ConfigSchema(const Frequency& obj);

}  // namespace sensesp

#endif
