' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: Application watchdog services for ESP8266 targets.
about: A one-shot FreeRTOS timer tracks the requested application deadline.
Expiry deliberately hands control to the ESP8266 hardware watchdog, retaining
a genuine watchdog reset reason. The SDK's independent system watchdog remains
active throughout.
End Rem
Module ESP8266.System.Watchdog
?esp8266

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Import Embedded.Hardware.Watchdog
?
