PORT=${1:-'/dev/ttyACM0'}
NODEMCU=$(find . -name 'nodemcu*float.bin')
esptool.py --trace --chip esp8266 --baud 115200 --port ${PORT} write_flash -fm dio 0x00000 ${NODEMCU}

