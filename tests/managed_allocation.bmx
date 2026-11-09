SuperStrict

Framework BRL.StandardIO

Type TManagedValue
	Field number:Int
	Field text:String
End Type

Local checksum:Int
For Local pass:Int = 0 Until 80
	Local values:TManagedValue[] = New TManagedValue[12]
	For Local index:Int = 0 Until values.length
		Local value:TManagedValue = New TManagedValue
		value.number = pass * 100 + index
		value.text = "managed-" + pass + "-" + index
		values[index] = value
		checksum :+ value.number + value.text.length
	Next
Next

Print "ESP8266 managed allocation OK: " + checksum
