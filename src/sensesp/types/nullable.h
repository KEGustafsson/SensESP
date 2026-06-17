#ifndef SENSESP_SRC_SENSESP_TYPES_NULLABLE_H_
#define SENSESP_SRC_SENSESP_TYPES_NULLABLE_H_

#include <type_traits>

#include "ArduinoJson.h"

namespace sensesp {

/**
 * @brief Template class that supports a special invalid magic value for a type.
 *
 * Each specialization defines an invalid sentinel (see nullable.cpp; e.g. float
 * -> -1e9, uint8_t -> 0xff), matching the NMEA 2000 "missing data" values. A
 * value is invalid only when it equals that sentinel. A default-constructed
 * Nullable holds T{} (0), so it is valid for types whose sentinel differs from
 * 0; use Nullable<T>::invalid() to obtain the invalid value explicitly.
 *
 * Nullable<bool> is intentionally unsupported: bool has only two values, so
 * there is no spare bit pattern to reserve as an invalid sentinel without
 * making one of `true`/`false` indistinguishable from "missing". Use a plain
 * bool, or a wider type if you need a distinct invalid state.
 */
template <typename T>
class Nullable {
  static_assert(
      !std::is_same<T, bool>::value,
      "Nullable<bool> is not supported: bool has no spare sentinel value. Use a "
      "plain bool, or a wider type if a distinct invalid state is needed.");

  public:
    Nullable() : value_{} {}
    Nullable(T value) : value_{value} {}
    Nullable<T>& operator=(T& value) {
      value_ = value;
      return *this;
    }
    Nullable<T>& operator=(const Nullable<T>& other) {
      value_ = other.value_;
      return *this;
    }
    operator T() const {
      return value_;
    }

    bool is_valid() const {
      return value_ != invalid_value_;
    }

    T* ptr() {
      return &value_;
    }

    static T invalid() {
      return invalid_value_;
    }

    T value() const {
      return value_;
    }

  private:
    T value_;
    static T invalid_value_;
};

typedef Nullable<int> NullableInt;
typedef Nullable<float> NullableFloat;
typedef Nullable<double> NullableDouble;

template <typename T>
void convertFromJson(JsonVariantConst src, Nullable<T> &dst) {
  if (src.isNull()) {
    dst = Nullable<T>::invalid();
  } else {
    dst = src.as<T>();
  }
}

template <typename T>
void convertToJson(const Nullable<T> &src, JsonVariant dst) {
  if (src.is_valid()) {
    dst.set(src.value());
  } else {
    dst.clear();
  }
}

}  // namespace sensesp

#endif  // SENSESP_SRC_SENSESP_TYPES_NULLABLE_H_
