#
# AVR side
#
echo "*** AVR ***"
AVR_VERSION=1.6.23
AVR_FILENAME=adelino-avr-${AVR_VERSION}.tar.bz2
echo "Filename: ${AVR_FILENAME}"
tar cjf ${AVR_FILENAME} AVR/
AVR_FILESIZE=`stat -c %s ${AVR_FILENAME}`
echo "Filesize: ${AVR_FILESIZE}"
AVR_SHA256=`sha256sum ${AVR_FILENAME} | cut -d' ' -f1`
echo "SHA-256: ${AVR_SHA256}"
#
# ESP side
#
echo "*** ESP ***"
ESP_VERSION=2.5.0
ESP_FILENAME=adelino-esp8266-${ESP_VERSION}.tar.bz2
echo "Filename: ${ESP_FILENAME}"
tar cjf ${ESP_FILENAME} ESP/
ESP_FILESIZE=`stat -c %s ${ESP_FILENAME}`
echo "Filesize: ${ESP_FILESIZE}"
ESP_SHA256=`sha256sum ${ESP_FILENAME} | cut -d' ' -f1`
echo "SHA-256: ${ESP_SHA256}"
#
# Patch package_adelino_index.json for Board Manager
#
sed \
  -e "s#@AVR_VERSION@#${AVR_VERSION}#g" \
  -e "s#@AVR_FILENAME@#${AVR_FILENAME}#g" \
  -e "s#@AVR_FILESIZE@#${AVR_FILESIZE}#g" \
  -e "s#@AVR_SHA256@#${AVR_SHA256}#g" \
  -e "s#@ESP_VERSION@#${ESP_VERSION}#g" \
  -e "s#@ESP_FILENAME@#${ESP_FILENAME}#g" \
  -e "s#@ESP_FILESIZE@#${ESP_FILESIZE}#g" \
  -e "s#@ESP_SHA256@#${ESP_SHA256}#g" \
package_adelino_index.json.template > package_adelino_index.json
#
echo "Finish."
