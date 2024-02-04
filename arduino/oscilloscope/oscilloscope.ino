int nMillis = 0;
int val0 = 0;
int val1 = 0;

void setup() {
  Serial.begin(2000000);
}

// Background info, see: https://www.gammon.com.au/adc
void loop() {
  val0 = analogRead(A0);
  val1 = analogRead(A1);
  if (nMillis < 10000) {
    // Duration of 2 x 10000 analogReads without Serial.write is ~2250 ms
    nMillis += 1;
  } else {
    Serial.println();
    Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    Serial.print("Millis for 10000 analog measurements: ");
    Serial.println(millis());
    nMillis = 0;
  }
  // Duration of 2 x 10000 analogReads including Serial.write is ~ 2500 ms
  Serial.write( 0xff );
  Serial.write( (val0 >> 8) & 0xff );
  Serial.write( val0 & 0xff );
  Serial.write( (val1 >> 8) & 0xff );
  Serial.write( val1 & 0xff );
}
