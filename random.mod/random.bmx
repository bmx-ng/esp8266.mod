' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: ESP8266 hardware random-number generator.
End Rem
Module ESP8266.Random
?esp8266

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Import Embedded.Random

Rem
bbdoc: Compatibility name for the shared device-backed random generator.
about: Values come from the ESP8266 hardware RNG. Instances cannot be seeded
or serialized.
End Rem
Type TESP8266Random Extends TEmbeddedRandom
	Method GetName:String() Override
		Return "ESP8266"
	End Method
End Type

Private
Type TESP8266RandomFactory Extends TRandomFactory
	Method New()
		Super.New()
		Init()
	End Method

	Method GetName:String() Override
		Return "ESP8266"
	End Method

	Method Create:TRandom(seed:Int) Override
		Return New TESP8266Random
	End Method

	Method Create:TRandom() Override
		Return New TESP8266Random
	End Method

	Method DeserializeState:TRandom(data:String) Override
		Return Null
	End Method
End Type
Public

Function ESP8266RandomUInt:UInt()
	Return EmbeddedRandomUInt()
End Function

Function ESP8266RandomULong:ULong()
	Return EmbeddedRandomULong()
End Function

Function ESP8266FillRandom:Int(buffer:Byte Ptr, length:Int)
	Return EmbeddedFillRandom(buffer, length)
End Function

New TESP8266RandomFactory
?
