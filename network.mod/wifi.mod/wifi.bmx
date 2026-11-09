' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: ESP8266 wireless LAN support.
about: Portable station, scanning, link-state and IPv4 services are provided
by Embedded.Network.WiFi. This module adds ESP8266 SoftAP control which can
coexist with station mode.
End Rem
Module ESP8266.Network.WiFi
?esp8266

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Import Embedded.Network.WiFi

Private

Extern "C"
	Function _ESP8266WiFiStartAccessPoint:Int(ssid:Byte Ptr, ssidLength:UInt, password:Byte Ptr, ..
		passwordLength:UInt, authentication:UInt, channel:UInt, maximumConnections:UInt, ..
		hidden:Int) = "bmx_esp8266_wifi_start_access_point"
	Function _ESP8266WiFiStopAccessPoint:Int() = "bmx_esp8266_wifi_stop_access_point"
	Function _ESP8266WiFiAccessPointActive:Int() = "bmx_esp8266_wifi_access_point_active"
	Function _ESP8266WiFiAccessPointClientCount:Int(count:UInt Var) = "bmx_esp8266_wifi_access_point_client_count"
	Function _ESP8266WiFiAccessPointIPv4Address:UInt() = "bmx_esp8266_wifi_access_point_ipv4_address"
	Function _ESP8266WiFiAccessPointIPv4Netmask:UInt() = "bmx_esp8266_wifi_access_point_ipv4_netmask"
	Function _ESP8266WiFiAccessPointIPv4Gateway:UInt() = "bmx_esp8266_wifi_access_point_ipv4_gateway"
End Extern

Function _ESP8266IPv4String:String(value:UInt)
	If value = 0 Then Return ""
	Return String(value & $ff) + "." + String((value Shr 8) & $ff) + "." + ..
		String((value Shr 16) & $ff) + "." + String((value Shr 24) & $ff)
End Function

Public

Function WiFiStartAccessPoint:Int(ssid:String, password:String = "", ..
		authentication:UInt = WiFiAuthenticationWPA2AESPSK, channel:UInt = 1, ..
		maximumConnections:UInt = 4, hidden:Int = False)
	If Not ssid Then Return -5
	Local ssidLength:Size_T, passwordLength:Size_T
	Local ssidBytes:Byte Ptr = ssid.ToUTF8String(ssidLength)
	Local passwordBytes:Byte Ptr
	If password Then passwordBytes = password.ToUTF8String(passwordLength)
	Local result:Int = _ESP8266WiFiStartAccessPoint(ssidBytes, UInt(ssidLength), passwordBytes, ..
		UInt(passwordLength), authentication, channel, maximumConnections, hidden)
	MemFree(ssidBytes)
	If passwordBytes Then MemFree(passwordBytes)
	Return result
End Function

Function WiFiStopAccessPoint:Int()
	Return _ESP8266WiFiStopAccessPoint()
End Function

Function WiFiAccessPointActive:Int()
	Return _ESP8266WiFiAccessPointActive()
End Function

Function WiFiAccessPointClientCount:Int(count:UInt Var)
	Return _ESP8266WiFiAccessPointClientCount(count)
End Function

Function WiFiAccessPointIPv4Address:String()
	Return _ESP8266IPv4String(_ESP8266WiFiAccessPointIPv4Address())
End Function

Function WiFiAccessPointIPv4Netmask:String()
	Return _ESP8266IPv4String(_ESP8266WiFiAccessPointIPv4Netmask())
End Function

Function WiFiAccessPointIPv4Gateway:String()
	Return _ESP8266IPv4String(_ESP8266WiFiAccessPointIPv4Gateway())
End Function
?
