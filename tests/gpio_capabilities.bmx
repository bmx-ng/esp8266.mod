SuperStrict

Framework BRL.StandardIO
Import ESP8266.Hardware.GPIO

Local passed:Int = True

passed :& GPIOIsValid(0) And GPIOIsValid(5) And GPIOIsValid(12) And GPIOIsValid(16)
passed :& Not GPIOIsValid(6) And Not GPIOIsValid(11) And Not GPIOIsValid(17)
passed :& GPIOIsPullUpCapable(4) And Not GPIOIsPullDownCapable(4)
passed :& Not GPIOIsPullUpCapable(16) And GPIOIsPullDownCapable(16)
passed :& GPIOIsIRQCapable(4) And Not GPIOIsIRQCapable(16)
passed :& Not GPIOIsDriveStrengthConfigurable(4)
passed :& Not GPIOInit(6)

passed :& GPIOInit(4)
passed :& GPIOSetOutput(4) And GPIOPut(4, True)
passed :& GPIOGetDirection(4) = GPIOOutput And GPIOGetOutput(4)
passed :& GPIOSetInput(4) And GPIOGetDirection(4) = GPIOInput
passed :& GPIOPullUp(4) And GPIOIsPulledUp(4) And Not GPIOIsPulledDown(4)
passed :& Not GPIOPullDown(4)
passed :& GPIODisablePulls(4) And Not GPIOIsPulledUp(4)
passed :& Not GPIOSetDriveStrength(4, GPIODriveStrengthHigh)
passed :& GPIOSetIRQEnabled(4, GPIOIRQEdgeRise, True)
passed :& GPIOSetIRQEnabled(4, GPIOIRQEdgeRise, False)

passed :& GPIOInit(16)
passed :& Not GPIOPullUp(16)
passed :& GPIOPullDown(16) And GPIOIsPulledDown(16)
passed :& Not GPIOSetIRQEnabled(16, GPIOIRQEdgeRise, True)

If Not passed Then RuntimeError "ESP8266 GPIO capability test failed"
Print "ESP8266 GPIO capability test passed"
