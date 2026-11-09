' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: Device identity, chip information, and reset services for ESP8266 targets.
End Rem
Module ESP8266.System.Device
?esp8266

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Const ChipFeatureEmbeddedFlash:UInt = 1 Shl 0
Const ChipFeatureWiFi:UInt = 1 Shl 1

Const DeviceResetReasonUnknown:Int = 0
Const DeviceResetReasonPowerOn:Int = 1
Const DeviceResetReasonExternal:Int = 2
Const DeviceResetReasonSoftware:Int = 3
Const DeviceResetReasonWatchdog:Int = 4
Const DeviceResetReasonPanic:Int = 5
Const DeviceResetReasonDeepSleep:Int = 6
Const DeviceResetReasonBrownout:Int = 7
Const DeviceResetReasonPowerGlitch:Int = 8
Const DeviceResetReasonCPULockup:Int = 9

Extern "C"
	Rem
	bbdoc: Returns the factory base MAC as twelve uppercase hexadecimal characters.
	End Rem
	Function UniqueDeviceID:String() = "bmx_embedded_unique_device_id"
	Function UniqueDeviceIDBytes:Byte[]() = "bmx_embedded_unique_device_id_bytes"
	Function DeviceResetReason:Int() = "bmx_embedded_device_reset_reason"
	Function DeviceResetReasonNative:Int() = "bmx_esp8266_device_reset_reason_native"
	Function Reboot:Int(delayMilliseconds:UInt = 0) = "bmx_embedded_device_reboot"

	Function ChipModelName:String() = "bmx_esp8266_device_chip_model_name"
	Function ChipRevision:UInt() = "bmx_esp8266_device_chip_revision"
	Function ChipCoreCount:UInt() = "bmx_esp8266_device_chip_cores"
	Function ChipFeatures:UInt() = "bmx_esp8266_device_chip_features"
	Function CPUFrequencyHz:UInt() = "bmx_esp8266_device_cpu_frequency_hz"
	Function NativeFreeHeap:UInt() = "bmx_esp8266_device_free_heap"
	Function NativeMinimumFreeHeap:UInt() = "bmx_esp8266_device_minimum_free_heap"
	Function FlashSizeMap:Int() = "bmx_esp8266_device_flash_size_map"
End Extern

Function DeviceResetReasonName:String(reason:Int)
	Select reason
		Case DeviceResetReasonPowerOn Return "Power on"
		Case DeviceResetReasonExternal Return "External"
		Case DeviceResetReasonSoftware Return "Software"
		Case DeviceResetReasonWatchdog Return "Watchdog"
		Case DeviceResetReasonPanic Return "Panic"
		Case DeviceResetReasonDeepSleep Return "Deep sleep"
		Case DeviceResetReasonBrownout Return "Brownout"
		Case DeviceResetReasonPowerGlitch Return "Power glitch"
		Case DeviceResetReasonCPULockup Return "CPU lockup"
	End Select
	Return "Unknown"
End Function
?
