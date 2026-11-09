SuperStrict

Framework BRL.StandardIO
Import ESP8266.Hardware.UART
Import ESP8266.IO.BufferedUART

' Connect D8/GPIO15 (TX) to D7/GPIO13 (RX).
Local passed:Int = UARTControllerCount() = 1
Print "UART test ready; connect D8 to D7 after this boot"
Delay 1000
passed :& UARTConfigurePins(UARTController0, 15, 13)
passed :& UARTUsesSwappedPins()
Local rate:UInt = UARTInit(UARTController0, 115200)
Local syncWrite:Int
Local syncRead:Int
Local bufferedWrite:Long = -1
Local bufferedRead:Long = -1
Local bufferedAvailable:UInt
Local syncMismatches:Int
Local bufferedMismatches:Int
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
	For Local index:Int = 0 Until incoming.length
		incoming[index] = 0
	Next
	bufferedWrite = stream.Write(outgoing, outgoing.length)
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
		bufferedMismatches + " break=" + breakOn + "/" + breakOff + ..
		" deinit=" + deinitialized
	If passed Then
		Print "ESP8266 UART/buffered UART test passed"
	Else
		Print "ESP8266 UART/buffered UART test failed"
	End If
	Delay 1000
Wend
