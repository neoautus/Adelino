
// Reset switch status can be read on PD5.
// 
// To test, press Adelino reset switch. The sketch should
// blink 3 times before reading the reset switch status.
// The D13 led will blink 2 times every 2s if the switch 
// was kept pressed after the 3 blinks, of just 1 time if
// the reset switch was released anytime before.

// D13 led is active low on X1, will be active HIGH on Adelino X2
#define LED_ON   digitalWrite (LED_BUILTIN, LOW)
#define LED_OFF  digitalWrite (LED_BUILTIN, HIGH)

volatile int reset_pressed;

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

void setup()
{
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  // Blink 3 times to inform the reset button state will be read
  for (int i = 0; i < 3; i++)
  {
    LED_ON;
    delay (500);
    LED_OFF;
    delay (500);
  }

  // The reset button was kept pressed?
  reset_pressed = rstat ();

  // Ok, wait a little to create some suspense :)
  delay (500);
}

// the loop function runs over and over again forever
void loop () 
{
  // One short blink
  LED_ON;
  delay (100);
  LED_OFF;

  // If reset button was pressed, blink 2 times
  if (reset_pressed)
  {
    // One more short blink
    delay (100);
    LED_ON;
    delay (100);
    LED_OFF;
  }

  // wait 1 second between blinks
  delay (2000);
}
