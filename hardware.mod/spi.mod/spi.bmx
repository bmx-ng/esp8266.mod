' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: ESP8266 facade for the portable embedded SPI controller API.
about: Controller zero maps to the user-accessible HSPI peripheral on the
fixed GPIO12 MISO, GPIO13 MOSI and GPIO14 clock pins. GPIO15 is the board
profile's conventional chip-select pin and remains application-controlled.
End Rem
Module ESP8266.Hardware.SPI
?esp8266

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Import Embedded.Hardware.SPI
?
