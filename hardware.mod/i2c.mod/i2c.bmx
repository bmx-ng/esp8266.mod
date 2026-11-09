' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: ESP8266 facade for the portable embedded I2C master API.
about: The ESP8266 RTOS SDK supplies one software I2C master. Its transfer
timing is fixed by the SDK; requests for standard-mode operation report the
actual 100 kHz nominal rate and unsupported faster rates are rejected.
End Rem
Module ESP8266.Hardware.I2C
?esp8266

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Import Embedded.Hardware.I2C
?
