---
layout: default
title: Migrating From Version 1
nav_order: 70
---

# Migration Guides for SensESP Major Versions

## Migrating SensESP Version 2 Projects to Version 3

### Quick Changes for the Impatient

SensESP v3 introduces some changes in the main program initialization and structure.

Update your project's `platformio.ini` file to use the new version of SensESP:

```ini
lib_deps =
  SignalK/SensESP @ ^3.0.0
```

Adjust the build flags in your project's `platformio.ini` file as follows:

```ini
build_flags =
  -D LED_BUILTIN=2
  ; Max (and default) debugging level in Arduino ESP32 Core
  -D CORE_DEBUG_LEVEL=ARDUHAL_LOG_LEVEL_VERBOSE
  ; Arduino Core bug workaround: define the log tag for the Arduino
  ; logging macros.
  -D TAG='"Arduino"'
  ; Use the ESP-IDF logging library - required by SensESP.
  -D USE_ESP_IDF_LOG
```

If you have the following in the beginning of your `setup()` function:

```cpp
#ifndef SERIAL_DEBUG_DISABLED
  SetupSerialDebug(115200);
#endif
```
replace it with:

```cpp
SetupLogging();
```

The `reactesp` namespace is no longer imported. If you have any references to
classes in this namespace, you will need to update them to use the namespace
explicitly.

Additionally, ReactESP classes have been renamed:

- `ReactESP` -> `reactesp::EventLoop`
- `*Reaction` -> `reactesp::*Event`

For example, `ReactESP` class should be referred to as
`reactesp::EventLoop`. In particular, this change probably needs to be made
in your project's `main.cpp` file.

SensESP uses the [ReactESP](https:://github.com/mairas/ReactESP) framework for event-based programming. In previous versions, the ReactESP "app" object had to be instantiated in the main program file. This is no longer the case, and SensESP will take care of this for you. Remove any line such as
```c++
reactesp::ReactESP app;
```
or
```c++
ReactESP app;
```
from your `main.cpp` file. Similarly, the ReactESP main class has been renamed to `EventLoop`. If you want to set up ReactESP events (previously called "reactions"), use the `event_loop()` convenience function to get a pointer to the `EventLoop` object.

For example:

```cpp
event_loop()->onRepeat(
  1000,
  []() { Serial.println("Hello, world!"); }
);
```

SensESP v3 has removed the `Startable` class. In previous versions, you would have `sensesp_app->start();` as the last line in your `setup()` function. This is no longer necessary. If you need to initialize something after the `setup()` function has finished, create a zero-delay event: `event_loop()->onDelay(0, []() { /* your code here */ });`.

Logging initialization has changed. At the beginning of your `setup()` function, remove these lines:

```c++
#ifndef SERIAL_DEBUG_DISABLED
  SetupSerialDebug(115200);
#endif
```
and replace them with this line:
```c++
SetupLogging();
```
In `platformio.ini`, replace the `build_flags` definition with this:
```ini
build_flags =
   -D LED_BUILTIN=2
   ; Max (and default) debugging level in Arduino ESP32 Core
   -D CORE_DEBUG_LEVEL=ARDUHAL_LOG_LEVEL_VERBOSE
   ; Arduino Core bug workaround: define the log tag for the Arduino
   ; logging macros.
   -D TAG='"ARDUINO"'
   ; Use the ESP-IDF logging library - required by SensESP.
   -D USE_ESP_IDF_LOG
```

See the next section how to update your code to the new config item system.

### Exposing Sensors, Transforms, and Outputs to the Web Interface

In previous versions of SensESP, any object inheriting from `Configurable` that had a config path defined, would automatically be added to the web interface. In SensESP v3, this has changed. Now, you need to explicitly expose objects to the web interface. This isdone by calling `ConfigItem` with the object as an argument. For example:

```cpp
auto input_calibration = new Linear(1.0, 0.0, "/input/calibration");

ConfigItem(input_calibration)
    ->set_title("Input Calibration")
    ->set_description("Analog input value adjustment.")
    ->set_sort_order(1100);

analog_input->connect_to(input_calibration);
```

### Logging

Previous SensESP versions logged to the serial port using the `debugX` functions, where `X` is the log level.
This pattern was inherited from the RemoteDebug library.
SensESP v3 has switched to using standard ESP-IDF logging functions.
They allow redirecting log messages to different outputs, which will be used in future SensESP versions to provide logging to the web interface.

To enable logging in SensESP v3, change the `build_flags` in your `platformio.ini` file to include the following:

```ini
build_flags =
   -D LED_BUILTIN=2
   ; Max (and default) debugging level in Arduino ESP32 Core
   -D CORE_DEBUG_LEVEL=ARDUHAL_LOG_LEVEL_VERBOSE
   ; Arduino Core bug workaround: define the log tag for the Arduino
   ; logging macros.
   -D TAG='"ARDUINO"'
   ; Use the ESP-IDF logging library - required by SensESP.
   -D USE_ESP_IDF_LOG
```

The `debugX` functions are still available, but they are now just wrappers around the ESP-IDF logging functions.
In any new code, use the `ESP_LOGX` functions, where `X` is the log level.
These require a tag argument, which is a string that identifies the source of the log message.
You can use any tag you like, but for simple programs, you can use the `__FILE__` macro, which expands to the name of the current file.

The allowed log levels are:

- `NONE`: No log output
- `ERROR`: Critical errors, software module can not recover on its own
- `WARN`: Error conditions from which recovery measures have been taken
- `INFO`: Information messages which describe normal flow of events
- `DEBUG`: Extra information which is not necessary for normal use (values, pointers, sizes, etc).
- `VERBOSE`: Bigger chunks of debugging information, or frequent messages which can potentially flood the output.

Here is an example of how to use the `ESP_LOGX` functions:

```c++
ESP_LOGI(__FILE__, "Initializing NMEA2000");
...
ESP_LOGD(__FILE__, "Sending value %d to fobulator %s", value, fobulator_name);
...
ESP_LOGE(__FILE__, "Failed to initialize NMEA2000");
```

To enable logging in previous SensESP versions, you had to call `SetupSerialDebug(115200)` as the first line in your `setup()` function.
In new SensESP versions, replace this with the new `SetupLogging()` function call to set logging defaults.
The old `SetupSerialDebug` function is still available, but it is now just a wrapper around `SetupLogging`.

It is possible to change the log level for individual tags.
Here is an example of how to set the overall log level to `INFO` and the log level for the `main.cpp` tag to `DEBUG`:

```c++
esp_log_level_set("*", ESP_LOG_INFO);
esp_log_level_set("main.cpp", ESP_LOG_DEBUG);
```

## Migrating SensESP Version 1 Projects to Version 2

SensESP version 2 has a number of backwards-incompatible changes compared to the previous version.
Most of the changes are trivial, while some others require a bit of more work to update the code.

This document walks through the most important changes, and explains how to update your project to the new version.

### ESP8266 Support Removed

If your project uses ESP8266 hardware, you will either have to update to an ESP32 device, *or* you can keep using SensESP version 1. To peg your project to SensESP v1, change the SensESP dependency in your project's `platformio.ini` file `lib_deps` section to this:

```ini
lib_deps =
    SignalK/SensESP @ ^1.0.8
```

### Main Program Structure

#### Setup and Loop Functions

SensESP builds on [ReactESP](https://github.com/mairas/ReactESP), which is an event-based framework for developing ESP32 firmware.
Previous versions of ReactESP defined the Arduino Framework default `setup()` and `loop()` functions internally and relied on a lambda function for initializing the program and defining the top-level functionality:

```c++
ReactESP app([] () {
  pinMode(LED_PIN, OUTPUT);
  app.onRepeat(400, [] () {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  });
});
```

This approach emphasized the event-based nature of ReactESP but also looked alien to newcomers expecting a more traditional program structure.
Additionally, some use cases might benefit from being able to tweak the details of the `setup()` and `loop()` functions, which is not possible with the lambda function approach.

Therefore, ReactESP v2 was updated to explicitly define the `setup()` and `loop()` functions.
To update your code, declare a default ReactESP app object and define the `setup()` and `loop()` functions in the `main.cpp` file.
Then, move the contents of the lambda function into the `setup()` function. `loop()` should only have a call to `app.tick()`:

```c++
ReactESP app;

void setup() {
  pinMode(LED_PIN, OUTPUT);
  app.onRepeat(400, [] () {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  });
}

void loop() { app.tick(); }
```

#### Namespace Usage

In projects with a lot of dependencies, it is common that some upstream library exports some very generic symbol names, which then causes conflicts or hard-to-debug issues in the code being developed. The standard C++ approach to mitigate these issues is to use a namespace.

All internal SensESP code is now wrapped in a `sensesp` namespace. To account for this, add a `using namespace sensesp;` statement right after the `#include` statements in your `main.cpp` file:

```c++
#include "..."

using namespace sensesp;

ReactESP app;

void setup() {
  ...
}

void loop() { app.tick(); }
```

If you're developing or maintaining an add-on library, you should wrap the library code in the `sensesp` namespace:

```c++
// includes should remain outside the namespace
#include "..."

namespace sensesp {
// All your code here
}
```

### External Sensors

All Sensor classes requiring external libraries have been removed.
Reducing the number of external dependencies improves code stability and improves build times.

Some Sensors are now available as [add-on libraries](../../additional_resources).
Most, however, have been removed in favor of a more generic approach, namely the [`RepeatSensor`](https://signalk.org/SensESP/generated/docs/classsensesp_1_1_repeat_sensor.html) class.
The `RepeatSensor` class allows you to easily interface any external hardware sensor libraries with SensESP.
See the `RepeatSensor` tutorials ([part 1](../tutorials/bmp280), [part 2](../tutorials/bmp280)) for more details.

### Renamed Classes and Types

Type-specific Consumer and Producer class names have been renamed to more closely match the native C++ types.

All class names including `Numeric` have been renamed to `Float`.
For example, `SKOutputNumeric` has been renamed to `SKOutputFloat` and `SKNumericListener` has been renamed to `FloatSKListener`.
In the latter case, the name has also been modified to reflect that float is the input type for the listener.

Similarly, names with other types have been renamed to more closely match the standard C++ builtin type names. For example, `*Boolean*` is now `*Bool*`, and `*Integer*` is now `*Int*`.

To better reflect the intent and the functionality, the `Enable` class has been renamed to `Startable`.

### Class Interface Changes

Some class public interfaces have been changed.

The [`DigitalInputState`](https://signalk.org/SensESP/generated/docs/classsensesp_1_1_digital_input_state.html) constructor no longer accepts the `interrupt_type` argument because that class never used interrupts.

The [`DigitalInputChange`](https://signalk.org/SensESP/generated/docs/classsensesp_1_1_digital_input_change.html) implementation has been simplified and the constructor no longer requres the `read_delay` argument.

### System Info Sensors

SensESP v1 had so called "standard sensors" that transmit information on the operation of the device: free memory, number of event loop executions per second, device IP address, and so on.
The standard sensors were initialized using a SensESP constructor or builder bitfield argument:

```c++
sensesp_app = builder.set_standard_sensors(UPTIME)
                     ->get_app();
```

The standard sensors have been renamed to system info sensors and they are initialized by regular builder methods:

```c++
sensesp_app = builder.enable_free_mem_sensor()
                     ->enable_uptime_sensor()
                     ->get_app();
```

### Remote Debugger Disabled

The Remote Debugger allows you to connect to the device over telnet and view the log messages and even reset the device, all without a USB cable connection.
Even though this is a neat and useful feature, it was not widely known and uses *a lot* of memory.
And, it was a gaping security hole.

The Remote Debugger has now been disabled by default.

To enable the Remote Debugger, add the following line to your `platformio.ini` file `build_flags`:

```ini
build_flags =
    ...
    -D REMOTE_DEBUG
```

After rebuilding and uploading, your device should listen to the default telnet port (23) on the device's local network.

If you want to disable all debugging output whatsoever, add the following:

```ini
build_flags =
    ...
    -D DEBUG_DISABLED
```

### Over-The-Air (OTA) Firmware Updates

OTA firmware updates have been supported already for a long time.
To improve security, OTA updates are now enabled only if an OTA password is defined in the App builder:

```c++
sensesp_app = builder.enable_free_mem_sensor()
                     ->enable_ota("my_password")
                     ->get_app();
```
