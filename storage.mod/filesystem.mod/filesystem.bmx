' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: BRL.FileSystem routing for ESP8266 persistent flash storage.
about: Import ESP8266.Storage.FATFS to mount the wear-levelled flash volume.
Plain paths and file:: then use it as the default backend; flash:: and fatfs::
always address the mounted volume.
End Rem
Module ESP8266.Storage.FileSystem
?esp8266

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Import BRL.FileSystem
Import "native/esp8266_filesystem.c"

Extern "C"
	Function _ESP8266FSOpen:Byte Ptr(path:String, readable:Int, writeMode:Int) = "bmx_esp8266_filesystem_open"
	Function _ESP8266FSClose:Int(handle:Byte Ptr) = "bmx_esp8266_filesystem_close"
	Function _ESP8266FSRead:Long(handle:Byte Ptr, buffer:Byte Ptr, count:Long) = "bmx_esp8266_filesystem_read"
	Function _ESP8266FSWrite:Long(handle:Byte Ptr, buffer:Byte Ptr, count:Long) = "bmx_esp8266_filesystem_write"
	Function _ESP8266FSPosition:Long(handle:Byte Ptr) = "bmx_esp8266_filesystem_position"
	Function _ESP8266FSSize:Long(handle:Byte Ptr) = "bmx_esp8266_filesystem_size"
	Function _ESP8266FSSeek:Long(handle:Byte Ptr, position:Long, whence:Int) = "bmx_esp8266_filesystem_seek"
	Function _ESP8266FSResize:Int(handle:Byte Ptr, size:Long) = "bmx_esp8266_filesystem_resize"
	Function _ESP8266FSFlush:Int(handle:Byte Ptr) = "bmx_esp8266_filesystem_flush"
	Function _ESP8266FSStat:Int(path:String, fileType:Int Var, size:Long Var, modified:Long Var, created:Long Var, accessed:Long Var, readOnly:Int Var) = "bmx_esp8266_filesystem_stat"
	Function _ESP8266FSSetTime:Int(path:String, time:Long, timeType:Int) = "bmx_esp8266_filesystem_set_time"
	Function _ESP8266FSMkdir:Int(path:String) = "bmx_esp8266_filesystem_mkdir"
	Function _ESP8266FSRemove:Int(path:String) = "bmx_esp8266_filesystem_remove"
	Function _ESP8266FSRename:Int(oldPath:String, newPath:String) = "bmx_esp8266_filesystem_rename"
	Function _ESP8266FSDirectoryOpen:Byte Ptr(path:String) = "bmx_esp8266_filesystem_directory_open"
	Function _ESP8266FSDirectoryNext:String(handle:Byte Ptr) = "bmx_esp8266_filesystem_directory_next"
	Function _ESP8266FSDirectoryClose(handle:Byte Ptr) = "bmx_esp8266_filesystem_directory_close"
End Extern

Type TESP8266FileStream Extends TStream
	Field handle:Byte Ptr
	Field readable:Int
	Field writeMode:Int

	Method Pos:Long() Override
		If Not handle Then Return -1
		Return _ESP8266FSPosition(handle)
	End Method
	Method Size:Long() Override
		If Not handle Then Return 0
		Return _ESP8266FSSize(handle)
	End Method
	Method Seek:Long(position:Long, whence:Int = SEEK_SET_) Override
		If Not handle Then Return -1
		Return _ESP8266FSSeek(handle, position, whence)
	End Method
	Method Flush() Override
		If handle And _ESP8266FSFlush(handle) < 0 Then Throw New TStreamWriteException
	End Method
	Method Close() Override
		If Not handle Then Return
		Local oldHandle:Byte Ptr = handle
		handle = Null
		If _ESP8266FSClose(oldHandle) < 0 Then Throw New TStreamWriteException
	End Method
	Method Read:Long(buffer:Byte Ptr, count:Long) Override
		If Not handle Or Not readable Then Throw New TStreamReadException
		Local result:Long = _ESP8266FSRead(handle, buffer, count)
		If result < 0 Then Throw New TStreamReadException
		Return result
	End Method
	Method Write:Long(buffer:Byte Ptr, count:Long) Override
		If Not handle Or Not writeMode Then Throw New TStreamWriteException
		Local result:Long = _ESP8266FSWrite(handle, buffer, count)
		If result < 0 Then Throw New TStreamWriteException
		Return result
	End Method
	Method SetSize:Int(size:Long) Override
		Return handle And writeMode And _ESP8266FSResize(handle, size) = 0
	End Method

	Function Open:TESP8266FileStream(path:String, readable:Int, writeMode:Int)
		Local nativeHandle:Byte Ptr = _ESP8266FSOpen(path, readable, writeMode)
		If Not nativeHandle Then Return Null
		Local stream:TESP8266FileStream = New TESP8266FileStream
		stream.handle = nativeHandle
		stream.readable = readable
		stream.writeMode = writeMode
		Return stream
	End Function
End Type

Type TESP8266FileSystemBackend Extends TFileSystemBackend
	Field currentDirectory:String = "/"

	Method HandlesProtocol:Int(value:String) Override
		value = value.ToLower()
		Return value = "flash" Or value = "fatfs"
	End Method
	Method OpenPath:TStream(path:String, readable:Int, writeMode:Int) Override
		Return TESP8266FileStream.Open(ResolvePath(path), readable, writeMode)
	End Method
	Method CurrentDirectory:String() Override
		Return currentDirectory
	End Method
	Method ChangeDirectory:Int(path:String) Override
		Local resolved:String = ResolvePath(path)
		Local info:SFileStat
		If Not Stat(resolved, info) Or info.fileType <> FILETYPE_DIR Then Return False
		currentDirectory = resolved
		Return True
	End Method
	Method Stat:Int(path:String, info:SFileStat Var) Override
		Return _ESP8266FSStat(ResolvePath(path), info.fileType, info.size, ..
			info.modifiedTime, info.creationTime, info.accessTime, info.isReadOnly) = 0
	End Method
	Method SetTime(path:String, time:Long, timeType:Int) Override
		_ESP8266FSSetTime(ResolvePath(path), time, timeType)
	End Method
	Method FileMode:Int(path:String) Override
		Local info:SFileStat
		If Not Stat(path, info) Then Return -1
		If info.fileType = FILETYPE_DIR Then Return $1FF
		Return $1B6
	End Method
	Method CreateFile:Int(path:String) Override
		Local stream:TESP8266FileStream = TESP8266FileStream.Open(ResolvePath(path), False, WRITE_MODE_OVERWRITE)
		If Not stream Then Return False
		stream.Close()
		Return True
	End Method
	Method CreateDirectory:Int(path:String) Override
		Return _ESP8266FSMkdir(ResolvePath(path)) = 0
	End Method
	Method DeleteFile:Int(path:String) Override
		Local info:SFileStat
		If Not Stat(path, info) Or info.fileType <> FILETYPE_FILE Then Return False
		Return _ESP8266FSRemove(ResolvePath(path)) = 0
	End Method
	Method DeleteDirectory:Int(path:String) Override
		Local info:SFileStat
		If Not Stat(path, info) Or info.fileType <> FILETYPE_DIR Then Return False
		Return _ESP8266FSRemove(ResolvePath(path)) = 0
	End Method
	Method Rename:Int(oldPath:String, newPath:String) Override
		Return _ESP8266FSRename(ResolvePath(oldPath), ResolvePath(newPath)) = 0
	End Method
	Method OpenDirectory:Byte Ptr(path:String) Override
		Return _ESP8266FSDirectoryOpen(ResolvePath(path))
	End Method
	Method NextDirectoryEntry:String(handle:Byte Ptr) Override
		Return _ESP8266FSDirectoryNext(handle)
	End Method
	Method CloseDirectory(handle:Byte Ptr) Override
		_ESP8266FSDirectoryClose(handle)
	End Method
End Type

Global _esp8266FlashBackend:TESP8266FileSystemBackend = New TESP8266FileSystemBackend
Global _esp8266FlashMounted:Int

Function _ESP8266StorageMountChanged(mounted:Int)
	_esp8266FlashMounted = mounted
	If mounted
		SetDefaultFileSystemBackend(_esp8266FlashBackend)
	Else If DefaultFileSystemBackend() = _esp8266FlashBackend
		SetDefaultFileSystemBackend(Null)
	End If
End Function

Function DefaultStorageVolume:String()
	If DefaultFileSystemBackend() = _esp8266FlashBackend Then Return "flash"
	Return ""
End Function
?
