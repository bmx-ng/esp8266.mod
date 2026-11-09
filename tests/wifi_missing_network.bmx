SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import ESP8266.Network.WiFi

If WiFiInitialize(WiFiCountryUK) <> 0 Then RuntimeError "WiFi initialization failed"

Local result:Int = WiFiConnectWait("BlitzMax-Missing-Network-8266", "", ..
	WiFiAuthenticationOpen, 1, 15000, 0)
If result <> WiFiLinkNoNetwork And result <> WiFiLinkFailed Then
	RuntimeError "Unexpected missing-network result: " + result
End If
If WiFiLinkStatus() <> result Then RuntimeError "WiFi failure state was not retained"
If WiFiIPv4Address() <> "" Then RuntimeError "Missing network unexpectedly acquired an address"
If WiFiDeinitialize() <> 0 Then RuntimeError "WiFi deinitialization failed"

Print "ESP8266 WiFi missing-network test passed: " + result
