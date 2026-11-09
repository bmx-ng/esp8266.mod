SuperStrict

Framework BRL.StandardIO
Import ESP8266.System.Device
Import ESP8266.System.Power
Import ESP8266.System.Time

If DeviceResetReason() = DeviceResetReasonDeepSleep Then
	While True
		Print "ESP8266 deep-sleep wake test passed"
		Delay 1000
	Wend
End If

Local passed:Int = PowerSupports(PowerCapabilityIdle | ..
	PowerCapabilitySleepInterrupt | PowerCapabilitySleepTimer)
passed :& Not PowerSupports(PowerCapabilityDormantTimer)
passed :& LowPowerSleep(0) = EPowerResult.InvalidArgument
passed :& DeepSleepForMilliseconds(0) = EPowerResult.InvalidArgument
passed :& DeepSleepSetRFOption(3) = EPowerResult.InvalidArgument
passed :& DeepSleepSetRFOption(ESP8266DeepSleepRFDefault) = EPowerResult.Success

Local before:ULong = MonotonicMilliseconds()
Local sleepResult:EPowerResult = LowPowerSleep(100)
Local elapsed:ULong = MonotonicMilliseconds() - before
passed :& sleepResult = EPowerResult.Success And elapsed >= 90 And elapsed < 1000

Print "ESP8266 light sleep: result=" + Int(sleepResult) + ", elapsed=" + elapsed + " ms"
If Not passed Then RuntimeError "ESP8266 light-sleep checks failed"
Print "ESP8266 light-sleep checks passed; connect D0 to RST for deep-sleep wake"
Delay 60000
Print "ESP8266 entering deep sleep"

' Timed deep-sleep wake on ESP-12 boards requires D0/GPIO16 connected to RST.
DeepSleepForMilliseconds(500)
RuntimeError "ESP8266 deep sleep returned unexpectedly"
