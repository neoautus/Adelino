
// Reset ESP-8266 using HWB.
// 
// It uses reset switch status to decide whether it will
// reset ESP or not. To test, press Adelino reset switch. 
// The sketch will blink 3 times before acquiring the reset 
// switch status. If the reset switch is kept pressed, the
// ESP-8266 will be reset by asserting HWB/PE2 low for 10ms.

int rstat ()
{
  // Read the reset switch preserving the port configuration afterwards
  unsigned char save_DDRD = DDRD;
  unsigned char save_PORTD = PORTD;
  cli ();
  DDRD &= ~_BV(5); // PD5 -> input
  PORTD |= _BV(5); // Pull up
  int stat = !(PIND & _BV(5));
  PORTD = save_PORTD;
  DDRD = save_DDRD;
  sei ();
  return (stat);
}

// Set the logic level on CHIP_EN line, which is 
// connected to HWB (PE2) from ATmega32U4.
int espen (int level)
{
  if (level)
  {
    PORTE |= _BV(2); // PE2 => HIGH
  }
  else
  {
    PORTE &= ~_BV(2); // PE2 => LOW
  }

  // We set pin direction only after setting value
  // because if it's not configured yet (hi-z) it 
  // will be pulled up. If we are setting it HIGH
  // and configure direction first, we may get a
  // unintended short LOW pulse. Therefore, 
  // direction is set only after the intended level 
  // is set.
  DDRE |= _BV(2); // PE2 => OUTPUT
}

void setup ()
{
  // GPIO15 is also HSPI_CS, which is connected to SS on
  // AVR SPI; so we MUST ensure GPIO15 LOW for proper boot.
  pinMode (PIN_SPI_SS, OUTPUT);

  // D13 is also used to drive GPIO0, which is the main
  // boot mode selector. GPIO0 HIGH for normal boot, GPIO0
  // LOW to enter ROM bootloader.
  pinMode (LED_BUILTIN, OUTPUT);

  for (int i = 0; i < 3; i++)
  {
    digitalWrite (LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite (LED_BUILTIN, LOW);
    delay(500);
  }

  //===========
  // Reset ESP
  //===========

  if (rstat ())
  {
    // Enter ROM bootloader
    digitalWrite (LED_BUILTIN, LOW); // GPIO0 => LOW
  }
  else
  {
    // Normal boot
    digitalWrite (LED_BUILTIN, HIGH); // GPIO0 => HIGH
  }
  
  digitalWrite (PIN_SPI_SS, LOW); // GPIO15 => LOW
  delay (10);
  espen (LOW); // CHIP_EN => LOW
  delay (50);
  espen (HIGH); // CHIP_EN => HIGH
  delay (250);
}

void loop ()
{
  // put your main code here, to run repeatedly
}
