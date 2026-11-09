SuperStrict

Framework BRL.StandardIO
Import BRL.Socket
Import ESP8266.Network.WiFi

Local listener:TSocket = CreateTCPSocket()
If Not listener Or Not BindSocket(listener, 0, AF_INET_) Or Not SocketListen(listener, 1) Then ..
	RuntimeError "Listener setup failed"

Local started:UInt = MilliSecs()
Local unexpected:TSocket = SocketAccept(listener, 75)
Local elapsed:UInt = MilliSecs() - started
If unexpected Then RuntimeError "Accept unexpectedly returned a socket"
If elapsed < 60 Or elapsed > 1000 Then ..
	RuntimeError "Accept timeout was outside its expected range: " + elapsed
CloseSocket listener
Print "Empty accept timed out after " + elapsed + " ms"

Local first:TSocket = CreateUDPSocket()
If Not first Or Not BindSocket(first, 0, AF_INET_) Then RuntimeError "First UDP bind failed"
Local second:TSocket = CreateUDPSocket()
If Not second Then RuntimeError "Second UDP socket creation failed"
If BindSocket(second, SocketLocalPort(first), AF_INET_) Then ..
	RuntimeError "Conflicting UDP bind unexpectedly succeeded"
CloseSocket second
CloseSocket first
Print "Conflicting bind was rejected"

Local idle:TSocket = CreateUDPSocket()
If Not idle Or Not BindSocket(idle, 0, AF_INET_) Then RuntimeError "Idle UDP setup failed"
Local buffer:Byte[1]
If idle.TryRecv(buffer, Size_T(buffer.length)) <> -2 Then ..
	RuntimeError "Empty nonblocking receive did not report not-ready"
CloseSocket idle
Print "Empty nonblocking receive reported not-ready"

For Local cycle:Int = 0 Until 40
	Local socket:TSocket = CreateUDPSocket()
	If Not socket Then RuntimeError "Socket allocation failed during reuse cycle " + cycle
	If Not socket.EnableEvents(SocketEventReadable | SocketEventError) Then ..
		RuntimeError "Event registration leaked during reuse cycle " + cycle
	socket.DisableEvents()
	CloseSocket socket
Next
Print "Socket and event resources survived 40 reuse cycles"

If WiFiInitialize(WiFiCountryUK) <> 0 Then RuntimeError "Wi-Fi guard setup failed"
Local active:TSocket = CreateUDPSocket()
If Not active Then RuntimeError "Active-socket guard setup failed"
If WiFiDeinitialize() = 0 Then RuntimeError "Wi-Fi teardown accepted an active socket"
CloseSocket active
If WiFiDeinitialize() <> 0 Then RuntimeError "Wi-Fi teardown failed after socket closure"
Print "Wi-Fi teardown correctly rejected an active socket"
Print "ESP8266 socket failure hardening passed"
