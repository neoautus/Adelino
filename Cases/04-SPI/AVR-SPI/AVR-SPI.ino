
// Receive data from ESP-8266 via SPI.
//
// The AVR is in SLAVE MODE and each time it receives
// a full string, it is dumped into serial port. It can
// be viewed using Arduino's IDE Serial Monitor.

#include <SPI.h>

char buff [100];
volatile byte index;
volatile bool receivedone;  // use reception complete flag

void setup (void)
{
  Serial.begin (115200);
  SPCR |= bit (SPE);        // Enable SPI
  pinMode (MISO, OUTPUT);   // Make MISO pin as OUTPUT
  index = 0;
  receivedone = false;
  SPI.attachInterrupt ();   // Attach SPI interrupt
}

void loop (void)
{
  if (receivedone)          // Check and print received buffer if any
  {
    Serial.println (buff);
    index = 0;
    receivedone = false;
  }
}

// SPI interrupt routine
ISR (SPI_STC_vect)
{
  uint8_t oldsrg = SREG;
  cli ();
  char c = SPDR;
  if (index < sizeof (buff))
  {
    buff [index++] = c;
    if (c == '\0')          // Check for C end-of-string NUL
    {     
      receivedone = true;
    }
  }
  SREG = oldsrg;
}
