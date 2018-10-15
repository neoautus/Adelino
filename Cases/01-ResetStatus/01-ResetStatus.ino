
// Reset switch status can be read on PE2. For test
// purposes on prototype 0 it can be read on D7.
// 
// To test, press Adelino reset switch. The sketch should
// blink 3 times before setting the reset switch status on
// the D13 led. If the reset switch is kept pressed, the
// reset switch status will be high.

void setup() {
  pinMode (7, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  for (int i = 0; i < 3; i++)
  {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
  }
  
  digitalWrite (LED_BUILTIN, !digitalRead (7));
}

void loop() {
  // put your main code here, to run repeatedly:

}
