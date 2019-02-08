PORT=${1:-'/dev/ttyACM0'}
esptool.py --trace --chip esp8266 --baud 115200 --port ${PORT} erase_flash

