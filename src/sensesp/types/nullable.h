#ifndef SENSESP_SRC_SENSESP_TYPES_NULLABLE_H_
#define SENSESP_SRC_SENSESP_TYPES_NULLABLE_H_

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
 * bool has no spare value to reserve as a sentinel, so Nullable<bool> is an
 * explicit specialization (below) that tracks validity with a separate flag.
 */
template <typename T>
class Nullable {
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

/**
 * @brief Validity-flag specialization of Nullable for bool.
 *
 * bool has no spare value to reserve as an invalid sentinel, so unlike the
 * primary template this specialization tracks validity with an explicit flag:
 * `true`, `false`, and invalid are all distinct. A default-constructed
 * Nullable<bool> is invalid (no value yet); construct from a bool for a valid
 * value, or use invalid() for the missing state.
 *
 * invalid() returns a Nullable<bool> instance (not a bare bool) because there
 * is no sentinel bool value to hand back; the only generic caller,
 * RepeatExpiring::repeat_function(), accepts that directly.
 */
template <>
class Nullable<bool> {
  public:
    Nullable() : value_{false}, valid_{false} {}
    Nullable(bool value) : value_{value}, valid_{true} {}
    Nullable<bool>& operator=(bool value) {
      value_ = value;
      valid_ = true;
      return *this;
    }
    Nullable<bool>& operator=(const Nullable<bool>& other) = default;
    operator bool() const {
      return value_;
    }

    bool is_valid() const {
      return valid_;
    }

    bool* ptr() {
      return &value_;
    }

    static Nullable<bool> invalid() {
      return Nullable<bool>();
    }

    bool value() const {
      return value_;
    }

  private:
    bool value_;
    bool valid_;
};

typedef Nullable<int> NullableInt;
typedef Nullable<float> NullableFloat;
typedef Nullable<double> NullableDouble;
typedef Nullable<bool> NullableBool;

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
