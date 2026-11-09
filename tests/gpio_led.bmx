SuperStrict

Framework BRL.StandardIO
Import Embedded.Hardware.GPIO

Const LEDPin:UInt = 2

If Not GPIOInit(LEDPin) Or Not GPIOSetOutput(LEDPin) Then
	RuntimeError "Unable to configure the D1 Mini LED"
End If

For Local cycle:Int = 0 Until 40
	' The ESP-12F D1 Mini LED is active low.
	GPIOPut(LEDPin, False)
	Delay 250
	GPIOPut(LEDPin, True)
	Delay 250
Next

Print "ESP8266 GPIO LED test passed"
