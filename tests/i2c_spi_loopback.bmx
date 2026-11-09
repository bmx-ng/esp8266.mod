SuperStrict

Framework BRL.StandardIO
Import Embedded.Hardware.I2C
Import Embedded.Hardware.SPI

Local passed:Int = True

' The SDK's software I2C master can be exercised without a peripheral: a
' missing address must fail cleanly while lifecycle operations still work.
Local i2c:Int = I2CDefaultController()
passed :& i2c = 0
passed :& I2CConfigurePins(i2c, I2CDefaultSDAPin(), I2CDefaultSCLPin(), True)
passed :& I2CInit(i2c, I2CSpeedStandard) = I2CSpeedStandard
Local probe:Byte
passed :& I2CWriteTimeout(i2c, $7f, Varptr probe, 1, 5000) < 0
passed :& I2CDeinit(i2c)
Print "I2C stage: " + passed

' Connect D7 (MOSI/GPIO13) to D6 (MISO/GPIO12).
Local spi:Int = SPIDefaultController()
passed :& spi = 0
passed :& SPIConfigurePins(spi, SPIDefaultRXPin(), SPIDefaultTXPin(), SPIDefaultClockPin())
passed :& SPIInit(spi, 10000000) = 10000000
passed :& SPISetFormat(spi, 8, SPIClockPolarity0, SPIClockPhase0, SPIBitOrderMSBFirst)

Local outgoing:Byte[96]
Local incoming:Byte[96]
For Local index:Int = 0 Until outgoing.length
	outgoing[index] = Byte((index * 37 + 11) & $ff)
Next
passed :& SPIWriteReadBlocking(spi, outgoing, incoming, outgoing.length) = outgoing.length
For Local index:Int = 0 Until outgoing.length
	If incoming[index] <> outgoing[index] Then
		Print "SPI8 mismatch at " + index + ": " + UInt(incoming[index]) + " <> " + UInt(outgoing[index])
		passed = False
		Exit
	End If
Next
Print "SPI8 stage: " + passed

passed :& SPISetFormat(spi, 16, SPIClockPolarity0, SPIClockPhase0, SPIBitOrderMSBFirst)
Local outgoing16:Short[40]
Local incoming16:Short[40]
For Local index:Int = 0 Until outgoing16.length
	outgoing16[index] = Short((index * 1009 + $1234) & $ffff)
Next
passed :& SPIWrite16Read16Blocking(spi, outgoing16, incoming16, outgoing16.length) = outgoing16.length
For Local index:Int = 0 Until outgoing16.length
	If incoming16[index] <> outgoing16[index] Then
		Print "SPI16 mismatch at " + index + ": " + UInt(incoming16[index]) + " <> " + UInt(outgoing16[index])
		passed = False
		Exit
	End If
Next
Print "SPI16 stage: " + passed
passed :& SPIDeinit(spi)

If passed Then
	Print "ESP8266 I2C/SPI test passed"
Else
	Print "ESP8266 I2C/SPI test failed"
End If
