' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: Monotonic time and sleep operations for ESP8266 targets.
End Rem
Module ESP8266.System.Time
?esp8266

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Extern "C"
	Rem
	bbdoc: Returns the current processor-clock frequency in hertz.
	End Rem
	Function SystemClockFrequency:UInt() = "bmx_esp8266_system_clock_hz"

	Function MonotonicMicroseconds:ULong() = "bmx_embedded_time_microseconds"
	Function MonotonicMilliseconds:ULong() = "bmx_embedded_time_milliseconds"
	Function SleepMilliseconds(milliseconds:UInt) = "bmx_embedded_sleep_milliseconds"
	Function SleepMicroseconds(microseconds:ULong) = "bmx_embedded_sleep_microseconds"
End Extern
?
