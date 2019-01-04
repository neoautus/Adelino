
// Transmit data to AVR via SPI.
//
// The ESP-8266 is in MASTER MODE and transmits a
// string every 1 second. It also blinks ESP LED to
// make it noticeable.

#include <SPI.h>

char buff[] = "The quick brown fox jumped over the lazy dogs";

void setup ()
{
  // initialize digital pin LED_BUILTIN as an output.
  pinMode (LED_BUILTIN, OUTPUT);

  // Start SPI
  SPI.begin ();
}

void loop ()
{
  // turn the LED on (HIGH is the voltage level)
  digitalWrite (LED_BUILTIN, LOW);
 
  // transfer buff data per second
  for (int i=0; i < sizeof (buff); i++)
  {
    SPI.transfer (buff[i]);
  }

  // Turn the LED off by making the voltage LOW.
  // Both LEDs from ESP-12F and Adelino are active LOW.
  delay (50);
  digitalWrite (LED_BUILTIN, HIGH);

  // Don't flood me! :)
  delay (1000);  
}
