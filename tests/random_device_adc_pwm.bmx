SuperStrict

Framework BRL.StandardIO
Import ESP8266.Random
Import ESP8266.System.Device
Import ESP8266.Hardware.ADC
Import ESP8266.Hardware.PWM

Const A0:UInt = 17
Const LED:UInt = 2
Const SECOND_PWM:UInt = 5

Local passed:Int = True

Local randomBytes:Byte[32]
passed :& ESP8266FillRandom(randomBytes, randomBytes.length)
passed :& Not ESP8266FillRandom(Null, 1)
Local anyRandom:Int
For Local index:Int = 0 Until randomBytes.length
	If randomBytes[index] Then anyRandom = True
Next
passed :& anyRandom

Local identifier:String = UniqueDeviceID()
Local identifierBytes:Byte[] = UniqueDeviceIDBytes()
passed :& identifier.length = 12 And identifierBytes.length = 6
passed :& ChipModelName() = "ESP8266" And ChipCoreCount() = 1
passed :& (ChipFeatures() & ChipFeatureWiFi) <> 0
passed :& CPUFrequencyHz() = 80000000 Or CPUFrequencyHz() = 160000000
passed :& NativeFreeHeap() > 0 And NativeMinimumFreeHeap() > 0
Local reason:Int = DeviceResetReason()
passed :& reason >= DeviceResetReasonUnknown And reason <= DeviceResetReasonCPULockup

passed :& ADCIsValidPin(A0) And Not ADCIsValidPin(0)
passed :& ADCResolutionBitsForPin(A0) = 10 And ADCMaximumValueForPin(A0) = 1023
passed :& ADCInitPin(A0) And ADCInitialized()
Local raw:UInt
passed :& ADCReadRaw(A0, raw) And raw <= ADCMaximumValue
passed :& ADCDeinitPin(A0) And Not ADCInitialized()

passed :& PWMIsValidPin(LED) And Not PWMIsValidPin(6)
Local achieved:UInt = PWMInitPin(LED, 500, PWMDutyMaximum / 4, True)
passed :& achieved > 0 And PWMGetPinFrequency(LED) = achieved
passed :& PWMGetPinDuty(LED) = PWMDutyMaximum / 4 And PWMGetPinPolarity(LED)
passed :& PWMActiveChannelCount() = 1
passed :& PWMSetPinDuty(LED, PWMDutyMaximum / 2)
Delay 250
passed :& PWMInitPin(SECOND_PWM, achieved, PWMDutyMaximum / 3) > 0
passed :& PWMActiveChannelCount() = 2
passed :& PWMSetPinFrequency(LED, 2000) = 0
passed :& PWMDeinitPin(SECOND_PWM) And PWMActiveChannelCount() = 1
passed :& PWMSetPinEnabled(LED, False) And Not PWMGetPinEnabled(LED)
passed :& PWMSetPinEnabled(LED, True) And PWMGetPinEnabled(LED)
passed :& PWMSetPinFrequency(LED, 1000) > 0
Delay 250
passed :& PWMDeinitPin(LED) And PWMActiveChannelCount() = 0

If Not passed Then RuntimeError "ESP8266 random/device/ADC/PWM test failed"
Print "ESP8266 random/device/ADC/PWM test passed"
Print "Device: " + identifier + ", reset: " + DeviceResetReasonName(reason)
Print "ADC A0 raw: " + raw + ", native heap: " + NativeFreeHeap()
