SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import ESP8266.Hardware.GPIO
Import ESP8266.Hardware.GPIOEvents
Import Embedded.Runtime.Events

' D1 (GPIO5) must be connected to D2 (GPIO4).
Const OutputPin:UInt = 5
Const InputPin:UInt = 4

Global received:Int
Global eventFlags:UInt
Global eventPin:Int = -1
Global eventTime:ULong

Function CaptureGPIOEvent:Object(hookId:Int, data:Object, context:Object)
	Local event:TEvent = TEvent(data)
	If event And event.id = EVENT_GPIOIRQ Then
		received :+ 1
		eventFlags :| UInt(event.data)
		eventPin = event.mods
		eventTime = GPIOIRQTimeUS(event)
	End If
	Return data
End Function

AddHook EmitEventHook, CaptureGPIOEvent

Local ready:Int = GPIOInit(OutputPin) And GPIOPut(OutputPin, False) And ..
	GPIOSetOutput(OutputPin)
ready :& GPIOInit(InputPin) And GPIOSetInput(InputPin) And GPIODisablePulls(InputPin)

Local source:TGPIOIRQSource
If ready Then source = TGPIOIRQSource.Create(InputPin, ..
	GPIOIRQEdgeRise | GPIOIRQEdgeFall)
ready :& source <> Null

If ready Then
	Delay 20
	ready :& GPIOPut(OutputPin, True)
	Delay 20
	ready :& GPIOPut(OutputPin, False)
	Delay 20
	PollSystem()
End If

Local pending:UInt = GPIOPendingIRQEvents(InputPin)
Local taken:UInt = GPIOTakeIRQEvents(InputPin)
Local expected:UInt = GPIOIRQEdgeRise | GPIOIRQEdgeFall
Local passed:Int = ready And received >= 2 And (eventFlags & expected) = expected And ..
	(pending & expected) = expected And (taken & expected) = expected And ..
	eventPin = Int(InputPin) And eventTime > 0 And EmbeddedDeferredEventDropped() = 0

If source Then source.Close()
If Not passed Then RuntimeError "ESP8266 GPIO event loopback failed: received=" + ..
	received + ", flags=" + eventFlags + ", pending=" + pending
Print "ESP8266 GPIO event loopback passed"
