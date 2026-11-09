SuperStrict

Framework BRL.StandardIO
Import Embedded.System.Calendar

CalendarStop()
If CalendarIsRunning() Then RuntimeError "Calendar initially running"

Local invalid:SDateTime = New SDateTime(2025, 2, 29, 0, 0, 0, 0, True)
If CalendarStart(invalid) Then RuntimeError "Invalid leap date accepted"

Local initial:SDateTime = New SDateTime(2026, 11, 9, 12, 34, 56, 250, True)
If Not CalendarStart(initial) Then RuntimeError "Calendar start failed"
If Not CalendarIsRunning() Then RuntimeError "Calendar did not start"

Local current:SDateTime
If Not CalendarGet(current) Then RuntimeError "Calendar read failed"
If current.year <> 2026 Or current.month <> 11 Or current.day <> 9 Or ..
		current.hour <> 12 Or current.minute <> 34 Or current.second < 56 Then
	RuntimeError "Calendar value mismatch"
End If
If CalendarResolutionNanoseconds() <> 1000 Then RuntimeError "Calendar resolution mismatch"

Local alarm:SDateTime = New SDateTime(2026, 11, 9, 12, 34, 58, 0, True)
If CalendarSetAlarm(alarm, True) Then RuntimeError "Unsupported low-power alarm accepted"
If Not CalendarSetAlarm(alarm) Then RuntimeError "Calendar alarm setup failed"
Delay 2200
If PendingCalendarAlarmEvents() <> 1 Then RuntimeError "Calendar alarm did not fire"
If TakeCalendarAlarmEvents() <> 1 Or PendingCalendarAlarmEvents() <> 0 Then ..
	RuntimeError "Calendar alarm consumption failed"

CalendarStop()
If CalendarIsRunning() Or CalendarGet(current) Then RuntimeError "Calendar stop failed"

While True
	Print "ESP8266 calendar test passed"
	Delay 1000
Wend
