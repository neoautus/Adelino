PORT=${1:-'/dev/ttyACM0'}
$HOME/.arduino15/packages/arduino/tools/avrdude/6.3.0-arduino14/bin/avrdude -C$HOME/.arduino15/packages/arduino/tools/avrdude/6.3.0-arduino14/etc/avrdude.conf -v -patmega32u4 -carduino -P$PORT -b19200 -e -Ulock:w:0xff:m -Ulfuse:w:0x5e:m -Uhfuse:w:0x99:m -Uefuse:w:0xf3:m
$HOME/.arduino15/packages/arduino/tools/avrdude/6.3.0-arduino14/bin/avrdude -C$HOME/.arduino15/packages/arduino/tools/avrdude/6.3.0-arduino14/etc/avrdude.conf -v -patmega32u4 -carduino -P$PORT -b19200 -Uflash:w:ATMega32U4-usbdevice_dfu-1_0_0.hex:i -Ulock:w:0x2c:m

