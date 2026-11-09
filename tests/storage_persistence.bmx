SuperStrict

Framework BRL.StandardIO
Import BRL.FileSystem
Import BRL.TextStream
Import ESP8266.Storage.FATFS

If Not FATFSIsMounted() Then RuntimeError "Flash filesystem did not mount: " + FATFSLastError()
If DefaultStorageVolume() <> "flash" Then RuntimeError "Flash filesystem is not the default volume"

If FileType("flash::config") = FILETYPE_NONE And Not CreateDir("flash::config") Then ..
	RuntimeError "Directory creation failed"

Local generation:Int = 0
If FileType("flash::config/generation.txt") = FILETYPE_FILE Then
	generation = Int(LoadText("flash::config/generation.txt").Trim())
End If
generation :+ 1
SaveText(String(generation), "config/generation.txt")
If LoadText("flash::config/generation.txt").Trim() <> String(generation) Then ..
	RuntimeError "Plain-path write did not route to flash"
If LoadText("fatfs::config/generation.txt").Trim() <> String(generation) Then ..
	RuntimeError "FATFS protocol did not route to flash"
If LoadText("file::config/generation.txt").Trim() <> String(generation) Then ..
	RuntimeError "File protocol did not route to flash"

Local stream:TStream = WriteFile("flash::config/seek.bin")
If Not stream Then RuntimeError "Binary stream creation failed"
Local payload:Byte[4]
payload[0] = 11
payload[1] = 22
payload[2] = 33
payload[3] = 44
If stream.Write(payload, payload.length) <> payload.length Then RuntimeError "Binary stream write failed"
If Not stream.SetSize(8) Or stream.Size() <> 8 Then RuntimeError "Binary stream extension failed"
If Not stream.SetSize(payload.length) Or stream.Size() <> payload.length Then ..
	RuntimeError "Binary stream truncation failed"
stream.Close()

stream = ReadFile("config/seek.bin")
If Not stream Or stream.Size() <> payload.length Then RuntimeError "Binary stream size failed"
If stream.Seek(2) <> 2 Then RuntimeError "Binary stream seek failed"
Local value:Byte
If stream.Read(Varptr value, 1) <> 1 Or value <> 33 Then RuntimeError "Binary stream read failed"
stream.Close()

Local entries:String[] = LoadDir("flash::config")
Local found:Int
For Local entry:String = EachIn entries
	If entry = "generation.txt" Then found = True
Next
If Not found Then RuntimeError "Directory enumeration failed"

If FileType("flash::config/renamed.bin") = FILETYPE_FILE Then DeleteFile("flash::config/renamed.bin")
If Not RenameFile("flash::config/seek.bin", "flash::config/renamed.bin") Then ..
	RuntimeError "Rename failed"
If FileType("flash::config/renamed.bin") <> FILETYPE_FILE Then RuntimeError "Renamed file is missing"
If Not DeleteFile("flash::config/renamed.bin") Then RuntimeError "File deletion failed"

SetFileTime("flash::config/generation.txt", 1700000000, FILETIME_MODIFIED)
If FileTime("flash::config/generation.txt", FILETIME_MODIFIED) < 1699999998 Then ..
	RuntimeError "File timestamp update failed"

If Not UnmountFATFS() Then RuntimeError "Flash filesystem unmount failed"
If FATFSIsMounted() Then RuntimeError "Flash filesystem remained mounted"
If Not MountFATFS(False) Then RuntimeError "Flash filesystem remount failed: " + FATFSLastError()
If LoadText("config/generation.txt").Trim() <> String(generation) Then ..
	RuntimeError "Data did not survive an unmount and remount"

Print "ESP8266 flash filesystem passed; generation=" + generation
Print "Flash free: " + FATFSFree() + " / " + FATFSCapacity() + " bytes"
