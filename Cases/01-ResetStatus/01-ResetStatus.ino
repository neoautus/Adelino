
// Reset switch status can be read on PE2. For test
// purposes on prototype 0 it can be read on D7.
// 
// To test, press Adelino reset switch. The sketch should
// blink 3 times before setting the reset switch status on
// the D13 led. If the reset switch is kept pressed, the
// reset switch status will be high.

const int the_led = 10;

void setup() {
  pinMode (7, INPUT_PULLUP);
  pinMode(the_led, OUTPUT);
  pinMode (3, OUTPUT);
  digitalWrite (3, LOW);

  digitalWrite (PIN_SPI_SCK, HIGH); 
  pinMode (PIN_SPI_SCK, OUTPUT);

  for (int i = 0; i < 3; i++)
  {
    digitalWrite(the_led, HIGH);
    delay(500);
    digitalWrite(the_led, LOW);
    delay(500);
  }

  int status = !digitalRead (7);
  digitalWrite (the_led, status);

  delay (5000);

  pinMode (7, OUTPUT);

  digitalWrite (the_led, !status);
  delay (100);
  digitalWrite (the_led, status);

  digitalWrite (3, status);

//  if (status)
//  {
    // Reset ESP
    digitalWrite (PIN_SPI_SCK, LOW);
    delay (10);
    digitalWrite (PIN_SPI_SCK, HIGH);
    delay (50);
//  }
  
  digitalWrite (3, status);
}

void loop() 
{
  // put your main code here, to run repeatedly:
  digitalWrite (7, HIGH);
  //delay (50);
  digitalWrite (7, LOW);
  //delay (50);
}
