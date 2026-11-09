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

	' The application deadline retains a one-boot marker and then uses the SDK's
	' safe restart path. The following boot must consume that marker exactly once.
	While True
		Delay 100
	Wend
End If
