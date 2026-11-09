' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: Full-duplex hardware UART support for ESP8266 targets.
about: The portable controller is UART0. It normally uses TX/GPIO1 and
RX/GPIO3, and can be swapped to D8/GPIO15 and D7/GPIO13. UART1 is deliberately
not presented as a portable controller because the ESP8266 exposes TX only.
End Rem
Module ESP8266.Hardware.UART
?esp8266

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Import Embedded.Hardware.UART

Extern "C"
	Function UARTUseSwappedPins:Int(enabled:Int) = "bmx_esp8266_uart_use_swapped_pins"
	Function UARTUsesSwappedPins:Int() = "bmx_esp8266_uart_uses_swapped_pins"
End Extern
?
