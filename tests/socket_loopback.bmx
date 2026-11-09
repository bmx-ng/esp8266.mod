SuperStrict

Framework BRL.StandardIO
Import BRL.EventQueue
Import BRL.Socket

Function WaitForSocketEvent:Int(eventId:Int, source:Object, timeout:UInt = 1000)
	Local started:UInt = MilliSecs()
	Repeat
		Local current:Int = PollEvent()
		If current = eventId And EventSource() = source Then Return True
		If current = EVENT_SOCKETERROR And EventSource() = source Then Return False
		Delay 1
	Until MilliSecs() - started >= timeout
	Return False
End Function

Local receiver:TSocket = CreateUDPSocket()
If Not receiver Or Not BindSocket(receiver, 0) Then RuntimeError "UDP bind failed"

Local addresses:TAddrInfo[] = AddrInfo("127.0.0.1", ..
	String(SocketLocalPort(receiver)), AF_INET_)
If Not addresses Or addresses.length = 0 Then RuntimeError "Loopback resolution failed"

Local sender:TSocket = CreateUDPSocket()
If Not sender Or Not ConnectSocket(sender, addresses[0]) Then RuntimeError "UDP connect failed"

Local outgoing:Byte[4]
Local incoming:Byte[4]
outgoing[0] = 66
outgoing[1] = 77
outgoing[2] = 88
If sender.Send(outgoing, Size_T(3)) <> 3 Then RuntimeError "UDP send failed"
If receiver.Recv(incoming, Size_T(3)) <> 3 Then RuntimeError "UDP receive failed"
If incoming[0] <> 66 Or incoming[1] <> 77 Or incoming[2] <> 88 Then ..
	RuntimeError "UDP payload mismatch"

Print "UDP loopback passed on port " + SocketLocalPort(receiver)
CloseSocket sender
CloseSocket receiver

Local listener:TSocket = CreateTCPSocket()
If Not listener Or Not BindSocket(listener, 0, AF_INET_) Then RuntimeError "TCP bind failed"
If Not SocketListen(listener, 1) Then RuntimeError "TCP listen failed"
If Not listener.EnableEvents(SocketEventAccept | SocketEventError) Then ..
	RuntimeError "TCP listener events failed"

Local hints:TAddrInfo = New TAddrInfo(AF_INET_, SOCK_STREAM_)
addresses = AddrInfo("127.0.0.1", String(SocketLocalPort(listener)), hints)
If Not addresses Or addresses.length = 0 Then RuntimeError "TCP loopback resolution failed"

Local client:TSocket = TSocket.Create(addresses[0])
If Not client Or Not ConnectSocket(client, addresses[0]) Then RuntimeError "TCP connect failed"
If Not WaitForSocketEvent(EVENT_SOCKETACCEPT, listener) Then RuntimeError "TCP accept event failed"
Local server:TSocket = SocketAccept(listener, 1000)
If Not server Then RuntimeError "TCP accept failed"
If Not server.EnableEvents(SocketEventReadable | SocketEventClosed | SocketEventError) Then ..
	RuntimeError "TCP server events failed"

For Local index:Int = 0 Until outgoing.length
	outgoing[index] = Byte((index + 1) * 11)
Next
If client.Send(outgoing, Size_T(outgoing.length)) <> outgoing.length Then RuntimeError "TCP send failed"
If Not WaitForSocketEvent(EVENT_SOCKETREADABLE, server) Then RuntimeError "TCP readable event failed"
If SocketReadAvail(server) < 1 Then RuntimeError "TCP available-byte count failed"
If server.Recv(incoming, Size_T(incoming.length)) <> incoming.length Then RuntimeError "TCP receive failed"
For Local index:Int = 0 Until outgoing.length
	If incoming[index] <> outgoing[index] Then RuntimeError "TCP payload mismatch"
Next

Print "TCP loopback passed on port " + SocketLocalPort(listener)
CloseSocket client
If Not WaitForSocketEvent(EVENT_SOCKETCLOSED, server) Then RuntimeError "TCP close event failed"
CloseSocket server
CloseSocket listener
Print "ESP8266 socket loopback passed"
