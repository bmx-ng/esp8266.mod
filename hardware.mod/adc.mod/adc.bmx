' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: Analogue-to-digital conversion for ESP8266 targets.
about: ESP8266 has one 10-bit external analogue input named A0. It is represented
as pin 17 so it cannot be confused with GPIO0 through GPIO16. The raw electrical
range depends on the module or carrier board; many bare ESP8266 modules accept a
maximum of approximately 1 V while some development boards add a divider.
End Rem
Module ESP8266.Hardware.ADC
?esp8266

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Const ADCExternalPin:UInt = 17
Const ADCResolutionBits:UInt = 10
Const ADCMaximumValue:UInt = 1023

Extern "C"
	Function ADCIsValidPin:Int(pin:UInt) = "bmx_embedded_adc_is_valid_pin"
	Function ADCInitPin:Int(pin:UInt) = "bmx_embedded_adc_init_pin"
	Function ADCDeinitPin:Int(pin:UInt) = "bmx_embedded_adc_deinit_pin"
	Function ADCReadRaw:Int(pin:UInt, value:UInt Var) = "bmx_embedded_adc_read_raw"
	Function ADCResolutionBitsForPin:UInt(pin:UInt) = "bmx_embedded_adc_resolution_bits"
	Function ADCMaximumValueForPin:UInt(pin:UInt) = "bmx_embedded_adc_maximum_value"
	Function ADCInitialized:Int() = "bmx_esp8266_adc_initialized"
End Extern
?
