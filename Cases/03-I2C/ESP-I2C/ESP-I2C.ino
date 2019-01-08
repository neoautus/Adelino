
// Test I2C from ESP by scanning available devices.
//
// This uses the default Wire library and default config
// (pins GPIO4->SDA and GPIO5->SCL). Scans all available 
// devices plugged into I2C bus and print their addresses, 
// every 5 seconds. 
//
// Notice that Adelino leaves to developer the option to
// pull-up I2C lines either using internal AVR/ESP pull-ups
// (enough for few devices and short connections) or the
// almost standard 4K7 pull-ups with external resistors.
// Adelino I2C is 5V. ESP I2C is 5V tolerant.
//
// This code is part of the excellent I2C article from
// Nick Gammon: http://gammon.com.au/i2c

#include <Wire.h>

void setup () 
{
  // Initialize serial and wait for USB setup
  Serial.begin (115200);

  // Init wire library (default pins GPIO4 and GPIO5)
  Wire.begin ();
}

void loop()
{
  Serial.println ();
  Serial.print ("millis = ");
  Serial.println (millis ());
  Serial.println ("I2C scanner. Scanning ...");
  byte count = 0;
  
  for (byte i = 8; i < 120; i++)
  {
    Wire.beginTransmission (i);
    if (Wire.endTransmission () == 0)
    {
      Serial.print ("Found address: ");
      Serial.print (i, DEC);
      Serial.print (" (0x");
      Serial.print (i, HEX);
      Serial.println (")");
      count++;
      delay (1);  // maybe unneeded?
    } // end of good response
  } // end of for loop
  Serial.println ("Done.");
  Serial.print ("Found ");
  Serial.print (count, DEC);
  Serial.println (" device(s).");

  // Wait a bit so we can plug/unplug things
  delay (5000);
}
