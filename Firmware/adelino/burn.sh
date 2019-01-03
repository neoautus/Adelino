PORT=${1:-'/dev/ttyACM0'}
$HOME/.arduino15/packages/arduino/tools/avrdude/6.3.0-arduino14/bin/avrdude -C$HOME/.arduino15/packages/arduino/tools/avrdude/6.3.0-arduino14/etc/avrdude.conf -v -patmega32u4 -carduino -P$PORT -b19200 -e -Ulock:w:0x3F:m -Uefuse:w:0xcb:m -Uhfuse:w:0xd8:m -Ulfuse:w:0xff:m 
$HOME/.arduino15/packages/arduino/tools/avrdude/6.3.0-arduino14/bin/avrdude -C$HOME/.arduino15/packages/arduino/tools/avrdude/6.3.0-arduino14/etc/avrdude.conf -v -patmega32u4 -carduino -P$PORT -b19200 -Uflash:w:Adelino.hex:i -Ulock:w:0x2F:m

