SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import ESP8266.Network.WiFi

Const TestSSID:String = "BlitzMax-ESP8266-Test"
Const TestPassword:String = "blitzmax8266"

If WiFiStartAccessPoint(TestSSID, TestPassword) = 0 Then
	RuntimeError "SoftAP started before WiFi initialization"
End If
If WiFiInitialize(WiFiCountryUK) <> 0 Then RuntimeError "WiFi initialization failed"

If WiFiStartAccessPoint(TestSSID, "password", WiFiAuthenticationOpen) = 0 Then
	RuntimeError "Open SoftAP accepted a password"
End If
If WiFiStartAccessPoint(TestSSID, "short", WiFiAuthenticationWPA2AESPSK) = 0 Then
	RuntimeError "Secured SoftAP accepted a short password"
End If
If WiFiStartAccessPoint(TestSSID, TestPassword, WiFiAuthenticationWPA2AESPSK, 6, 5) = 0 Then
	RuntimeError "SoftAP accepted more than four clients"
End If

Local result:Int = WiFiStartAccessPoint(TestSSID, TestPassword, ..
	WiFiAuthenticationWPA2AESPSK, 6, 4)
If result <> 0 Then RuntimeError "SoftAP failed to start: " + result
If Not WiFiAccessPointActive() Then RuntimeError "SoftAP did not become active"

Local address:String = WiFiAccessPointIPv4Address()
Local netmask:String = WiFiAccessPointIPv4Netmask()
Local gateway:String = WiFiAccessPointIPv4Gateway()
If Not address Or Not netmask Or Not gateway Then
	RuntimeError "SoftAP address information is incomplete"
End If

Local clients:UInt
If WiFiAccessPointClientCount(clients) <> 0 Then
	RuntimeError "SoftAP client count failed"
End If

' APSTA mode must leave the station interface usable.
If WiFiStartScan() <> 0 Then RuntimeError "Station scan failed while SoftAP was active"
Local scanComplete:Int
Local networks:Int
Local scanStarted:UInt = MilliSecs()
While Not scanComplete And MilliSecs() - scanStarted < 15000
	Select PollEvent()
		Case EVENT_WIFISCANRESULT
			networks :+ 1
		Case EVENT_WIFISCANCOMPLETE
			scanComplete = True
	End Select
	Delay 10
Wend
If Not scanComplete Then RuntimeError "Station scan timed out while SoftAP was active"
If Not WiFiAccessPointActive() Then RuntimeError "Station scan stopped the SoftAP"

Print "ESP8266 SoftAP active: ssid=" + TestSSID + " address=" + address + ..
	" netmask=" + netmask + " gateway=" + gateway + " clients=" + clients + ..
	" scannedNetworks=" + networks
Print "SoftAP will remain visible for 30 seconds"
Local started:UInt = MilliSecs()
While MilliSecs() - started < 30000
	PollSystem()
	Delay 10
Wend

If WiFiStopAccessPoint() <> 0 Then RuntimeError "SoftAP failed to stop"
If WiFiAccessPointActive() Then RuntimeError "SoftAP remained active after stop"
If WiFiAccessPointClientCount(clients) <> 0 Or clients <> 0 Then
	RuntimeError "Inactive SoftAP reported clients"
End If
If WiFiAccessPointIPv4Address() <> "" Then
	RuntimeError "Inactive SoftAP reported an address"
End If
If WiFiDeinitialize() <> 0 Then RuntimeError "WiFi deinitialization failed"

While True
	Print "ESP8266 SoftAP test passed"
	Delay 1000
Wend
