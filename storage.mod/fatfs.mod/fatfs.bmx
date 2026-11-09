' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: Wear-levelled FAT filesystem on the ESP8266 internal flash partition.
about: Importing this module mounts the reserved storage partition and installs
it as the default BRL.FileSystem backend. Completely erased storage is formatted
automatically; unrecognised nonblank data is not destroyed automatically.
End Rem
Module ESP8266.Storage.FATFS
?esp8266

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Import ESP8266.Storage.FileSystem

Extern "C"
	Function _MountFATFS:Int(formatBlank:Int) = "bmx_esp8266_fatfs_mount"
	Function _UnmountFATFS:Int() = "bmx_esp8266_fatfs_unmount"
	Function _FormatFATFS:Int() = "bmx_esp8266_fatfs_format"
	Function FATFSIsMounted:Int() = "bmx_esp8266_fatfs_is_mounted"
	Function FATFSLastError:Int() = "bmx_esp8266_fatfs_last_error"
	Function FATFSCapacity:Long() = "bmx_esp8266_fatfs_capacity"
	Function FATFSFree:Long() = "bmx_esp8266_fatfs_free"
End Extern

Rem
bbdoc: Mounts the reserved wear-levelled flash filesystem.
about: When formatBlank is true, a completely erased partition is formatted if
it cannot be mounted. Existing unrecognised data is preserved.
End Rem
Function MountFATFS:Int(formatBlank:Int = True)
	Local result:Int = _MountFATFS(formatBlank) = 0
	_ESP8266StorageMountChanged(result)
	Return result
End Function

Function UnmountFATFS:Int()
	Local result:Int = _UnmountFATFS() = 0
	If result Then _ESP8266StorageMountChanged(False)
	Return result
End Function

Rem
bbdoc: Erases, formats, and mounts the reserved storage partition.
about: This destroys every file in the partition.
End Rem
Function FormatFATFS:Int()
	Local result:Int = _FormatFATFS() = 0
	_ESP8266StorageMountChanged(result)
	Return result
End Function

Global _esp8266FATFSAutoMounted:Int = MountFATFS(True)
?
