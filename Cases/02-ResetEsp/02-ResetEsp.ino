
// Reset ESP-8266 using SCK.
// 
// It uses reset switch status to decide whether it will
// reset ESP or not. To test, press Adelino reset switch. 
// The sketch will blink 3 times before acquiring the reset 
// switch status. If the reset switch is kept pressed, the
// ESP-8266 will be reset by asserting SCK low for 10ms.

void setup() {
  pinMode (7, INPUT);
  pinMode (LED_BUILTIN, OUTPUT);

  // Notice RST already have a weak 12K pull-up on
  // ESP-12F. However we need to pull SCK high here
  // to avoid unintended reset when we set SCK as 
  // output (SCK as output takes it away from high-z).
  //
  digitalWrite (PIN_SPI_SCK, HIGH); 
  pinMode (PIN_SPI_SCK, OUTPUT);

  for (int i = 0; i < 3; i++)
  {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
  }

  // Reset switch is active-low
  if (!digitalRead (7))
  {
    digitalWrite (LED_BUILTIN, HIGH);

    // Reset ESP
    digitalWrite (PIN_SPI_SCK, LOW);
    delay (10);
    digitalWrite (PIN_SPI_SCK, HIGH);
  }
}

void loop() {
  // put your main code here, to run repeatedly:

}
