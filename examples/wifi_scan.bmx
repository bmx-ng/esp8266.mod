SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import ESP8266.Network.WiFi

If WiFiInitialize(WiFiCountryUK) <> 0 Then RuntimeError "WiFi initialization failed"
If WiFiStartScan() <> 0 Then RuntimeError "WiFi scan failed to start"

Print "Scanning for WiFi networks..."
Local complete:Int
Local found:Int
While Not complete
	Select WaitEvent()
		Case EVENT_WIFISCANRESULT
			Local network:TWiFiNetwork = TWiFiNetwork(EventExtra())
			found :+ 1
			Print network.ssid + "  channel=" + network.channel + ..
				" rssi=" + network.rssi + " security=" + network.security
		Case EVENT_WIFISCANCOMPLETE
			complete = True
	End Select
Wend

Print "Scan complete; networks=" + found + ", dropped=" + WiFiDroppedEvents()
If WiFiDeinitialize() <> 0 Then RuntimeError "WiFi deinitialization failed"
