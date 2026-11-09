' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: UTC system clock and absolute alarm services for ESP8266 targets.
about: The clock uses ESP8266 RTOS SDK system time and does not survive a
restart or loss of power. Calendar alarm callbacks perform native bookkeeping
only; managed code consumes their coalesced event count.
End Rem
Module ESP8266.System.Calendar
?esp8266

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Import Pub.Time

Extern "C"
	Function CalendarStart:Int(dateTime:SDateTime Var) = "bmx_esp8266_calendar_start"
	Function CalendarStop() = "bmx_esp8266_calendar_stop"
	Function CalendarSet:Int(dateTime:SDateTime Var) = "bmx_esp8266_calendar_set"
	Function CalendarGet:Int(dateTime:SDateTime Var) = "bmx_esp8266_calendar_get"
	Function CalendarIsRunning:Int() = "bmx_esp8266_calendar_is_running"
	Function CalendarResolutionNanoseconds:ULong() = "bmx_esp8266_calendar_resolution_nanoseconds"

	Rem
	bbdoc: Configures the single one-shot calendar alarm.
	about: ESP8266 cannot use this alarm as a low-power wake source, so a True
	@wakeFromLowPower request is rejected.
	End Rem
	Function CalendarSetAlarm:Int(dateTime:SDateTime Var, wakeFromLowPower:Int = False) = "bmx_esp8266_calendar_set_alarm"
	Function CalendarDisableAlarm() = "bmx_esp8266_calendar_disable_alarm"
	Function PendingCalendarAlarmEvents:UInt() = "bmx_esp8266_calendar_pending_alarm_events"
	Function TakeCalendarAlarmEvents:UInt() = "bmx_esp8266_calendar_take_alarm_events"
End Extern
?
