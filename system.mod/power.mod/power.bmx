' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: Low-power sleep and wake services for ESP8266 targets.
about: Returning operations use the SDK's light sleep. Deep sleep restarts the
application and requires GPIO16/D0 to be connected to RST for timed wakeup on
typical ESP-12 development boards.
End Rem
Module ESP8266.System.Power
?esp8266

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Const PowerCapabilityIdle:UInt = $01
Const PowerCapabilitySleepInterrupt:UInt = $02
Const PowerCapabilitySleepTimer:UInt = $04
Const PowerCapabilityDormantGPIO:UInt = $08
Const PowerCapabilityDormantTimer:UInt = $10
Const PowerCapabilityLowLeakagePins:UInt = $20

Const ESP8266DeepSleepRFDefault:Int = 0
Const ESP8266DeepSleepRFCalibrate:Int = 1
Const ESP8266DeepSleepRFNoCalibration:Int = 2
Const ESP8266DeepSleepRFDisabled:Int = 4

Enum EPowerResult:Int
	Success = 0
	Error = -1
	Timeout = -2
	NotPermitted = -4
	InvalidArgument = -5
	InsufficientResources = -9
	InvalidState = -12
	PreconditionNotMet = -14
	InvalidData = -16
	Unavailable = -17
	ResourceInUse = -21
End Enum

Extern "C"
	Function PowerCapabilities:UInt() = "bmx_embedded_power_capabilities"
	Function PowerIdle() = "bmx_embedded_power_idle"
	Function LowPowerSleepUntilInterrupt:EPowerResult() = "bmx_embedded_power_sleep_until_interrupt"
	Function LowPowerSleep:EPowerResult(milliseconds:UInt, exclusive:Int = True) = "bmx_embedded_power_sleep_for_ms"
	Function DormantSleep:EPowerResult(milliseconds:UInt) = "bmx_embedded_power_dormant_for_ms"
	Function DormantSleepUntilGPIO:EPowerResult(pin:UInt, edge:Int, high:Int) = "bmx_embedded_power_dormant_until_gpio"
	Function SetUnusedPinsLowLeakage:EPowerResult(excludeMask:ULong = 0) = "bmx_embedded_power_set_unused_pins_low_leakage"

	Rem
	bbdoc: Returns from light sleep when pin reaches the requested logic level.
	about: GPIO0 through GPIO15 are supported. The wake source is disabled before
	this function returns. Wi-Fi must be stopped before entering light sleep.
	End Rem
	Function LowPowerSleepUntilGPIO:EPowerResult(pin:UInt, high:Int) = "bmx_esp8266_power_sleep_until_gpio"

	Rem
	bbdoc: Enters deep sleep and restarts after approximately milliseconds.
	about: A successful call does not return. DeviceResetReason reports deep sleep
	on the next boot. Zero is rejected to prevent an accidental indefinite sleep.
	Timed wake requires GPIO16/D0 to be wired to RST on ESP-12 boards.
	End Rem
	Function DeepSleepForMilliseconds:EPowerResult(milliseconds:ULong) = "bmx_esp8266_power_deep_sleep_for_ms"

	Rem
	bbdoc: Selects the radio-calibration policy used after the next deep-sleep wake.
	End Rem
	Function DeepSleepSetRFOption:EPowerResult(option:Int) = "bmx_esp8266_power_deep_sleep_set_rf_option"
End Extern

Function PowerSupports:Int(capabilities:UInt)
	Return (PowerCapabilities() & capabilities) = capabilities
End Function
?
