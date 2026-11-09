# BlitzMax ESP8266 modules

This module collection adds the legacy ESP8266 as a BlitzMax embedded target.
It uses Espressif's ESP8266 RTOS SDK 3.4 and the `xtensa-lx106-elf` toolchain;
it is separate from the ESP32 target and ESP-IDF.

The first supported profile is `d1_mini_esp12f`, for generic D1 Mini carrier
boards using an ESP-12F module with 4 MiB flash.

## Requirements

Install [ESP8266 RTOS SDK 3.4](https://docs.espressif.com/projects/esp8266-rtos-sdk/en/latest/)
and Espressif's `xtensa-lx106-elf` toolchain. This is not ESP-IDF: an existing
ESP32 setup does not include the ESP8266 SDK or compiler. The SDK also needs a
compatible Python environment containing its `requirements.txt` packages.

On Apple Silicon, Espressif's final macOS LX106 toolchain is an Intel binary,
so Rosetta 2 is required.

## Build and upload

The board profile selects both `esp8266` and `xtensa`, so a normal build is:

```sh
bmk makeapp -r -board d1_mini_esp12f examples/hello_world.bmx
```

Add `-x` to upload to the configured or automatically detected serial port.
The SDK console defaults to 74880 baud.

`-heap auto` provides a conservative 12 KiB arena for BlitzMax managed
objects while retaining SRAM for FreeRTOS, Wi-Fi, and native SDK services.
Explicit values from 1 KiB through 32 KiB are accepted, for example
`-heap 20KiB`.

Inspect an attached board, or the static profile, with:

```sh
bmk deviceinfo -l esp8266 -board d1_mini_esp12f
bmk boardinfo -l esp8266 -board d1_mini_esp12f
```

## Tool configuration

Tool paths may be supplied in `custom.bmk`:

```text
#addoption esp8266.idf "/path/to/ESP8266_RTOS_SDK"
#addoption esp8266.tools "/path/to/.espressif"
#addoption esp8266.python "/path/to/python-or-venv"
#addoption esp8266.toolchain "/path/to/xtensa-lx106-elf"
#addoption esp8266.port "/dev/ttyUSB0"
```

Environment equivalents are `ESP8266_RTOS_SDK_PATH`,
`ESP8266_TOOLS_PATH`, `ESP8266_PYTHON`, `ESP8266_TOOLCHAIN_PATH`, and
`ESP8266_PORT`.

The default layout is:

```text
~/.espressif/esp8266/v3.4/ESP8266_RTOS_SDK
~/.espressif/esp8266/python_env/<environment>
~/.espressif/tools/xtensa-lx106-elf/<version>
```

Configured Python and toolchain values may name either their root directories
or the actual executable-containing directories.

## Current scope

The target currently provides the precise embedded collector, managed objects,
arrays and strings, timing/delay services, standard console output, GPIO,
board profiles, and the complete `bmk` build/upload workflow. Hardware and
networking modules will be layered onto this base rather than pretending
ESP8266 is an ESP32 variant.

GPIO is available through both `Embedded.Hardware.GPIO` and the target-specific
`ESP8266.Hardware.GPIO` module. The portable API exposes separate capability
queries for pull-up, pull-down, drive-strength configuration, and interrupts.
This matters on ESP8266: GPIO6–11 are occupied by flash, ordinary GPIOs provide
pull-up only, and GPIO16 provides pull-down but no GPIO interrupt.

Managed GPIO interrupt delivery is available through
`ESP8266.Hardware.GPIOEvents` (or the portable
`Embedded.Hardware.GPIOEvents`). Interrupts are timestamped in native code and
deferred through `BRL.EventQueue`; managed objects are never touched directly
from the interrupt handler.

`tests/managed_allocation.bmx` is the initial on-device regression covering
objects, arrays, strings, reclamation, and console output in the default 12 KiB
managed arena. `tests/gpio_led.bmx` and `tests/gpio_capabilities.bmx` cover the
active-low onboard LED and the ESP8266-specific pin limitations.
`tests/gpio_events_loopback.bmx` verifies native edge capture and managed event
delivery with D1 connected to D2.
