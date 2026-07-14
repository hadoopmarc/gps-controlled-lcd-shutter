#include <Servo.h>  // https://docs.arduino.cc/libraries/servo/

Servo myservo;

void setup()
{
  myservo.attach(9);  // pin D9
}

void loop()
{
  myservo.write(0);    // have servo rotate one direction (shut)
  delay(10000);
  myservo.write(180);  // have servo rotate other direction (open)
  delay(10000);
}
