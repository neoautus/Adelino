AVR_VERSION=1.6.23
AVR_FILENAME=adelino-avr-${AVR_VERSION}.tar.bz2
echo "Filename: ${AVR_FILENAME}"
tar cjf ${AVR_FILENAME} AVR/
AVR_FILESIZE=`stat -c %s ${AVR_FILENAME}`
echo "Filesize: ${AVR_FILESIZE}"
AVR_SHA256=`sha256sum ${AVR_FILENAME} | cut -d' ' -f1`
echo "SHA-256: ${AVR_SHA256}"
sed \
  -e "s#@AVR_VERSION@#${AVR_VERSION}#g" \
  -e "s#@AVR_FILENAME@#${AVR_FILENAME}#g" \
  -e "s#@AVR_FILESIZE@#${AVR_FILESIZE}#g" \
  -e "s#@AVR_SHA256@#${AVR_SHA256}#g" \
package_adelino_index.json.template > package_adelino_index.json
