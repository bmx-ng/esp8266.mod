SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import ESP8266.Hardware.UART
Import ESP8266.IO.BufferedUART
Import Embedded.Runtime.Events

Global observedStream:TBufferedUARTStream
Global rxEvents:Int
Global txEvents:Int
Global errorEvents:Int
Global eventSourceMatches:Int = True

Function CaptureUARTEvent:Object(hookId:Int, data:Object, context:Object)
	Local event:TEvent = TEvent(data)
	If event And (event.id = EVENT_STREAMAVAIL Or event.id = EVENT_UARTTXEMPTY Or ..
			event.id = EVENT_UARTERROR) Then
		eventSourceMatches :& event.source = observedStream
		If event.id = EVENT_STREAMAVAIL Then rxEvents :+ 1
		If event.id = EVENT_UARTTXEMPTY Then txEvents :+ 1
		If event.id = EVENT_UARTERROR Then errorEvents :+ 1
	End If
	Return data
End Function

AddHook EmitEventHook, CaptureUARTEvent

' Connect D8/GPIO15 (TX) to D7/GPIO13 (RX).
Local passed:Int = UARTControllerCount() = 1
Print "UART test ready; connect D8 to D7 after this boot"
Delay 1000
passed :& UARTConfigurePins(UARTController0, 15, 13)
passed :& UARTUsesSwappedPins()
Local rate:UInt = UARTInit(UARTController0, 115200)
Local syncWrite:Int
Local syncRead:Int
Local bufferedWrite:Long
Local bufferedRead:Long = -1
Local bufferedAvailable:UInt
Local syncMismatches:Int
Local bufferedMismatches:Int
Local partialWrites:Int
Local pressureAvailable:UInt
Local pressureRead:Long
Local pressureDropped:UInt
Local pressureMismatches:Int
Local breakOn:Int
Local breakOff:Int
Local deinitialized:Int
passed :& rate >= 114000 And rate <= 116000
passed :& UARTSetFormat(UARTController0, 8, 1, UARTParityNone)
passed :& UARTSetFlowControl(UARTController0, False, False)

' GPIO15 is a boot-strapping pin and must not be connected to the idle-high RX
' route during reset. Wait for the loopback to be fitted after startup.
Local probeOut:Byte[1]
Local probeIn:Byte[1]
probeOut[0] = $a5
Local probeWait:Int
Local linked:Int
While passed And Not linked And probeWait < 1200
	passed :& UARTWriteBlocking(UARTController0, probeOut, 1) = 1
	passed :& UARTTXWaitBlocking(UARTController0)
	linked = UARTReadTimeout(UARTController0, probeIn, 1, 100000) = 1 And ..
		probeIn[0] = probeOut[0]
	probeWait :+ 1
Wend
passed :& linked

' Link discovery may have queued more than one probe before the first byte is
' observed. Drain that traffic so it cannot contaminate the measured payload.
Local discarded:Byte[64]
For Local settle:Int = 0 Until 4
	While UARTReadAvailable(UARTController0, discarded, discarded.length) > 0
	Wend
	Delay 2
Next

Local outgoing:Byte[96]
Local incoming:Byte[96]
For Local index:Int = 0 Until outgoing.length
	outgoing[index] = Byte((index * 29 + 7) & $ff)
Next
syncWrite = UARTWriteBlocking(UARTController0, outgoing, outgoing.length)
passed :& syncWrite = outgoing.length
passed :& UARTTXWaitBlocking(UARTController0)
syncRead = UARTReadTimeout(UARTController0, incoming, incoming.length, 500000)
passed :& syncRead = incoming.length
For Local index:Int = 0 Until outgoing.length
	If incoming[index] <> outgoing[index] Then syncMismatches :+ 1
Next
passed :& syncMismatches = 0

Local stream:TBufferedUARTStream = OpenBufferedUARTStream(UARTController0, 128, 128)
passed :& stream <> Null
If stream Then
	observedStream = stream
	For Local index:Int = 0 Until incoming.length
		incoming[index] = 0
	Next
	For Local chunk:Int = 0 Until 3
		Local amount:Long = stream.Write(Varptr outgoing[chunk * 32], 32)
		bufferedWrite :+ amount
		passed :& amount = 32
	Next
	passed :& bufferedWrite = outgoing.length
	stream.Flush()
	Local waited:Int
	While stream.Available() < UInt(outgoing.length) And waited < 1000
		Delay 1
		waited :+ 1
	Wend
	bufferedAvailable = stream.Available()
	bufferedRead = stream.Read(incoming, incoming.length)
	passed :& bufferedRead = incoming.length
	For Local index:Int = 0 Until outgoing.length
		If incoming[index] <> outgoing[index] Then bufferedMismatches :+ 1
	Next
	passed :& bufferedMismatches = 0
	PollSystem()
	passed :& rxEvents >= 1 And txEvents >= 1 And errorEvents = 0 And ..
		eventSourceMatches
	stream.Close()
End If

' A 32-byte stream must expose bounded queues, partial writes, and precise
' receive-drop accounting instead of silently falling back to the SDK's
' larger UART buffer.
stream = OpenBufferedUARTStream(UARTController0, 32, 32)
passed :& stream <> Null
If stream Then
	observedStream = stream
	rxEvents = 0
	txEvents = 0
	For Local index:Int = 0 Until incoming.length
		incoming[index] = 0
	Next
	For Local chunk:Int = 0 Until 3
		Local amount:Long = stream.Write(Varptr outgoing[chunk * 32], 32)
		partialWrites :+ Int(amount)
		passed :& amount = 32
	Next
	stream.Flush()
	Local pressureWait:Int
	While stream.DroppedBytes() < 64 And pressureWait < 1000
		Delay 1
		PollSystem()
		pressureWait :+ 1
	Wend
	pressureAvailable = stream.Available()
	pressureDropped = stream.DroppedBytes()
	pressureRead = stream.Read(incoming, pressureAvailable)
	For Local index:Int = 0 Until Int(pressureRead)
		If incoming[index] <> outgoing[index] Then pressureMismatches :+ 1
	Next
	PollSystem()
	passed :& partialWrites = outgoing.length And pressureAvailable = 32 And ..
		pressureRead = 32 And pressureDropped = 64 And pressureMismatches = 0
	passed :& rxEvents >= 1 And txEvents >= 1 And errorEvents = 0 And ..
		eventSourceMatches And EmbeddedDeferredEventDropped() = 0
	stream.Close()
End If

breakOn = UARTSetBreak(UARTController0, True)
passed :& breakOn
Delay 2
breakOff = UARTSetBreak(UARTController0, False)
passed :& breakOff
deinitialized = UARTDeinit(UARTController0)
passed :& deinitialized
passed :& UARTUseSwappedPins(False)

While True
	Print "UART results: linked=" + linked + " sync=" + syncWrite + "/" + syncRead + ..
		" syncMismatch=" + syncMismatches + " buffered=" + bufferedWrite + "/" + ..
		bufferedAvailable + "/" + bufferedRead + " bufferedMismatch=" + ..
		bufferedMismatches + " events=" + rxEvents + "/" + txEvents + "/" + ..
		errorEvents + " pressure=" + partialWrites + "/" + pressureAvailable + ..
		"/" + pressureRead + "/" + pressureDropped + " pressureMismatch=" + ..
		pressureMismatches + " break=" + breakOn + "/" + breakOff + ..
		" deinit=" + deinitialized
	If passed Then
		Print "ESP8266 UART/buffered UART test passed"
	Else
		Print "ESP8266 UART/buffered UART test failed"
	End If
	Delay 1000
Wend
