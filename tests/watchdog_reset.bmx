SuperStrict

Framework BRL.StandardIO
Import ESP8266.System.Watchdog

If WatchdogCausedReboot() Then
	Print "ESP8266 watchdog reset test passed"
	While True
		Delay 1000
	Wend
Else
	Print "Arming ESP8266 watchdog reset test"
	If Not WatchdogEnable(250) Then RuntimeError "Unable to enable watchdog"

	' The application watchdog expires first, then deliberately stops feeding the
	' independent hardware watchdog so that the following boot has a genuine
	' watchdog reset reason.
	While True
		Delay 100
	Wend
End If
